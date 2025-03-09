#include "../include/peer_wire_protocol.hpp"
#include "../include/peer_connection.hpp"
#include "../include/torrent_file_parser.hpp"
#include "../include/piece_manager.hpp"

#include <cstring>
#include <algorithm>
#include <random>
#include <iostream>
#include <array>
#include <openssl/evp.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// Helper function to close sockets cross-platform
void closeSocket(int sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

PeerWireProtocol::PeerWireProtocol(const std::string& torrentFilePath) 
    : torrentFileParser(torrentFilePath), pieceStorage(nullptr) {
        std::cout << "Initializing DHT Bootstrap in PeerWireProtocol..." << '\n';
    
        // Generate a random node ID
        DHT::NodeID my_node_id = DHT::DHTBootstrap::generate_random_node_id();
    
        // Initialize DHT Bootstrap
        dht_instance = new DHT::DHTBootstrap(my_node_id);
        dht_instance->add_bootstrap_node("67.215.246.10", 6881); // Add a known bootstrap node
    
        std::cout << "DHT Bootstrap initialized successfully." << '\n';
        
        // Automatically bootstrap the DHT
        dht_instance->bootstrap();
    try {
        torrentFile = torrentFileParser.parse();  // Parse the torrent file
        infoHash = torrentFile.infoHash;
        // Initialize PieceManager with the number of pieces and piece length
        // pieceStorage = std::make_unique<PieceManager>(torrentFile.numPieces, torrentFile.pieceLength);

        std::cout << "Torrent parsed: " << torrentFile.numPieces 
                    << " pieces, " << torrentFile.pieceLength << " bytes each.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error parsing torrent file: " << e.what() << '\n';
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

int PeerWireProtocol::connectToPeer(const std::string& peerIP, int peerPort) {
    std::cout << "**ATTEMPTING TO CONNECT TO PEER: " << peerIP << ":" << peerPort << "**" << std::endl;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(peerPort);
    inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);

    if (connect(sock, (sockaddr*)&peerAddr, sizeof(peerAddr))) {
        std::cerr << "**ERROR: CONNECTION FAILED TO " << peerIP << ":" << peerPort << " - Connection refused or timed out**" << std::endl;
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

    return sock;
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
    
    // int received = 0;
    // int remaining = 68;
    // int attempts = 3;  // Number of retries

    // while (remaining > 0 && attempts-- > 0) {
    //     int bytes = recv(peerSocket, reinterpret_cast<char*>(response) + received, remaining, 0);

    //     if (bytes > 0) {
    //         std::cout << "**DEBUG: Received " << bytes << " bytes:** ";
    //         for (int i = 0; i < bytes; i++) {
    //             printf("%02x ", response[i]);
    //         }
    //         std::cout << '\n';
    //     }

    //     if (bytes <= 0) {
    //         std::cerr << "**WARNING: Incomplete handshake received. Retrying... (" 
    //                 << attempts << " attempts left)**" << std::endl;
    //         std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //         continue;
    //     }

    //     received += bytes;
    //     remaining -= bytes;
    // }

    // if (received != 68) {
    //     std::cerr << "**ERROR: Handshake failed. Received only " << received 
    //             << " bytes instead of 68. Closing connection.**" << std::endl;
    //     closeSocket(peerSocket);
    //     return;
    // }

    // std::cout << "**SUCCESSFUL: FULL HANDSHAKE RECEIVED FROM PEER**" << std::endl;



///////////////////////////////////////////////////////////////////////////////////////////////////////////
    int received = recv(peerSocket, reinterpret_cast<char*>(response), sizeof(response), 0);
    if (received != 68) {
        std::cerr << "Peer did not respond with a valid handshake. Received " << received << " bytes.\n";
        std::cerr << "Recv error: " << strerror(errno) << '\n';
        return;
    }
    std::cout << "Received handshake from peer!\n";
///////////////////////////////////////////////////////////////////////////////////////////////////////////



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

/////////////////////////////////////////////////////// HERE ///////////////////////////////////////////////////////
void PeerWireProtocol::handleRequest(int peerSocket, int pieceIndex, int blockOffset, int blockSize) {
    std::lock_guard<std::mutex> lock(peerMutex);

    // Check if peer exists
    if (peers.find(peerSocket) == peers.end()) {
        std::cerr << "Error: Peer socket " << peerSocket << " not found.\n";
        return;
    }

    // Validate request
    if (pieceIndex < 0 || pieceIndex >= torrentFile.numPieces || blockSize <= 0 || blockSize > MAX_BLOCK_SIZE) {
        std::cerr << "Invalid request from peer " << peerSocket << " for piece " << pieceIndex 
                  << ", offset " << blockOffset << ", size " << blockSize << '\n';
        return;
    }

    // Fetch the requested piece data
    std::vector<uint8_t> pieceData;
    if (!pieceStorage->getPieceBlock(pieceIndex, blockOffset, blockSize, pieceData)) {
        std::cerr << "Error: Failed to retrieve requested block.\n";
        return;
    } else {
        std::cout << "Retrieved " << pieceData.size() << " bytes for piece " << pieceIndex << " (offset " << blockOffset << ").\n";
    }

    // Construct the Piece message
    std::vector<uint8_t> pieceMessage(9 + blockSize);  // 9 bytes header + block data
    uint32_t netPiece = htonl(pieceIndex);
    uint32_t netOffset = htonl(blockOffset);
    
    memcpy(pieceMessage.data(), &netPiece, 4);
    memcpy(pieceMessage.data() + 4, &netOffset, 4);
    memcpy(pieceMessage.data() + 8, pieceData.data(), blockSize);

    // Add message to peer's queue
    peers[peerSocket]->message_queue.push_back(pieceMessage);
    
    std::cout << "Queued response for peer " << peerSocket << " for piece " << pieceIndex 
              << " (offset " << blockOffset << ", size " << blockSize << ").\n";
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
// void PeerWireProtocol::handlePiece(int peerSocket, int pieceIndex, int blockOffset, const std::vector<uint8_t>& blockData) {
//     std::lock_guard<std::mutex> lock(peerMutex);

//     // Validate piece index
//     if (pieceIndex < 0 || pieceIndex >= torrentFile.numPieces) {
//         std::cerr << "Error: Invalid piece index " << pieceIndex << " received from peer " << peerSocket << '\n';
//         return;
//         }

//     // Store the received block
//     bool success = pieceStorage->storePieceBlock(pieceIndex, blockOffset, blockData);
//     if (!success) {
//         std::cerr << "Error: Failed to store received piece block for piece " << pieceIndex << '\n';
//         return;
//     }

//     std::cout << "Received piece " << pieceIndex << ", block " << blockOffset 
//     << " (" << blockData.size() << " bytes) from peer " << peerSocket << '\n';

//     // Check if we have received the full piece
//     if (pieceStorage->isPieceComplete(pieceIndex)) {
//         std::cout << "storePieceBlock: Piece " << pieceIndex << " is now complete!\n";
//         std::vector<uint8_t> fullPiece;
//         // pieceStorage->getFullPiece(pieceIndex, fullPiece);
//         // Retrieve full piece data
//         if (!pieceStorage->getFullPiece(pieceIndex, fullPiece) || fullPiece.empty()) {
//             std::cerr << "ERROR: Failed to retrieve full piece data for verification (Piece " << pieceIndex << ").\n";
//             return;
//         }

//         std::cout << "Verifying SHA-1 for complete piece " << pieceIndex << "...\n";

//         // Compute SHA-1 hash and verify
//         std::string computedHash = computeSHA1(fullPiece);
//         std::string expectedHash = torrentFile.pieces[pieceIndex];

//         std::cout << "Computed Hash: " << computedHash << '\n';
//         std::cout << "Expected Hash: " << expectedHash << '\n';

//         // if (computedHash != torrentFile.pieces[pieceIndex]) {
//             if (computedHash != expectedHash) {
//             std::cerr << "Error: SHA-1 hash mismatch for piece " << pieceIndex << '\n';
//             return;
//         }

//         // Mark piece as downloaded
//         pieceStorage->markPieceAsDownloaded(pieceIndex);
//         std::cout << "Piece " << pieceIndex << " successfully verified and stored.\n";
//     }
// }

void PeerWireProtocol::handlePiece(int peerSocket, int pieceIndex, int blockOffset, const std::vector<uint8_t>& blockData) {
    std::lock_guard<std::mutex> lock(peerMutex);

    // Validate piece index
    if (pieceIndex < 0 || pieceIndex >= torrentFile.numPieces) {
        std::cerr << "Error: Invalid piece index " << pieceIndex << " received from peer " << peerSocket << '\n';
        return;
    }

    // Store the received block
    bool success = pieceStorage->storePieceBlock(pieceIndex, blockOffset, blockData);
    if (!success) {
        std::cerr << "Error: Failed to store received piece block for piece " << pieceIndex << '\n';
        return;
    }

    std::cout << "Received piece " << pieceIndex << ", block " << blockOffset 
              << " (" << blockData.size() << " bytes) from peer " << peerSocket << '\n';

    // Check if we have received the full piece
    if (pieceStorage->isPieceComplete(pieceIndex)) {
        std::cout << "storePieceBlock: Piece " << pieceIndex << " is now complete!\n";

        std::vector<uint8_t> fullPiece;
        if (!pieceStorage->getFullPiece(pieceIndex, fullPiece) || fullPiece.empty()) {
            std::cerr << "Error: Failed to retrieve full piece data for verification (Piece " << pieceIndex << ").\n";
            return;
        }

        std::cout << "Retrieved full piece " << pieceIndex << " (" << fullPiece.size() << " bytes) for verification.\n";

        /////////////////////////// ONLY FOR TESTING //////////////////////////////////////////////////////////////////
        // if (pieceIndex == 10 || pieceIndex == 20 || pieceIndex == 30) {
        //     torrentFile.pieces[pieceIndex] = computeSHA1(fullPiece); // Override expected hash for test
        // }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Compute SHA-1 hash and verify
        std::string computedHash = computeSHA1(fullPiece);
        std::string expectedHash = rawToHex(torrentFile.pieces[pieceIndex]);
        // std::string expectedHash = torrentFile.pieces[pieceIndex];

        // Debugging: Print hashes
        std::cout << "Computed Hash: " << computedHash << '\n';
        std::cout << "Expected Hash: " << expectedHash << '\n';

        // Validate hash
        if (computedHash != expectedHash) {
            std::cerr << "Error: SHA-1 hash mismatch for piece " << pieceIndex << "!\n";
            return;
        }

        // Mark piece as successfully downloaded
        pieceStorage->markPieceAsDownloaded(pieceIndex);
        std::cout << "Piece " << pieceIndex << " successfully verified and stored.\n";
    }
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


std::string PeerWireProtocol::computeSHA1(const std::vector<uint8_t>& data) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha1(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    if (EVP_DigestUpdate(mdctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("EVP_DigestUpdate failed");
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(mdctx);

    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}




//////////////////////////// QUERY TRACKER ////////////////////////////////////////////////////////////////
// Helper: URL-encode a binary string
std::string urlEncode(const std::string& data) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    for (unsigned char c : data) {
        // Encode all characters; in production, you may skip alphanumerics.
        oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return oss.str();
}

// Overload for std::array<uint8_t, 20>
std::string urlEncode(const std::array<uint8_t, 20>& data) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    for (uint8_t byte : data) {
        oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}


// Helper: Parse the "peers" value from the tracker response.
// Handles both compact (binary string) and non-compact (list of dictionaries) formats.
std::vector<DHT::Node> parseTrackerPeers(const BencodedValue& peersValue) {
    std::vector<DHT::Node> peers;

    // Compact format: peers is a single binary string
    if (std::holds_alternative<std::string>(peersValue.value)) {
        std::string peersStr = std::get<std::string>(peersValue.value);
        if (peersStr.size() % 6 != 0) {
            std::cerr << "**ERROR: Invalid peers string length.**" << std::endl;
            return peers;
        }
        for (size_t i = 0; i < peersStr.size(); i += 6) {
            DHT::Node node;
            uint32_t ipBinary = 0;
            memcpy(&ipBinary, peersStr.data() + i, 4);
            struct in_addr ipAddr;
            ipAddr.s_addr = ipBinary;
            node.ip = inet_ntoa(ipAddr);
            uint16_t portNetwork = 0;
            memcpy(&portNetwork, peersStr.data() + i + 4, 2);
            node.port = ntohs(portNetwork);
            peers.push_back(node);
        }
    }
    // Non-compact format: peers is a list of dictionaries
    else if (std::holds_alternative<BencodedList>(peersValue.value)) {
        const BencodedList& peersList = std::get<BencodedList>(peersValue.value);
        for (const auto& peerEntry : peersList) {
            BencodedDict peerDict = peerEntry.asDict();
            auto ipIt = peerDict.find("ip");
            auto portIt = peerDict.find("port");
            if (ipIt != peerDict.end() && portIt != peerDict.end()) {
                DHT::Node node;
                node.ip = ipIt->second.asString();
                node.port = static_cast<uint16_t>(portIt->second.asInt());
                peers.push_back(node);
            } else {
                std::cerr << "**WARNING: Peer dictionary missing 'ip' or 'port'.**" << std::endl;
            }
        }
    }
    else {
        std::cerr << "**ERROR: Unknown peers format.**" << std::endl;
    }
    return peers;
}

// Implementation of queryTracker() in PeerWireProtocol (Windows version)
std::vector<DHT::Node> PeerWireProtocol::queryTracker() {
    std::vector<DHT::Node> trackerPeers;

    // Get the announce URL from the torrent file
    std::string announceUrl = torrentFile.announce;
    if (announceUrl.empty()) {
        std::cerr << "**ERROR: No tracker announce URL in torrent file.**" << std::endl;
        return trackerPeers;
    }

    // Prepare query parameters.
    // The infoHash is stored in torrentFile.infoHash (assumed to be a 20-byte binary string)
    std::string encodedInfoHash = urlEncode(infoHash);
    
    // Get peer_id from the DHT instance (assumed to be DHT::NodeID, e.g., std::array<uint8_t,20>)
    std::string peerIdStr;
    {
        const DHT::NodeID& peerId = dht_instance->getMyNodeId();
        peerIdStr = std::string(reinterpret_cast<const char*>(peerId.data()), peerId.size());
    }
    std::string encodedPeerId = urlEncode(peerIdStr);

    // Build the query parameters:
    // Typical parameters: ?info_hash=...&peer_id=...&port=6881&uploaded=0&downloaded=0&left=0&event=started
    std::ostringstream queryParams;
    queryParams << "?info_hash=" << encodedInfoHash
                << "&peer_id=" << encodedPeerId
                << "&port=6881"
                << "&uploaded=0"
                << "&downloaded=0"
                << "&left=0"
                << "&event=started";

    std::string fullUrl = announceUrl + queryParams.str();
    std::cout << "**QUERYING TRACKER: " << fullUrl << "**" << std::endl;

    // Convert fullUrl (UTF-8) to wide string (UTF-16) for WinHTTP.
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, fullUrl.c_str(), -1, NULL, 0);
    if (wideLen == 0) {
        std::cerr << "**ERROR: Failed to convert URL to wide string.**" << std::endl;
        return trackerPeers;
    }
    std::wstring wFullUrl(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, fullUrl.c_str(), -1, &wFullUrl[0], wideLen);

    // Initialize URL_COMPONENTS structure
    URL_COMPONENTS urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    // Prepare buffers for host name and URL path.
    wchar_t hostName[256] = {0};
    wchar_t urlPath[1024] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(wFullUrl.c_str(), static_cast<DWORD>(wFullUrl.size() * sizeof(wchar_t)), 0, &urlComp)) {
        std::cerr << "**ERROR: WinHttpCrackUrl failed.**" << std::endl;
        return trackerPeers;
    }

    // Open a WinHTTP session.
    HINTERNET hSession = WinHttpOpen(L"PeerWireProtocol/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        std::cerr << "**ERROR: WinHttpOpen failed.**" << std::endl;
        return trackerPeers;
    }

    // Connect to the tracker host.
    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        std::cerr << "**ERROR: WinHttpConnect failed.**" << std::endl;
        WinHttpCloseHandle(hSession);
        return trackerPeers;
    }

    // Open an HTTP request.
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        std::cerr << "**ERROR: WinHttpOpenRequest failed.**" << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return trackerPeers;
    }

    // Send the HTTP GET request.
    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResults) {
        std::cerr << "**ERROR: WinHttpSendRequest failed.**" << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return trackerPeers;
    }

    // Receive the response.
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        std::cerr << "**ERROR: WinHttpReceiveResponse failed.**" << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return trackerPeers;
    }

    // Read the response data into a string.
    std::string responseStr;
    DWORD dwSize = 0;
    do {
        DWORD dwDownloaded = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            std::cerr << "**ERROR: WinHttpQueryDataAvailable failed.**" << std::endl;
            break;
        }

        if (dwSize == 0)
            break;  // No more data

        std::vector<char> buffer(dwSize);
        if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
            std::cerr << "**ERROR: WinHttpReadData failed.**" << std::endl;
            break;
        }

        responseStr.append(buffer.data(), dwDownloaded);
    } while (dwSize > 0);

    // Close WinHTTP handles.
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::cout << "**TRACKER RESPONSE RECEIVED (" << responseStr.size() << " bytes)**" << std::endl;
    // For debugging, print first 256 characters (if available)
    std::cout << responseStr.substr(0, 256) << std::endl;

    // Parse the tracker response using your BencodeParser
    try {
        BencodeParser parser;
        BencodedValue responseValue = parser.parse(responseStr);
        // Expect a dictionary
        BencodedDict responseDict = responseValue.asDict();

        // Look for the "peers" key using find() instead of operator[]
        auto it = responseDict.find("peers");
        if (it == responseDict.end()) {
            std::cerr << "**ERROR: Tracker response does not contain a 'peers' key.**" << std::endl;
            return trackerPeers;
        }
        
        std::cout << "STARTING TO PARSE TRACKER PEERS" << '\n';
        trackerPeers = parseTrackerPeers(it->second);
        
    } catch (const std::exception& e) {
        std::cerr << "**ERROR: Failed to parse tracker response: " << e.what() << "**" << std::endl;
    }

    return trackerPeers;
}


