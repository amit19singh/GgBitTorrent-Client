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

    DHTBootstrap::DHTBootstrap(const NodeID& my_node_id): my_node_id_(my_node_id) {
        init_winsock();

        // Create UDP socket
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) {
    #ifdef _WIN32
            std::cerr << "Error creating socket! Winsock error: " << WSAGetLastError() << std::endl;
    #else
            std::cerr << "Error creating socket! errno: " << strerror(errno) << std::endl;
    #endif
            exit(1);
        }

        // Bind the socket to a port
        sockaddr_in local_addr{};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(DHT_PORT); // Use port 6881 (or any other port)
        local_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces

        if (bind(sock_, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) < 0) {
    #ifdef _WIN32
            std::cerr << "Error binding socket! Winsock error: " << WSAGetLastError() << std::endl;
    #else
            std::cerr << "Error binding socket! errno: " << strerror(errno) << std::endl;
    #endif
            exit(1);
        }
        std::cout << "DHT Node started on Port: " << DHT_PORT << std::endl;
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
        init_winsock();  // Initialize Winsock on Windows

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

    void DHTBootstrap::handle_ping(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract transaction ID
            std::string transaction_id = request.asDict().at("t").asString();
    
            // Extract requester's node ID
            std::string requester_id = request.asDict().at("a").asDict().at("id").asString();
    
            // Create the pong response
            BencodedDict response;
            response["t"] = BencodedValue(transaction_id); // Same transaction ID
            response["y"] = BencodedValue("r");           // Response type
            response["r"] = BencodedValue(BencodedDict{
                {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))}
            });
    
            // Encode the response
            std::string response_str = BencodeEncoder::encode(response);
    
            // Send the response back to the requester
            sendto(sock_, response_str.c_str(), response_str.size(), 0,
                   reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));
            // Log the response
            std::cout << "Sent PONG response to: "
                    << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling ping request: " << e.what() << std::endl;
        }
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

    void DHTBootstrap::handle_find_node(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract transaction ID
            std::string transaction_id = request.asDict().at("t").asString();
    
            // Extract target ID
            std::string target_id_str = request.asDict().at("a").asDict().at("target").asString();
            NodeID target_id;
            std::memcpy(target_id.data(), target_id_str.data(), NODE_ID_SIZE);
    
            // Find the K closest nodes to the target ID
            std::vector<Node> closest_nodes = find_closest_nodes(target_id, K);
    
            // Create the response
            BencodedDict response;
            response["t"] = BencodedValue(transaction_id); // Same transaction ID
            response["y"] = BencodedValue("r");           // Response type
            response["r"] = BencodedValue(BencodedDict{
                {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))},
                {"nodes", BencodedValue(encode_nodes(closest_nodes))}
            });
    
            // Encode the response
            std::string response_str = BencodeEncoder::encode(response);
    
            // Send the response back to the requester
            sendto(sock_, response_str.c_str(), response_str.size(), 0,
                   reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));
    
            // Log the response
            std::cout << "************Sent FIND_NODE response to: "
                      << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
            std::cout << "Response sent: " << response_str << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling find_node request: " << e.what() << std::endl;
        }
    }

    std::vector<Node> DHTBootstrap::find_closest_nodes(const NodeID& target_id, size_t k) {
        std::vector<Node> closest_nodes;
        for (const auto& bucket : routing_table_) {
            for (const auto& node : bucket) {
                closest_nodes.push_back(node);
            }
        }
    
        // Sort nodes by XOR distance to the target ID
        std::sort(closest_nodes.begin(), closest_nodes.end(), [&](const Node& a, const Node& b) {
            return xor_distance(a.id, target_id) < xor_distance(b.id, target_id);
        });
    
        // Return the K closest nodes
        if (closest_nodes.size() > k) {
            closest_nodes.resize(k);
        }
        return closest_nodes;
    }

    std::string DHTBootstrap::encode_nodes(const std::vector<Node>& nodes) {
        std::string result;
        for (const auto& node : nodes) {
            // Node ID (20 bytes)
            result.append(reinterpret_cast<const char*>(node.id.data()), 20);
    
            // IP address (4 bytes)
            uint32_t ip_binary;
            inet_pton(AF_INET, node.ip.c_str(), &ip_binary);
            result.append(reinterpret_cast<const char*>(&ip_binary), 4);
    
            // Port (2 bytes, network byte order)
            uint16_t port_network = htons(node.port);
            result.append(reinterpret_cast<const char*>(&port_network), 2);
        }
        return result;
    }

    std::string DHTBootstrap::encode_peers(const std::vector<Node>& peers) {
        std::string result;
        for (const auto& peer : peers) {
            // IP address (4 bytes)
            uint32_t ip_binary;
            inet_pton(AF_INET, peer.ip.c_str(), &ip_binary);
            result.append(reinterpret_cast<const char*>(&ip_binary), 4);
    
            // Port (2 bytes, network byte order)
            uint16_t port_network = htons(peer.port);
            result.append(reinterpret_cast<const char*>(&port_network), 2);
        }
        return result;
    }

    void DHTBootstrap::handle_get_peers(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract transaction ID
            std::string transaction_id = request.asDict().at("t").asString();
    
            // Extract infohash
            std::string infohash = request.asDict().at("a").asDict().at("info_hash").asString();
    
            // Check if peers are available for the infohash
            auto it = peer_store_.find(infohash);
            if (it != peer_store_.end()) {
                // Return peers
                BencodedDict response;
                response["t"] = BencodedValue(transaction_id); // Same transaction ID
                response["y"] = BencodedValue("r");           // Response type
                response["r"] = BencodedValue(BencodedDict{
                    {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))},
                    {"values", BencodedValue(encode_peers(it->second))}
                });
    
                // Encode the response
                std::string response_str = BencodeEncoder::encode(response);
    
                // Send the response back to the requester
                sendto(sock_, response_str.c_str(), response_str.size(), 0,
                       reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));
    
                // Log the response
                std::cout << "Sent GET_PEERS response (peers) to: "
                          << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
            } else {
                // Return the K closest nodes
                NodeID target_id;
                std::memcpy(target_id.data(), infohash.data(), NODE_ID_SIZE);
                std::vector<Node> closest_nodes = find_closest_nodes(target_id, K);
    
                BencodedDict response;
                response["t"] = BencodedValue(transaction_id); // Same transaction ID
                response["y"] = BencodedValue("r");           // Response type
                response["r"] = BencodedValue(BencodedDict{
                    {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))},
                    {"nodes", BencodedValue(encode_nodes(closest_nodes))}
                });
    
                // Encode the response
                std::string response_str = BencodeEncoder::encode(response);
    
                // Send the response back to the requester
                sendto(sock_, response_str.c_str(), response_str.size(), 0,
                       reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));
    
                // Log the response
                std::cout << "Sent GET_PEERS response (nodes) to: "
                          << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling get_peers request: " << e.what() << std::endl;
        }
    }

    NodeID DHTBootstrap::string_to_node_id(const std::string& str) {
        NodeID node_id;
        if (str.size() != NODE_ID_SIZE) {
            throw std::runtime_error("Invalid string length for NodeID");
        }
        std::memcpy(node_id.data(), str.data(), NODE_ID_SIZE);
        return node_id;
    }

    void DHTBootstrap::handle_announce_peer(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract infohash
            std::string infohash = request.asDict().at("a").asDict().at("info_hash").asString();
    
            // Extract peer information
            Node peer;
            peer.ip = inet_ntoa(sender_addr.sin_addr);
            peer.port = ntohs(sender_addr.sin_port);
    
            // Store the peer information
            peer_store_[infohash].push_back(peer);
    
            // Log the announcement
            std::cout << "Stored peer " << peer.ip << ":" << peer.port
                      << " for infohash " << node_id_to_hex(string_to_node_id(infohash)) << std::endl;
    
            // Send a response
            BencodedDict response;
            response["t"] = BencodedValue(request.asDict().at("t").asString()); // Same transaction ID
            response["y"] = BencodedValue("r");                                // Response type
            response["r"] = BencodedValue(BencodedDict{
                {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))}
            });
    
            // Encode the response
            std::string response_str = BencodeEncoder::encode(response);
    
            // Send the response back to the requester
            sendto(sock_, response_str.c_str(), response_str.size(), 0,
                   reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));
    
            // Log the response
            std::cout << "Sent ANNOUNCE_PEER response to: "
                      << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling announce_peer request: " << e.what() << std::endl;
        }
    }
    
    void DHTBootstrap::run() {
        char buffer[1024];
        sockaddr_in sender_addr{};
        socklen_t sender_len = sizeof(sender_addr);
    
        while (true) {
            // Receive a message
            int bytes_received = recvfrom(sock_, buffer, sizeof(buffer), 0,
                                          reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);
            if (bytes_received < 0) {
    #ifdef _WIN32
                int wsa_error = WSAGetLastError();
                std::cerr << "recvfrom failed! WSA Error Code: " << wsa_error << std::endl;
    #else
                std::cerr << "recvfrom failed! errno: " << strerror(errno) << std::endl;
    #endif
                continue;
            }
    
            // Log received bytes and sender details
            std::cout << "[DHT] Received " << bytes_received << " bytes from "
                      << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) << std::endl;
    
            // Log raw message in hex format
            std::cout << "[DHT] Raw Data: ";
            for (int i = 0; i < bytes_received; i++) {
                printf("%02x ", (unsigned char)buffer[i]);
            }
            std::cout << std::endl;
    
            // Parse the message
            try {
                BencodeParser parser;
                std::string message_str(buffer, bytes_received);
                BencodedValue message = parser.parse(message_str);
    
                // Log Parsed Message (JSON-like format)
                std::cout << "[DHT] Parsed Message: " << message_str << std::endl;
    
                // Extract the message type
                std::string message_type = message.asDict().at("y").asString();
    
                if (message_type == "q") {  // Query message
                    std::string query_type = message.asDict().at("q").asString();
                    std::cout << "[DHT] Query Type: " << query_type << std::endl;
    
                    if (query_type == "ping") {
                        std::cout << "[DHT] Handling PING request from " 
                                  << inet_ntoa(sender_addr.sin_addr) << ":" 
                                  << ntohs(sender_addr.sin_port) << std::endl;
                        handle_ping(message, sender_addr);
                    } else if (query_type == "find_node") {
                        std::cout << "[DHT] Handling FIND_NODE request" << std::endl;
                        handle_find_node(message, sender_addr);
                    } else if (query_type == "get_peers") {
                        std::cout << "[DHT] Handling GET_PEERS request" << std::endl;
                        handle_get_peers(message, sender_addr);
                    } else if (query_type == "announce_peer") {
                        std::cout << "[DHT] Handling ANNOUNCE_PEERS request" << std::endl;
                        handle_announce_peer(message, sender_addr);
                    }
                } else if (message_type == "r") {  // Response message
                    std::cout << "[DHT] Received RESPONSE message" << std::endl;
                } else if (message_type == "e") {  // Error message
                    std::cout << "[DHT] Received ERROR message" << std::endl;
                }
    
            } catch (const std::exception& e) {
                std::cerr << "[DHT] Error parsing message: " << e.what() << std::endl;
            }
        }
    }

}
