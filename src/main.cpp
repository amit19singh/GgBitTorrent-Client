// #include <iostream>
// #include <sstream>
// #include <stdexcept>
// #include <vector>
// #include <random>
// #include <algorithm>
// #include <thread>
// #include <chrono>
// #include "../include/peer_wire_protocol.hpp"
// #include "../include/peer_connection.hpp"


// int main() {
//     // Generate a random Node ID
//     DHT::NodeID my_node_id = DHT::DHTBootstrap::generate_random_node_id();

//     // Create a DHTBootstrap instance
//     DHT::DHTBootstrap dht_bootstrap(my_node_id);
//     dht_bootstrap.add_bootstrap_node("67.215.246.10", 6881);

//     // Generate a random target Node ID
//     DHT::NodeID target_id = DHT::DHTBootstrap::generate_random_node_id();

//     // Send a FIND_NODE request
//     auto nodes = dht_bootstrap.send_find_node_request(dht_bootstrap.get_bootstrap_nodes()[0], target_id);

//     // Print the received nodes
//     std::cout << "Received " << nodes.size() << " nodes:" << '\n';
//     for (const auto& node : nodes) {
//         std::cout << "  Node: " << node.ip << ":" << node.port  
//                   << " (ID: " << DHT::node_id_to_hex(node.id) << ")" << '\n';
//     }
    
//     dht_bootstrap.bootstrap();

//     // Keep it running
//     dht_bootstrap.run();
//     return 0;
// }

// #include "../include/peer_wire_protocol.hpp"
// #include <iostream>
// #include <array>
// #include <cstdlib> 


// int main(int argc, char* argv[]) {

//     if (argc != 3) {
//         std::cerr << "Usage: " << argv[0] << " <peer_ip> <peer_port>\n";
//         return 1;
//     }

//     std::string peerIP = argv[1];
//     int peerPort = std::atoi(argv[2]);  // Convert string to int

//     if (peerPort <= 0 || peerPort > 65535) {
//         std::cerr << "Error: Invalid port number.\n";
//         return 1;
//     }

//     PeerWireProtocol pwp;
    
//     // Set info hash (use a real torrent hash)
//     std::array<uint8_t, 20> testInfoHash = {0x3f, 0x9a, 0xac, 0x15, 0x8c, 0x7d, 0xe8, 
//                                             0xdf, 0xca, 0xb1, 0x71, 0xea, 0x58, 0xa1, 
//                                             0x7a, 0xab, 0xdf, 0x7f, 0xbc, 0x93};
//     pwp.setInfoHash(testInfoHash);
    
//     // Connect to the specified peer
//     try {
//         std::cout << "Connecting to " << peerIP << ":" << peerPort << "...\n";
//         pwp.connectToPeer(peerIP, peerPort);
//     } catch (const std::exception& e) {
//         std::cerr << "Connection failed: " << e.what() << '\n';
//     }

//     return 0;
// }


// #include <iostream>
// #include <sstream>
// #include <stdexcept>
// #include "../include/peer_wire_protocol.hpp"

// int main(int argc, char* argv[]) {
//     if (argc != 2) {
//         std::cerr << "Usage: " << argv[0] << " <peer_ip:port>\n";
//         return 1;
//     }

//     std::string peerInput = argv[1];
//     std::string peerIP;
//     int peerPort;

//     // Split peerInput into IP and port
//     size_t colonPos = peerInput.find(':');
//     if (colonPos == std::string::npos) {
//         std::cerr << "Error: Invalid format. Expected <peer_ip:port>\n";
//         return 1;
//     }

//     peerIP = peerInput.substr(0, colonPos);
//     std::string portStr = peerInput.substr(colonPos + 1);

//     try {
//         peerPort = std::stoi(portStr);
//         if (peerPort <= 0 || peerPort > 65535) {
//             throw std::out_of_range("Invalid port number");
//         }
//     } catch (const std::exception& e) {
//         std::cerr << "Error: Invalid port number: " << portStr << "\n";
//         return 1;
//     }

//     // Hardcoded .torrent file location
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";  

//     PeerWireProtocol pwp(torrentFilePath);

//     try {
//         std::cout << "Connecting to " << peerIP << ":" << peerPort << "...\n";
//         pwp.connectToPeer(peerIP, peerPort);
//     } catch (const std::exception& e) {
//         std::cerr << "Connection failed: " << e.what() << '\n';
//         return 1;
//     }

//     return 0;
// }


////////////////////////////////////////////// WORKING //////////////////////////////////////////////
// int main() {
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";
//     PeerWireProtocol pwp(torrentFilePath);

//     int peerSocket = 100;  // Fake peer socket for testing
//     int pieceIndex = 10;
//     int blockSize = 16384;  // 16 KB blocks (standard)
//     int totalBlocks = pwp.torrentFile.pieceLength / blockSize;  // Compute total blocks per piece

//     // Add a fake peer manually before testing
//     std::shared_ptr<PeerConnection> testPeer = std::make_shared<PeerConnection>();
//     pwp.peers[peerSocket] = testPeer;

//     // Simulate requesting the first block
//     std::cout << "\n🔹 Testing handleRequest (First Block)...\n";
//     pwp.handleRequest(peerSocket, pieceIndex, 0, blockSize);

//     // Simulate receiving all blocks for a full piece
//     std::cout << "\n🔹 Testing handlePiece (Full Piece Download)...\n";
//     for (int i = 0; i < totalBlocks; i++) {
//         int blockOffset = i * blockSize;
//         std::vector<uint8_t> fakeData(blockSize, i);  // Dummy data filled with `i`
        
//         std::cout << "🔸 Simulating block " << i << " (Offset: " << blockOffset << ")\n";
//         pwp.handlePiece(peerSocket, pieceIndex, blockOffset, fakeData);
//     }

//     return 0;
// }

////////////////////////////////////////////// WORKING //////////////////////////////////////////////
// int main() {
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";
//     PeerWireProtocol pwp(torrentFilePath);

//     int peerSocket = 100;  // Fake peer socket for testing
//     int pieceIndex = 10;
//     int blockOffset = 0;
//     int blockSize = 16384;  // 16 KB (valid request)

//     // Add a fake peer manually before testing
//     std::shared_ptr<PeerConnection> testPeer = std::make_shared<PeerConnection>();
//     pwp.peers[peerSocket] = testPeer;


//     std::cout << "Testing handleRequest...\n";
//     pwp.handleRequest(peerSocket, pieceIndex, blockOffset, blockSize);

//     // Simulate receiving the same piece
//     std::vector<uint8_t> fakeData(blockSize, 0xAB); // Dummy data (all 0xAB)
//     std::cout << "Testing handlePiece...\n";
//     pwp.handlePiece(peerSocket, pieceIndex, blockOffset, fakeData);

//     return 0;
// }

////////////////////////////////////////////// WORKING //////////////////////////////////////////////

// int main() {
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";
//     PeerWireProtocol pwp(torrentFilePath);

//     int peerSocket = 100;  // Fake peer socket for testing
//     int blockSize = 16384; // 16 KB block size

//     // Add a fake peer
//     std::shared_ptr<PeerConnection> testPeer = std::make_shared<PeerConnection>();
//     pwp.peers[peerSocket] = testPeer;

//     std::vector<int> testPieces = {10, 20, 30};  // Request multiple pieces

//     for (int pieceIndex : testPieces) {
//         std::cout << "🟡 Testing handleRequest for piece " << pieceIndex << "...\n";
//         pwp.handleRequest(peerSocket, pieceIndex, 0, blockSize);

//         // Simulate receiving all blocks for this piece
//         std::cout << "🟢 Testing handlePiece for full piece " << pieceIndex << "...\n";
//         for (int i = 0; i < pwp.pieceStorage->getBlockCount(pieceIndex); i++) {
//             int blockOffset = i * blockSize;
//             std::vector<uint8_t> fakeData(blockSize, 0xAB); // Dummy data
//             pwp.handlePiece(peerSocket, pieceIndex, blockOffset, fakeData);
//         }
//     }

//     return 0;
// }

// int main() {
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";
//     PeerWireProtocol pwp(torrentFilePath);

//     int peerSocket = 100;  // Fake peer socket for testing
//     int blockSize = 16384; // 16 KB block size
//     std::shared_ptr<PeerConnection> testPeer = std::make_shared<PeerConnection>();
//     pwp.peers[peerSocket] = testPeer;

//     std::vector<int> testPieces = {10, 20, 30};  

//     for (int pieceIndex : testPieces) {
//         std::cout << "\n🟡 Testing handleRequest for piece " << pieceIndex << "...\n";
//         pwp.handleRequest(peerSocket, pieceIndex, 0, blockSize);

//         int totalBlocks = pwp.pieceStorage->getBlockCount(pieceIndex);
//         std::vector<int> blockOrder(totalBlocks);
//         std::iota(blockOrder.begin(), blockOrder.end(), 0);

//         // **1️⃣ Randomize block order to simulate out-of-order reception**
//         std::shuffle(blockOrder.begin(), blockOrder.end(), std::mt19937{std::random_device{}()});

//         std::cout << "🔀 Block reception order: ";
//         for (int b : blockOrder) std::cout << b << " ";
//         std::cout << "\n";

//         // **2️⃣ Simulate missing blocks by skipping some at random**
//         std::vector<int> missingBlocks = {3, 7}; // Example: Skip block 3 and 7
//         std::cout << "⛔ Simulating missing blocks: ";
//         for (int m : missingBlocks) std::cout << m << " ";
//         std::cout << "\n";

//         // **3️⃣ Corrupt a random block**
//         int corruptBlock = blockOrder[std::rand() % blockOrder.size()];
//         std::cout << "⚠️ Corrupting block " << corruptBlock << " for piece " << pieceIndex << "\n";

//         // **4️⃣ Send blocks in the shuffled order**
//         for (int blockIndex : blockOrder) {
//             if (std::find(missingBlocks.begin(), missingBlocks.end(), blockIndex) != missingBlocks.end()) {
//                 continue;  // Skip sending this block
//             }

//             int blockOffset = blockIndex * blockSize;
//             std::vector<uint8_t> fakeData(blockSize, 0xAB);

//             // **Corrupt one block**
//             if (blockIndex == corruptBlock) {
//                 fakeData[0] ^= 0xFF; // Flip first byte to corrupt it
//             }

//             std::cout << "📦 Receiving block " << blockIndex << " (Offset: " << blockOffset << ")\n";
//             pwp.handlePiece(peerSocket, pieceIndex, blockOffset, fakeData);
//         }
//     }

//     return 0;
// }


// int main() {
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";
//     PeerWireProtocol pwp(torrentFilePath);

//     int peerSocket = 100;  // Fake peer socket for testing
//     int blockSize = 16384; // 16 KB block size
//     std::shared_ptr<PeerConnection> testPeer = std::make_shared<PeerConnection>();
//     pwp.peers[peerSocket] = testPeer;

//     std::vector<int> testPieces = {10, 20, 30};  

//     for (int pieceIndex : testPieces) {
//         std::cout << "\n🟡 Testing handleRequest for piece " << pieceIndex << "...\n";
//         pwp.handleRequest(peerSocket, pieceIndex, 0, blockSize);

//         int totalBlocks = pwp.pieceStorage->getBlockCount(pieceIndex);
//         std::vector<int> blockOrder(totalBlocks);
//         std::iota(blockOrder.begin(), blockOrder.end(), 0);

//         // **1️⃣ Randomize block order to simulate out-of-order reception**
//         std::shuffle(blockOrder.begin(), blockOrder.end(), std::mt19937{std::random_device{}()});

//         std::cout << "🔀 Block reception order: ";
//         for (int b : blockOrder) std::cout << b << " ";
//         std::cout << "\n";

//         // **2️⃣ Simulate missing blocks**
//         std::vector<int> missingBlocks = {3, 7};  // Example: Skip block 3 and 7
//         std::cout << "⛔ Simulating missing blocks: ";
//         for (int m : missingBlocks) std::cout << m << " ";
//         std::cout << "\n";

//         // **3️⃣ Corrupt a random block**
//         int corruptBlock = blockOrder[std::rand() % blockOrder.size()];
//         std::cout << "⚠️ Corrupting block " << corruptBlock << " for piece " << pieceIndex << "\n";

//         // **4️⃣ Send blocks in shuffled order**
//         for (int blockIndex : blockOrder) {
//             if (std::find(missingBlocks.begin(), missingBlocks.end(), blockIndex) != missingBlocks.end()) {
//                 continue;  // Skip sending this block
//             }

//             int blockOffset = blockIndex * blockSize;
//             std::vector<uint8_t> fakeData(blockSize, 0xAB);

//             // **Corrupt one block**
//             if (blockIndex == corruptBlock) {
//                 fakeData[0] ^= 0xFF; // Flip first byte to corrupt it
//             }

//             std::cout << "📦 Receiving block " << blockIndex << " (Offset: " << blockOffset << ")\n";
//             pwp.handlePiece(peerSocket, pieceIndex, blockOffset, fakeData);
//         }

//         // **5️⃣ Delay and send the missing blocks**
//         std::cout << "\n⏳ Waiting 3 seconds before sending missing blocks...\n";
//         std::this_thread::sleep_for(std::chrono::seconds(3));

//         for (int blockIndex : missingBlocks) {
//             int blockOffset = blockIndex * blockSize;
//             std::vector<uint8_t> fakeData(blockSize, 0xAB);

//             std::cout << "📦 Sending previously missing block " << blockIndex << " (Offset: " << blockOffset << ")\n";
//             pwp.handlePiece(peerSocket, pieceIndex, blockOffset, fakeData);
//         }
//     }

//     return 0;
// }

// std::string toHexString(const std::array<uint8_t, 20>& infoHash) {
//     std::ostringstream oss;
//     oss << std::hex << std::setfill('0');
//     for (uint8_t byte : infoHash) {
//         oss << std::setw(2) << static_cast<int>(byte);
//     }
//     return oss.str();
// }

// void testPeerDiscovery(const std::string& torrentFilePath) {
//     std::cout << "**TESTING PEER DISCOVERY USING DHT**" << '\n';

//     // Initialize PeerWireProtocol (automatically bootstraps DHT)
//     PeerWireProtocol pwp(torrentFilePath);

//     std::cout << "**WAITING FOR DHT BOOTSTRAP TO COMPLETE...**" << '\n';
//     std::this_thread::sleep_for(std::chrono::seconds(5));

//     std::string hexInfoHash = toHexString(pwp.getInfoHash());
//     std::cout << "**REQUESTING PEERS FOR TORRENT INFOHASH: " << hexInfoHash << "**" << '\n';

//     // Request peers
//     std::vector<DHT::Node> discoveredPeers = pwp.dht_instance->findPeers(pwp.getInfoHash());

//     if (discoveredPeers.empty()) {
//         std::cout << "**ERROR: NO PEERS DISCOVERED.**" << '\n';
//         return;
//     }

//     std::cout << "**SUCCESSFUL: DISCOVERED PEERS:**" << '\n';
//     for (const auto& peer : discoveredPeers) {
//         std::cout << "PEER -> IP: " << peer.ip << " PORT: " << peer.port << '\n';
//     }

//     // Try connecting to the first discovered peer
//     std::string peerIP = discoveredPeers[0].ip;
//     uint16_t peerPort = discoveredPeers[0].port;
//     int socket = pwp.connectToPeer(peerIP, peerPort);

//     if (socket >= 0) {
//         std::cout << "**SUCCESSFUL: CONNECTION ESTABLISHED WITH PEER " << peerIP << ":" << peerPort << "**" << '\n';
//     } else {
//         std::cout << "**ERROR: FAILED TO CONNECT TO PEER " << peerIP << ":" << peerPort << "**" << '\n';
//     }
// }

// int main() {
//     std::cout << "**STARTING PEER DISCOVERY TEST**" << '\n';
//     testPeerDiscovery("C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent");
//     return 0;
// }



// #include <iostream>
// #include <sstream>
// #include <stdexcept>
// #include "../include/peer_wire_protocol.hpp"

// int main(int argc, char* argv[]) {
//     if (argc != 2) {
//         std::cerr << "Usage: " << argv[0] << " <peer_ip:port>\n";
//         return 1;
//     }

//     std::string peerInput = argv[1];
//     std::string peerIP;
//     int peerPort;

//     // Split peerInput into IP and port
//     size_t colonPos = peerInput.find(':');
//     if (colonPos == std::string::npos) {
//         std::cerr << "Error: Invalid format. Expected <peer_ip:port>\n";
//         return 1;
//     }

//     peerIP = peerInput.substr(0, colonPos);
//     std::string portStr = peerInput.substr(colonPos + 1);

//     try {
//         peerPort = std::stoi(portStr);
//         if (peerPort <= 0 || peerPort > 65535) {
//             throw std::out_of_range("Invalid port number");
//         }
//     } catch (const std::exception& e) {
//         std::cerr << "Error: Invalid port number: " << portStr << "\n";
//         return 1;
//     }

//     // Hardcoded .torrent file location
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";  

//     PeerWireProtocol pwp(torrentFilePath);

//     try {
//         std::cout << "Connecting to " << peerIP << ":" << peerPort << "...\n";
//         pwp.connectToPeer(peerIP, peerPort);
//     } catch (const std::exception& e) {
//         std::cerr << "Connection failed: " << e.what() << '\n';
//         return 1;
//     }

//     return 0;
// }


// #include <iostream>
// #include <vector>
// #include <string>
// #include <thread>
// #include <chrono>
// #include <mutex>
// #include <atomic>
// #include "../include/peer_wire_protocol.hpp"


// std::string toHexString(const std::array<uint8_t, 20>& infoHash) {
//     std::ostringstream oss;
//     oss << std::hex << std::setfill('0');
//     for (uint8_t byte : infoHash) {
//         oss << std::setw(2) << static_cast<int>(byte);
//     }
//     return oss.str();
// }


// int main() {
//     std::cout << "**STARTING PEER DISCOVERY AND CONNECTION TEST**" << '\n';

//     // Hardcoded .torrent file location
//     std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";  

//     // Initialize PeerWireProtocol (which bootstraps DHT)
//     PeerWireProtocol pwp(torrentFilePath);

//     // Give DHT some time to bootstrap
//     std::cout << "**WAITING FOR DHT BOOTSTRAP TO COMPLETE...**" << '\n';
//     std::this_thread::sleep_for(std::chrono::seconds(5));

//     // Request peers for the torrent
//     std::cout << "**REQUESTING PEERS FOR TORRENT INFOHASH: " << toHexString(pwp.getInfoHash()) << "**" << '\n';
//     std::vector<DHT::Node> discoveredPeers = pwp.dht_instance->findPeers(pwp.getInfoHash());

//     if (discoveredPeers.empty()) {
//         std::cerr << "**ERROR: NO PEERS DISCOVERED!**" << '\n';
//         return 1;
//     }

//     std::cout << "**SUCCESSFUL: DISCOVERED PEERS:**" << '\n';
//     for (const auto& peer : discoveredPeers) {
//         std::cout << "PEER -> IP: " << peer.ip << " PORT: " << peer.port << '\n';
//     }

//     std::atomic<bool> connected = false;  // Flag to stop other threads after success
//     std::mutex outputMutex;  // Protects console output

//     std::vector<std::thread> threads;

//     // Attempt to connect to multiple peers concurrently
//     for (const auto& peer : discoveredPeers) {
//         if (connected.load()) break;  // Stop if already connected

//         threads.emplace_back([&]() {
//             if (connected.load()) return;  // Another thread succeeded, stop

//             try {
//                 {
//                     std::lock_guard<std::mutex> lock(outputMutex);
//                     std::cout << "**ATTEMPTING TO CONNECT TO PEER: " << peer.ip << ":" << peer.port << "**" << '\n';
//                 }

//                 pwp.connectToPeer(peer.ip, peer.port);

//                 {
//                     std::lock_guard<std::mutex> lock(outputMutex);
//                     // std::cout << "**SUCCESSFUL: CONNECTED TO PEER AND SENT HANDSHAKE**" << '\n';
//                 }

//                 connected.store(true);  // Mark success
//             } catch (const std::exception& e) {
//                 std::lock_guard<std::mutex> lock(outputMutex);
//                 std::cerr << "**ERROR: CONNECTION FAILED TO " << peer.ip << ":" << peer.port << " - " << e.what() << "**" << '\n';
//             }
//         });
//     }

//     // Wait for all threads to finish
//     for (auto& t : threads) {
//         if (t.joinable()) t.join();
//     }
//     if (!connected.load()) {
//         std::cerr << "**ERROR: FAILED TO CONNECT TO ANY PEER**" << '\n';
//         return 1;
//     }

//     return 0;
//     // // Try connecting to multiple peers until one succeeds
//     // bool connected = false;
//     // for (const auto& peer : discoveredPeers) {
//     //     try {
//     //         pwp.connectToPeer(peer.ip, peer.port);
//     //         std::cout << "**SUCCESSFUL: CONNECTED TO PEER AND SENT HANDSHAKE**" << '\n';
//     //         connected = true;
//     //         break;  // Stop after the first successful connection
//     //     } catch (const std::exception& e) {
//     //         std::cerr << "**ERROR: CONNECTION FAILED TO " << peer.ip << ":" << peer.port << " - " << e.what() << "**" << '\n';
//     //     }
//     // }

//     // if (!connected) {
//     //     std::cerr << "**ERROR: FAILED TO CONNECT TO ANY PEERS**" << '\n';
//     //     return 1;
//     // }

//     // return 0;
// }






#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include "../include/peer_wire_protocol.hpp"


std::string toHexString(const std::array<uint8_t, 20>& infoHash) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : infoHash) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

int main() {
    std::cout << "**STARTING PEER DISCOVERY AND CONNECTION TEST**" << '\n';

    // Hardcoded .torrent file location.
    std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";
    
    // Initialize PeerWireProtocol (which bootstraps DHT in the constructor)
    PeerWireProtocol pwp(torrentFilePath);

    // First, try querying the tracker.
    std::vector<DHT::Node> trackerPeers = pwp.queryTracker();
    std::vector<DHT::Node> peersToTry;
    
    if (!trackerPeers.empty()) {
        std::cout << "**SUCCESSFUL: TRACKER RETURNED " << trackerPeers.size() << " PEERS**" << '\n';
        for (const auto& peer : trackerPeers) {
            std::cout << "TRACKER PEER -> IP: " << peer.ip << " PORT: " << peer.port << '\n';
        }
        peersToTry = trackerPeers;
    } else {
        std::cout << "**WARNING: TRACKER RETURNED NO PEERS. FALLING BACK TO DHT DISCOVERED PEERS**" << '\n';
        // Use DHT-discovered peers. For this, you might have to call a function like findPeers.
        peersToTry = pwp.dht_instance->findPeers(pwp.getInfoHash());
        if (peersToTry.empty()) {
            std::cerr << "**ERROR: NO PEERS DISCOVERED FROM DHT**" << '\n';
            return 1;
        }
        std::cout << "**SUCCESSFUL: DHT DISCOVERED " << peersToTry.size() << " PEERS**" << '\n';
        for (const auto& peer : peersToTry) {
            std::cout << "DHT PEER -> IP: " << peer.ip << " PORT: " << peer.port << '\n';
        }
    }

    // Try connecting to multiple peers concurrently
    std::atomic<bool> connected(false);
    std::mutex outputMutex;
    std::vector<std::thread> threads;

    for (const auto& peer : peersToTry) {
        if (connected.load())
            break;

        threads.emplace_back([&]() {
            if (connected.load())
                return;
            try {
                {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cout << "**ATTEMPTING TO CONNECT TO PEER: " << peer.ip << ":" << peer.port << "**" << '\n';
                }
                int sock = pwp.connectToPeer(peer.ip, peer.port);
                if (sock >= 0) {
                    {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        std::cout << "**SUCCESSFUL: CONNECTED TO PEER " << peer.ip << ":" << peer.port << "**" << '\n';
                    }
                    connected.store(true);
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cerr << "**ERROR: CONNECTION FAILED TO " << peer.ip << ":" << peer.port << " - " << e.what() << "**" << '\n';
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }

    if (!connected.load()) {
        std::cerr << "**ERROR: FAILED TO CONNECT TO ANY PEERS**" << '\n';
        return 1;
    }

    return 0;
}


