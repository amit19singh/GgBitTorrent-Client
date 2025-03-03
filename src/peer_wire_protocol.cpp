#include "../include/peer_wire_protocol.hpp"
#include "../include/peer_connection.hpp"
#include "../include/torrent_file_parser.hpp"
#include <cstring>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>
#include <array>

// Helper function to close sockets cross-platform
void closeSocket(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

PeerWireProtocol::PeerWireProtocol(const std::string& torrentFilePath) 
    : torrentFileParser(torrentFilePath) {
        std::cout << "Initializing DHT Bootstrap in PeerWireProtocol..." << std::endl;
    
        // Generate a random node ID
        DHT::NodeID my_node_id = DHT::DHTBootstrap::generate_random_node_id();
    
        // Initialize DHT Bootstrap
        dht_instance = new DHT::DHTBootstrap(my_node_id);
        dht_instance->add_bootstrap_node("67.215.246.10", 6881); // Add a known bootstrap node
    
        std::cout << "DHT Bootstrap initialized successfully." << std::endl;
    try {
        torrentFile = torrentFileParser.parse();  // Parse the torrent file
        infoHash = torrentFile.infoHash;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing torrent file: " << e.what() << std::endl;
        throw;  // Rethrow exception
    }
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


void PeerWireProtocol::sendHandshake(int peerSocket) {
    std::vector<uint8_t> handshake;
    handshake.reserve(68);
    
    std::cout << "Inside sendHandshake" << '\n';

    // Protocol length
    handshake.push_back(HANDSHAKE_PROTOCOL_LEN);

    // Protocol string
    handshake.insert(handshake.end(), HANDSHAKE_PROTOCOL_STR, HANDSHAKE_PROTOCOL_STR + HANDSHAKE_PROTOCOL_LEN);

    // Reserved bytes (8 bytes, all zero)
    handshake.insert(handshake.end(), 8, 0);

    // Info Hash (SHA-1 hash of the torrent's metadata)
    std::array<uint8_t, 20> infoHash = getInfoHash();

    // // ////////////////////////////////////////
    // std::cout << "InfoHASH: ";
    // for (uint8_t byte : infoHash) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    // }
    // std::cout << std::dec << std::endl;
    // // ////////////////////////////////////////

    // handshake.insert(handshake.end(), infoHash.begin(), infoHash.end());

    // // Peer ID (our DHT Node ID)
    // const DHT::NodeID& peerId = dht_instance->getMyNodeId();
    // handshake.insert(handshake.end(), peerId.begin(), peerId.end());

    // // Send the handshake
    // int sent = send(peerSocket, reinterpret_cast<const char*>(handshake.data()), handshake.size(), 0);
    // if (sent != static_cast<int>(handshake.size())) {
    //     std::cerr << "Failed to send full handshake to peer socket: " << peerSocket << '\n';
    //     return;
    // }
    // std::cout << "Sent handshake to peer socket: " << peerSocket << '\n';

    // // Receive handshake response (68 bytes)
    // uint8_t response[68];
    // int received = recv(peerSocket, reinterpret_cast<char*>(response), sizeof(response), 0);
    // if (received != 68) {
    //     std::cerr << "Peer did not respond with a valid handshake. Received " << received << " bytes." << '\n';
    //     return;
    // }
    // std::cout << "Received handshake from peer!" << '\n';

    // // Start reading the next message (Bitfield, Choke, Unchoke, etc.)
    // std::vector<uint8_t> buffer(4096);  // Allocate buffer for message
    // int bytes_read = recv(peerSocket, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);

    // std::cout << "Trying to read post-handshake message" << '\n';

    std::cout << "InfoHASH (raw bytes): ";
    for (uint8_t byte : infoHash) {
        std::cout << static_cast<int>(byte) << " ";
    }
    std::cout << '\n';

    // Convert infoHash to hex string
    std::ostringstream infoHashHex;
    infoHashHex << std::hex << std::setw(2) << std::setfill('0');
    for (uint8_t byte : infoHash) {
        infoHashHex << static_cast<int>(byte);
    }
    std::cout << "InfoHASH (Hex): " << infoHashHex.str() << '\n';

    // Log before inserting into handshake
    std::cout << "Inserting infoHash into handshake..." << '\n';
    handshake.insert(handshake.end(), infoHash.begin(), infoHash.end());
    std::cout << "Inserted infoHash into handshake." << '\n';

    // Peer ID (our DHT Node ID)
    if (!dht_instance) {
        std::cerr << "Error: DHT instance is null!" << '\n';
        return;
    }
    
    const DHT::NodeID& peerId = dht_instance->getMyNodeId();
    std::cout << "Peer ID Retrieved: ";
    for (uint8_t byte : peerId) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << '\n';

    std::cout << "Retrieved peer ID from DHT instance." << '\n';
    
    handshake.insert(handshake.end(), peerId.begin(), peerId.end());
    std::cout << "Inserted peer ID into handshake." << '\n';

    // Debug: Check handshake size before sending
    std::cout << "Final handshake size: " << handshake.size() << " bytes\n";
    std::cout << "Final handshake contents (hex): ";
    for (uint8_t byte : handshake) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << '\n';

    // Send the handshake
    std::cout << "Sending handshake to peer...\n";
    int sent = send(peerSocket, reinterpret_cast<const char*>(handshake.data()), handshake.size(), 0);
    if (sent != static_cast<int>(handshake.size())) {
        std::cerr << "Failed to send full handshake to peer socket: " << peerSocket << '\n';
        std::cerr << "Send returned: " << sent << " (Expected: " << handshake.size() << ")\n";
        std::cerr << "Send error: " << strerror(errno) << '\n';
        return;
    }
    std::cout << "Sent handshake to peer socket: " << peerSocket << '\n';

    // Receive handshake response (68 bytes)
    uint8_t response[68];
    std::cout << "Waiting for handshake response from peer...\n";
    int received = recv(peerSocket, reinterpret_cast<char*>(response), sizeof(response), 0);
    if (received != 68) {
        std::cerr << "Peer did not respond with a valid handshake. Received " << received << " bytes.\n";
        std::cerr << "Recv error: " << strerror(errno) << '\n';
        return;
    }
    std::cout << "Received handshake from peer!\n";

    // Start reading the next message (Bitfield, Choke, Unchoke, etc.)
    std::vector<uint8_t> buffer(4096);  // Allocate buffer for message
    std::cout << "Trying to read post-handshake message...\n";
    int bytes_read = recv(peerSocket, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
    if (bytes_read <= 0) {
        std::cerr << "No post-handshake message received. Bytes read: " << bytes_read << '\n';
        std::cerr << "Recv error: " << strerror(errno) << '\n';
        return;
    }
    std::cout << "Received " << bytes_read << " bytes after handshake.\n";

    // Print raw message in hex
    std::cout << "Raw message: ";
    for (int i = 0; i < bytes_read; i++) {
        printf("%02x ", buffer[i]);
    }
    std::cout << '\n';

    if (bytes_read > 0) {
        std::cout << "Received " << bytes_read << " bytes after handshake." << '\n';

        // Print raw bytes in hex format for debugging
        std::cout << "Raw message: ";
        for (int i = 0; i < bytes_read; i++) {
            printf("%02x ", buffer[i]);
        }
        std::cout << '\n';

        // Ensure we have at least 4 bytes for the length prefix
        if (bytes_read >= 4) {
            uint32_t messageLength;
            memcpy(&messageLength, buffer.data(), 4);
            messageLength = ntohl(messageLength);  // Convert from network byte order
            std::cout << "Parsed message length: " << messageLength << '\n';

            // Read full message if more data is expected
            if (messageLength + 4 > bytes_read) {
                int remaining_bytes = (messageLength + 4) - bytes_read;
                std::vector<uint8_t> extra_buffer(remaining_bytes);
                int extra_read = recv(peerSocket, reinterpret_cast<char*>(extra_buffer.data()), remaining_bytes, 0);
                if (extra_read > 0) {
                    buffer.insert(buffer.end(), extra_buffer.begin(), extra_buffer.begin() + extra_read);
                    bytes_read += extra_read;
                }
            }
        }

        // Ensure we have at least 1 byte for message type
        if (bytes_read >= 5) {
            uint8_t messageType = buffer[4];  // The message type is always at index 4
            std::cout << "Message Type ID: " << (int)messageType << '\n';

            switch (messageType) {
                case 5: {  // Bitfield
                    std::cout << "Received BITFIELD message!" << '\n';
                    std::vector<uint8_t> bitfield(buffer.begin() + 5, buffer.end());
                    handleBitfield(peerSocket, bitfield);
                    break;
                }
                case 0:
                    std::cout << "Received CHOKE message!" << '\n';
                    break;
                case 1:
                    std::cout << "Received UNCHOKE message!" << '\n';
                    break;
                case 2:
                    std::cout << "Received INTERESTED message!" << '\n';
                    break;
                case 3:
                    std::cout << "Received NOT INTERESTED message!" << '\n';
                    break;
                default:
                    std::cout << "Unknown message type: " << (int)messageType << '\n';
                    break;
            }
        } else {
            std::cout << "Not enough data to determine message type." << '\n';
        }
    } else {
        std::cerr << "No message received after handshake." << '\n';
    }
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

void PeerWireProtocol::handleBitfield(int peerSocket, const std::vector<uint8_t>& bitfieldBytes) {
    std::vector<bool> bitfield;  // Store pieces in a bitfield
    int bitIndex = 0;
    int numPieces = torrentFile.numPieces;  // Fetch numPieces from parser


    // Convert each byte into 8 bits 
    // for (uint8_t byte : bitfieldBytes) {
    //     for (int i = 7; i >= 0; --i) {  // Extract bits from MSB to LSB
    //         // bitfield.push_back((byte >> i) & 1);
    //         if (bitIndex < numPieces) {  // Only store valid bits
    //             bitfield.push_back((byte >> i) & 1);
    //         }
    //         bitIndex++;
    //     }
    // }
    for (size_t i = 0; i < bitfieldBytes.size(); ++i) {
        for (int j = 7; j >= 0; --j) {
            size_t bitIndex = (i * 8) + (7 - j);
            if (bitIndex >= numPieces) break;  // Prevent extra bits
            bitfield[bitIndex] = (bitfieldBytes[i] >> j) & 1;
        }
    }

    std::cout << "Bitfield length (bits): " << bitfield.size() << '\n';
    std::cout << "Bitfield length (bytes): " << bitfieldBytes.size() << '\n';


    // Store the processed bitfield in peer's state
    std::lock_guard<std::mutex> lock(peerMutex);
    if (peers.find(peerSocket) != peers.end()) {
        peers[peerSocket]->bitfield = bitfield;
    }

    std::cout << "Processed bitfield from peer " << peerSocket << ": ";
    for (bool hasPiece : bitfield) {
        std::cout << hasPiece;
    }
    std::cout << '\n';

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