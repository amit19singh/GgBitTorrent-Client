// #include "../include/dht_bootstrap.hpp"
// #include <iostream>

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
//     std::cout << "Received " << nodes.size() << " nodes:" << std::endl;
//     for (const auto& node : nodes) {
//         std::cout << "  Node: " << node.ip << ":" << node.port  
//                   << " (ID: " << DHT::node_id_to_hex(node.id) << ")" << std::endl;
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
//         std::cerr << "Connection failed: " << e.what() << std::endl;
//     }

//     return 0;
// }


#include <iostream>
#include <sstream>
#include <stdexcept>
#include "../include/peer_wire_protocol.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <peer_ip:port>\n";
        return 1;
    }

    std::string peerInput = argv[1];
    std::string peerIP;
    int peerPort;

    // Split peerInput into IP and port
    size_t colonPos = peerInput.find(':');
    if (colonPos == std::string::npos) {
        std::cerr << "Error: Invalid format. Expected <peer_ip:port>\n";
        return 1;
    }

    peerIP = peerInput.substr(0, colonPos);
    std::string portStr = peerInput.substr(colonPos + 1);

    try {
        peerPort = std::stoi(portStr);
        if (peerPort <= 0 || peerPort > 65535) {
            throw std::out_of_range("Invalid port number");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid port number: " << portStr << "\n";
        return 1;
    }

    // Hardcoded .torrent file location
    std::string torrentFilePath = "C:/Users/amit1/Downloads/ubuntu-24.10-desktop-amd64.iso.torrent";  

    PeerWireProtocol pwp(torrentFilePath);

    try {
        std::cout << "Connecting to " << peerIP << ":" << peerPort << "...\n";
        pwp.connectToPeer(peerIP, peerPort);
    } catch (const std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
