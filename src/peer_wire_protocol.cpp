#include "../include/peer_wire_protocol.hpp"
#include "../include/peer_connection.hpp"
#include <cstring>
#include <algorithm>
#include <random>

// Helper function to close sockets cross-platform
void closeSocket(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

PeerWireProtocol::PeerWireProtocol() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
}

PeerWireProtocol::~PeerWireProtocol() {
#ifdef _WIN32
    WSACleanup();
#endif
    std::lock_guard<std::mutex> lock(peerMutex);
    for (auto& pair : peers) {
        closeSocket(pair.first);
    }
}

void PeerWireProtocol::connectToPeer(const std::string& peerIP, int peerPort) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peerPort);
    inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);

    if (connect(sock, (sockaddr*)&peerAddr, sizeof(peerAddr))) {
        closeSocket(sock);
        throw std::runtime_error("Connection failed to " + peerIP);
    }

    {
        std::lock_guard<std::mutex> lock(peerMutex);
        auto conn = std::make_shared<PeerConnection>();
        conn->socket = sock;
        peers[sock] = conn;
    }

    std::cout << "Sending Handshake" << '\n';
    sendHandshake(sock);
    
    // Start I/O threads
    std::thread([this, sock]() {
        handlePeerInput(sock);
    }).detach();
    std::thread([this, sock]() {
        handlePeerOutput(sock);
    }).detach();
}

////////////////////HERE//////////////////////////////////////////////////////////////////////////////
void PeerWireProtocol::sendHandshake(int peerSocket) {
    std::vector<uint8_t> handshake;
    handshake.reserve(68);
    
    std::cout << "Inside sendHandshake" << '\n';
    // Protocol length
    handshake.push_back(HANDSHAKE_PROTOCOL_LEN);
    
    // Protocol string
    handshake.insert(handshake.end(), 
                    HANDSHAKE_PROTOCOL_STR, 
                    HANDSHAKE_PROTOCOL_STR + HANDSHAKE_PROTOCOL_LEN);
    
    // Reserved bytes
    handshake.insert(handshake.end(), 8, 0);

    std::array<uint8_t, 20> infoHash = getInfoHash();
    
    // Info Hash (20 bytes) - SHA-1 hash of the torrent's metadata
    handshake.insert(handshake.end(), infoHash.begin(), infoHash.end());
    
    // Peer ID (20 bytes) - Get the unique ID from DHT
    const DHT::NodeID& peerId = dht_instance->getMyNodeId();
    handshake.insert(handshake.end(), peerId.begin(), peerId.end());

    // Send the handshake
    int sent = send(peerSocket, reinterpret_cast<const char *>(handshake.data()), handshake.size(), 0);

    if (sent != static_cast<int>(handshake.size())) {
        std::cerr << "Failed to send full handshake to peer socket: " << peerSocket << std::endl;
    } else {
        std::cout << "Sent handshake to peer socket: " << peerSocket << std::endl;
    }

    // Wait for handshake response (blocking)
    uint8_t response[68];
    int received = recv(peerSocket, reinterpret_cast<char *>(response), sizeof(response), 0);
    if (received <= 0) {
        std::cerr << "Peer did not respond to handshake." << std::endl;
        return;
    }

    // Successfully received handshake response
    std::cout << "Received handshake from peer!" << std::endl;
}

void PeerWireProtocol::handleHandshake(int peerSocket) {
    uint8_t buffer[68];
    int received = recv(peerSocket, reinterpret_cast<char *>(buffer), sizeof(buffer), 0);
    
    if (received != 68 || 
        buffer[0] != HANDSHAKE_PROTOCOL_LEN ||
        memcmp(buffer + 1, HANDSHAKE_PROTOCOL_STR, HANDSHAKE_PROTOCOL_LEN) != 0) {
        closeSocket(peerSocket);
        throw std::runtime_error("Invalid handshake received");
    }

    std::lock_guard<std::mutex> lock(peerMutex);
    auto& conn = peers[peerSocket];
    memcpy(conn->info_hash.data(), buffer + 28, 20);
    memcpy(conn->peer_id.data(), buffer + 48, 20);
}

void PeerWireProtocol::sendBitfield(int peerSocket, const std::vector<bool>& bitfield) {
    std::vector<uint8_t> message;
    const size_t byteCount = (bitfield.size() + 7) / 8;
    message.reserve(5 + byteCount);
    
    // Message header
    uint32_t length = htonl(1 + byteCount);
    message.insert(message.end(), 
                 reinterpret_cast<uint8_t*>(&length),
                 reinterpret_cast<uint8_t*>(&length) + 4);
    message.push_back(5);  // Bitfield message ID
    
    // Pack bits into bytes
    for (size_t i = 0; i < byteCount; ++i) {
        uint8_t byte = 0;
        for (int j = 0; j < 8; ++j) {
            size_t index = i * 8 + j;
            if (index < bitfield.size() && bitfield[index]) {
                byte |= (1 << (7 - j));
            }
        }
        message.push_back(byte);
    }

    send(peerSocket, reinterpret_cast<char *>(message.data()), message.size(), 0);
}

void PeerWireProtocol::handleBitfield(int peerSocket, const std::vector<bool>& bitfield) {
    std::lock_guard<std::mutex> lock(peerMutex);
    if (peers.find(peerSocket) != peers.end()) {
        peers[peerSocket]->bitfield = bitfield;
    }
}

void PeerWireProtocol::sendRequest(int peerSocket, int pieceIndex, int blockOffset, int blockSize) {
    std::vector<uint8_t> message(17);
    uint32_t networkLength = htonl(13);
    uint8_t messageId = 6;
    
    memcpy(message.data(), &networkLength, 4);
    message[4] = messageId;
    
    uint32_t networkIndex = htonl(pieceIndex);
    memcpy(message.data() + 5, &networkIndex, 4);
    
    uint32_t networkOffset = htonl(blockOffset);
    memcpy(message.data() + 9, &networkOffset, 4);
    
    uint32_t networkSize = htonl(blockSize);
    memcpy(message.data() + 13, &networkSize, 4);

    {
        std::lock_guard<std::mutex> lock(peerMutex);
        if (peers.find(peerSocket) != peers.end()) {
            peers[peerSocket]->append_to_output(message);
        }
    }
}
////////////////////HERE//////////////////////////////////////////////////////////////////////////////
void PeerWireProtocol::handleRequest(int peerSocket, int pieceIndex, int blockOffset, int blockSize) {
    // Implementation would depend on your piece management system
    // Here we just forward valid requests to the message queue
    std::vector<uint8_t> request(12);
    uint32_t netPiece = htonl(pieceIndex);
    uint32_t netOffset = htonl(blockOffset);
    uint32_t netSize = htonl(blockSize);
    
    memcpy(request.data(), &netPiece, 4);
    memcpy(request.data() + 4, &netOffset, 4);
    memcpy(request.data() + 8, &netSize, 4);

    std::lock_guard<std::mutex> lock(peerMutex);
    if (peers.find(peerSocket) != peers.end()) {
        peers[peerSocket]->message_queue.push_back(request);
    }
}

void PeerWireProtocol::sendPiece(int peerSocket, int pieceIndex, int blockOffset, 
                               const std::vector<uint8_t>& blockData) {
    const size_t messageSize = 4 + 1 + 4 + 4 + blockData.size();
    std::vector<uint8_t> message(messageSize);
    
    uint32_t length = htonl(9 + blockData.size());
    uint8_t messageId = 7;
    
    memcpy(message.data(), &length, 4);
    message[4] = messageId;
    
    uint32_t netIndex = htonl(pieceIndex);
    memcpy(message.data() + 5, &netIndex, 4);
    
    uint32_t netOffset = htonl(blockOffset);
    memcpy(message.data() + 9, &netOffset, 4);
    
    memcpy(message.data() + 13, blockData.data(), blockData.size());

    {
        std::lock_guard<std::mutex> lock(peerMutex);
        if (peers.find(peerSocket) != peers.end()) {
            peers[peerSocket]->append_to_output(message);
        }
    }
}

////////////////////HERE//////////////////////////////////////////////////////////////////////////////
void PeerWireProtocol::handlePiece(int peerSocket, int pieceIndex, int blockOffset,
                                 const std::vector<uint8_t>& blockData) {
    // Implementation would depend on your storage system
    std::cout << "Received piece " << pieceIndex
              << " block " << blockOffset
              << " (" << blockData.size() << " bytes)\n";
}

void PeerWireProtocol::manageChoking() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        std::lock_guard<std::mutex> lock(peerMutex);
        std::vector<std::shared_ptr<PeerConnection>> candidates;
        
        // Collect interested peers
        for (auto& pair : peers) {
            auto& conn = pair.second;
            if (conn->peer_interested && !conn->choked_by_us) {
                candidates.push_back(conn);
            }
        }
        
        // Sort by download rate
        std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) {
                return a->download_rate > b->download_rate;
            });
        
        // Unchoke top 4 peers
        int unchoked = 0;
        for (auto& conn : candidates) {
            if (unchoked >= 4) break;
            if (conn->choked_by_us) {
                conn->send_unchoke();
                unchoked++;
            }
        }
    }
}

void PeerWireProtocol::optimisticUnchoke() {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        std::lock_guard<std::mutex> lock(peerMutex);
        std::vector<int> candidates;
        
        // Find choked peers
        for (auto& pair : peers) {
            if (pair.second->choked_by_us) {
                candidates.push_back(pair.first);
            }
        }
        
        if (!candidates.empty()) {
            std::uniform_int_distribution<> dist(0, candidates.size()-1);
            int selected = candidates[dist(gen)];
            peers[selected]->send_unchoke();
        }
    }
}

void PeerWireProtocol::run() {
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        throw std::runtime_error("Listener socket creation failed");
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(6881);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr))) {
        closeSocket(listenSocket);
        throw std::runtime_error("Bind failed");
    }

    listen(listenSocket, 5);

    // Start choking management threads
    std::thread([this]() { manageChoking(); }).detach();
    std::thread([this]() { optimisticUnchoke(); }).detach();

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(listenSocket, 
                                (sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) continue;

        std::thread([this, clientSocket]() {
            try {
                handleHandshake(clientSocket);
                {
                    std::lock_guard<std::mutex> lock(peerMutex);
                    auto conn = std::make_shared<PeerConnection>();
                    conn->socket = clientSocket;
                    peers[clientSocket] = conn;
                }
                handlePeerInput(clientSocket);
            } catch (...) {
                closeSocket(clientSocket);
            }
        }).detach();
    }
}

// Private helper methods
void PeerWireProtocol::handlePeerInput(int sock) {
    std::vector<uint8_t> buffer(4096);
    while (true) {
        ssize_t received = recv(sock, reinterpret_cast<char *>(buffer.data()), buffer.size(), 0);
        if (received <= 0) break;

        {
            std::lock_guard<std::mutex> lock(peerMutex);
            if (peers.find(sock) == peers.end()) continue;
            
            auto& conn = peers[sock];
            conn->input_buffer.insert(conn->input_buffer.end(),
                                    buffer.begin(),
                                    buffer.begin() + received);
            conn->process_input_buffer();
            conn->update_rate_counters(received, 0);
        }
    }
    
    // Cleanup on disconnect
    std::lock_guard<std::mutex> lock(peerMutex);
    closeSocket(sock);
    peers.erase(sock);
}

void PeerWireProtocol::handlePeerOutput(int sock) {
    while (true) {
        std::vector<uint8_t> data;
        {
            std::lock_guard<std::mutex> lock(peerMutex);
            if (peers.find(sock) == peers.end()) break;
            
            auto& conn = peers[sock];
            if (!conn->output_buffer.empty()) {
                data.swap(conn->output_buffer);
            }
        }

        if (!data.empty()) {
            ssize_t sent = send(sock, reinterpret_cast<char *>(data.data()), data.size(), 0);
            if (sent <= 0) break;
            
            std::lock_guard<std::mutex> lock(peerMutex);
            if (peers.find(sock) != peers.end()) {
                peers[sock]->update_rate_counters(0, sent);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup on disconnect
    std::lock_guard<std::mutex> lock(peerMutex);
    closeSocket(sock);
    peers.erase(sock);
}