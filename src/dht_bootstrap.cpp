#include "../include/dht_bootstrap.hpp"
#include "../include/bencode_encoder.hpp"
#include "../include/bencode_parser.hpp"
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib") 
    #include <winsock2.h>
    #include <ws2tcpip.h>
    void init_winsock() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize Winsock!" << std::endl;
            exit(1);
        }
    }
    void cleanup_winsock() {
        WSACleanup();
    }
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace DHT {

    DHTBootstrap::DHTBootstrap(const NodeID& my_node_id)
        : my_node_id_(my_node_id) {
            init_winsock();
        routing_table_.emplace_back();
    }

    void DHTBootstrap::add_bootstrap_node(const std::string& ip, uint16_t port) {
        Node bootstrap_node;
        bootstrap_node.id = generate_random_node_id();
        bootstrap_node.ip = ip;
        bootstrap_node.port = port;
        bootstrap_nodes_.push_back(bootstrap_node);
    }

    void DHTBootstrap::bootstrap() {
        // init_winsock();  // Initialize Winsock on Windows

        for (const auto& bootstrap_node : bootstrap_nodes_) {
            std::cout << "Contacting bootstrap node: " << bootstrap_node.ip << ":" << bootstrap_node.port << std::endl;
            auto nodes = send_find_node_request(bootstrap_node, my_node_id_);
            for (const auto& node : nodes) {
                add_to_routing_table(node);
            }
        }

        cleanup_winsock();  // Cleanup Winsock on Windows
    }

    const std::vector<Bucket>& DHTBootstrap::get_routing_table() const {
        return routing_table_;
    }
    
    const std::vector<Node> DHTBootstrap::get_bootstrap_nodes() {
        return bootstrap_nodes_;
    }

    NodeID DHTBootstrap::generate_random_node_id() {
        NodeID id;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
            id[i] = dist(gen);
        }
        return id;
    }

    NodeID DHTBootstrap::xor_distance(const NodeID& a, const NodeID& b) {
        NodeID result;
        for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
            result[i] = a[i] ^ b[i];
        }
        return result;
    }
    
    std::vector<Node> DHTBootstrap::send_find_node_request(const Node& remote_node, const NodeID& target_id) {
        std::vector<Node> nodes;
    
        // Create UDP socket
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            #ifdef _WIN32
                    std::cerr << "Error creating socket! Winsock error: " << WSAGetLastError() << std::endl;
            #else
                    std::cerr << "Error creating socket! errno: " << strerror(errno) << std::endl;
            #endif
                    return nodes;
        }
    
        // Set socket timeout
        #ifdef _WIN32
            DWORD timeout = 2000; // 2 seconds in milliseconds
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        #else
            struct timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        #endif
        
        // Set up remote address
        sockaddr_in remote_addr{};
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_port = htons(remote_node.port);
        inet_pton(AF_INET, remote_node.ip.c_str(), &remote_addr.sin_addr);
    
        // Log the remote node's address
        std::cout << "Sending FIND_NODE request to: " << remote_node.ip << ":" << remote_node.port << std::endl;
    
        // Create the request message using BencodeEncoder
        BencodedDict query;
        query["id"] = BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20));
        query["target"] = BencodedValue(std::string(reinterpret_cast<const char*>(target_id.data()), 20));
    
        BencodedDict message;
        message["t"] = BencodedValue("aa"); // Transaction ID
        message["y"] = BencodedValue("q");  // Message type (query)
        message["q"] = BencodedValue("find_node");
        message["a"] = BencodedValue(query);
    
        std::string request = BencodeEncoder::encode(message);
    
        // Log the request
        std::cout << "Request: " << request << std::endl;
        std::cout << "Sending: " << request.size() << " bytes -> " << request << std::endl;
    
        // Send the FIND_NODE request
        if (sendto(sock, request.c_str(), request.size(), 0,
                   (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0) {
            #ifdef _WIN32
                    std::cerr << "Sendto failed! Winsock error: " << WSAGetLastError() << std::endl;
            #else
                    std::cerr << "Sendto failed! errno: " << strerror(errno) << std::endl;
            #endif
                    CLOSESOCKET(sock);
                    return nodes;
        }
    
        // Receive response
        char buffer[1024];
        sockaddr_in sender_addr{};
        socklen_t sender_len = sizeof(sender_addr);
        std::cout << "Waiting for response..." << std::endl;
        int bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                      (struct sockaddr*)&sender_addr, &sender_len);
        if (bytes_received < 0) {
            #ifdef _WIN32
                    std::cerr << "recvfrom failed! Winsock error: " << WSAGetLastError() << std::endl;
            #else
                    std::cerr << "recvfrom failed! errno: " << strerror(errno) << std::endl;
            #endif
        }
    
        // Parse the response
        if (bytes_received > 0) {
            std::cout << "Received " << bytes_received << " bytes from "
                      << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
    
            try {
                BencodeParser parser;
                BencodedValue response = parser.parse(std::string(buffer, bytes_received));
                auto& response_dict = response.asDict();
    
                if (response_dict.at("y").asString() == "r") {
                    auto& nodes_str = response_dict.at("r").asDict().at("nodes").asString();
                    parse_compact_nodes(nodes_str, nodes);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing response: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "No response received!" << std::endl;
        }
    
        CLOSESOCKET(sock);
        return nodes;
    }

    void DHTBootstrap::parse_compact_nodes(const std::string& compact, std::vector<Node>& nodes) {
        const char* data = compact.data();
        size_t num_nodes = compact.size() / 26;
    
        for (size_t i = 0; i < num_nodes; ++i) {
            Node node;
            const char* entry = data + i * 26;
    
            // Parse NodeID (20 bytes)
            std::memcpy(node.id.data(), entry, 20);
    
            // Parse IP (4 bytes)
            uint32_t ip_binary;
            std::memcpy(&ip_binary, entry + 20, 4);
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ip_binary, ip_str, sizeof(ip_str));
            node.ip = ip_str;
    
            // Parse port (2 bytes)
            uint16_t port;
            std::memcpy(&port, entry + 24, 2);
            node.port = ntohs(port);
    
            nodes.push_back(node);
        }
    }

    void DHTBootstrap::add_to_routing_table(const Node& node) {
        NodeID distance = xor_distance(my_node_id_, node.id);
        size_t bucket_index = 0;
        
        // Find the appropriate bucket index
        while (bucket_index < routing_table_.size() && (distance[bucket_index / 8] & (1 << (bucket_index % 8)))) {
            bucket_index++;
        }
    
        // If the bucket doesn't exist, create it
        if (bucket_index >= routing_table_.size()) {
            routing_table_.emplace_back();
        }
    
        // Check if the node is already in the bucket
        auto& bucket = routing_table_[bucket_index];
        auto it = std::find(bucket.begin(), bucket.end(), node);
        if (it != bucket.end()) {
            // Move node to the back (most recently seen)
            std::rotate(it, it + 1, bucket.end());
            return;
        }
    
        // If the bucket has space, add the node
        if (bucket.size() < K) {
            bucket.push_back(node);
            return;
        }
    
        // Kademlia eviction rule: Ping the oldest node
        Node& oldest_node = bucket.front();
        if (ping(oldest_node)) {
            // If the oldest node responds, move it to the back
            std::rotate(bucket.begin(), bucket.begin() + 1, bucket.end());
        } else {
            // If the oldest node is unresponsive, replace it
            bucket.front() = node;
        }
    }
    
    bool DHTBootstrap::ping(const Node& node) {
        // Create a UDP socket
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            std::cerr << "Error creating socket!" << std::endl;
            return false;
        }
    
        // Set up the target node's address
        sockaddr_in node_addr{};
        node_addr.sin_family = AF_INET;
        node_addr.sin_port = htons(node.port);
        inet_pton(AF_INET, node.ip.c_str(), &node_addr.sin_addr);
    
        // Ping message
        std::string ping_msg = "PING";
    
        // Send the PING message
        sendto(sock, ping_msg.c_str(), ping_msg.size(), 0,
               (struct sockaddr*)&node_addr, sizeof(node_addr));
    
        // Set socket timeout (2 seconds)
        #ifdef _WIN32
            DWORD timeout = 2000; // 2000 ms 
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        #else
            struct timeval timeout{};
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        #endif
    
        // Receive the PONG response
        char buffer[1024];
        sockaddr_in sender_addr{};
        socklen_t sender_len = sizeof(sender_addr);
        int bytes_received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr*)&sender_addr, &sender_len);
    
        // Close the socket
        CLOSESOCKET(sock);
    
        return (bytes_received > 0); // If data received, the node is alive
    }
    

    std::string node_id_to_hex(const NodeID& id) {
        std::ostringstream oss;
        for (uint8_t byte : id) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return oss.str();
    }

    bool ip_to_binary(const std::string& ip, uint32_t& binary_ip) {
        return inet_pton(AF_INET, ip.c_str(), &binary_ip) == 1;
    }

}
