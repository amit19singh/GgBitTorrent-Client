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

    /**
     * @brief Initialize Winsock for Windows.
     */
    void init_winsock() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize Winsock!" << std::endl;
            exit(1);
        }
    }

    /**
     * @brief Cleanup Winsock for Windows.
     */
    void cleanup_winsock() {
        WSACleanup();
    }

#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>

    // For non-Windows platforms, these are no-ops.
    inline void init_winsock() {}
    inline void cleanup_winsock() {}
#endif

namespace DHT {

    /**
     * @brief Constructor for the DHTBootstrap class. Initializes Winsock (on Windows),
     *        creates a UDP socket, and binds it to the specified DHT port.
     *
     * @param my_node_id The local node's ID.
     */
    DHTBootstrap::DHTBootstrap(const NodeID& my_node_id) : my_node_id_(my_node_id) {
        init_winsock();  // Initialize Winsock on Windows (no-op on other platforms)

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
        local_addr.sin_port = htons(DHT_PORT);     // Use the configured DHT port
        local_addr.sin_addr.s_addr = INADDR_ANY;   // Listen on all interfaces

        if (bind(sock_, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) < 0) {
#ifdef _WIN32
            std::cerr << "Error binding socket! Winsock error: " << WSAGetLastError() << std::endl;
#else
            std::cerr << "Error binding socket! errno: " << strerror(errno) << std::endl;
#endif
            exit(1);
        }

        std::cout << "DHT Node started on Port: " << DHT_PORT << std::endl;

        // Create the first bucket in the routing table.
        routing_table_.emplace_back();
    }

    /**
     * @brief Adds a bootstrap node to the internal list of bootstrap nodes.
     *
     * @param ip   The IP address of the bootstrap node.
     * @param port The UDP port of the bootstrap node.
     */
    void DHTBootstrap::add_bootstrap_node(const std::string& ip, uint16_t port) {
        Node bootstrap_node;
        bootstrap_node.id = generate_random_node_id();
        bootstrap_node.ip = ip;
        bootstrap_node.port = port;
        bootstrap_nodes_.push_back(bootstrap_node);
    }

    /**
     * @brief Perform the bootstrapping process by contacting each known bootstrap node
     *        with a FIND_NODE request for our own node ID.
     */
    void DHTBootstrap::bootstrap() {
        // Re-initialize Winsock on Windows (redundant if constructor already did it, 
        // but kept here to preserve original logic).
        init_winsock();

        for (const auto& bootstrap_node : bootstrap_nodes_) {
            std::cout << "Contacting bootstrap node: " << bootstrap_node.ip 
                      << ":" << bootstrap_node.port << std::endl;

            // Send a FIND_NODE request to the bootstrap node
            auto nodes = send_find_node_request(bootstrap_node, my_node_id_);
            for (const auto& node : nodes) {
                add_to_routing_table(node);
            }
        }

        // Cleanup Winsock on Windows
        cleanup_winsock();
    }

    /**
     * @brief Retrieve the current routing table.
     *
     * @return A const reference to the routing table (vector of buckets).
     */
    const std::vector<Bucket>& DHTBootstrap::get_routing_table() const {
        return routing_table_;
    }

    /**
     * @brief Retrieve the list of bootstrap nodes added via add_bootstrap_node().
     *
     * @return A vector of Node structs for all known bootstrap nodes.
     */
    const std::vector<Node> DHTBootstrap::get_bootstrap_nodes() {
        return bootstrap_nodes_;
    }

    /**
     * @brief Generate a random 20-byte (160-bit) Node ID.
     *
     * @return A randomly generated NodeID.
     */
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

    /**
     * @brief Compute the XOR distance between two Node IDs.
     *
     * @param a First NodeID.
     * @param b Second NodeID.
     *
     * @return The XOR distance as a NodeID.
     */
    NodeID DHTBootstrap::xor_distance(const NodeID& a, const NodeID& b) {
        NodeID result;
        for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
            result[i] = a[i] ^ b[i];
        }
        return result;
    }

    /**
     * @brief Send a FIND_NODE request to a remote node for a given target_id. 
     *        Parse the response (if any) and return the list of nodes included.
     *
     * @param remote_node The node to which the FIND_NODE request will be sent.
     * @param target_id   The NodeID being searched for.
     *
     * @return A list of Node objects parsed from the response.
     */
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

        // Set socket timeout (2 seconds)
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

        std::cout << "Sending FIND_NODE request to: " 
                  << remote_node.ip << ":" << remote_node.port << std::endl;

        // Create the request message using bencode
        BencodedDict query;
        query["id"]     = BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20));
        query["target"] = BencodedValue(std::string(reinterpret_cast<const char*>(target_id.data()), 20));

        BencodedDict message;
        message["t"] = BencodedValue("aa");  // Transaction ID
        message["y"] = BencodedValue("q");   // Message type (query)
        message["q"] = BencodedValue("find_node");
        message["a"] = BencodedValue(query);

        std::string request = BencodeEncoder::encode(message);

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

        // Parse the response if we got any data
        if (bytes_received > 0) {
            std::cout << "Received " << bytes_received << " bytes from "
                      << inet_ntoa(sender_addr.sin_addr) << ":"
                      << ntohs(sender_addr.sin_port) << std::endl;

            try {
                BencodeParser parser;
                BencodedValue response = parser.parse(std::string(buffer, bytes_received));
                auto& response_dict = response.asDict();

                // If this is a response message ("y": "r"), parse out the nodes.
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

    /**
     * @brief Parse a compact node info string (from a FIND_NODE response) into
     *        a vector of Node objects.
     *
     * @param compact The compact node info string.
     * @param nodes   [out] The vector in which parsed nodes will be stored.
     */
    void DHTBootstrap::parse_compact_nodes(const std::string& compact, std::vector<Node>& nodes) {
        const char* data = compact.data();
        size_t num_nodes = compact.size() / 26; // Each node entry is 26 bytes: 20 for ID, 4 for IP, 2 for port

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

    /**
     * @brief Add a node to the routing table. If the corresponding bucket is full,
     *        use the Kademlia eviction rule (ping the oldest node, replace if dead).
     *
     * @param node The node to add.
     */
    void DHTBootstrap::add_to_routing_table(const Node& node) {
        NodeID distance = xor_distance(my_node_id_, node.id);
        size_t bucket_index = 0;

        // Find the appropriate bucket index based on XOR distance bits
        while (bucket_index < routing_table_.size() && 
               (distance[bucket_index / 8] & (1 << (bucket_index % 8)))) {
            bucket_index++;
        }

        // If the bucket doesn't exist, create it
        if (bucket_index >= routing_table_.size()) {
            routing_table_.emplace_back();
        }

        auto& bucket = routing_table_[bucket_index];
        // Check if the node is already in the bucket
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

    /**
     * @brief Ping a node to check if it's alive (simple UDP send + receive).
     *
     * @param node The node to ping.
     *
     * @return True if the node responded, false otherwise.
     */
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
        // std::string ping_msg = "PING";
        // Create a valid DHT PING query
        BencodedDict query;
        query["id"] = BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20));

        BencodedDict message;
        message["t"] = BencodedValue("pp"); // Transaction ID
        message["y"] = BencodedValue("q");  // Query
        message["q"] = BencodedValue("ping");
        message["a"] = BencodedValue(query);

        std::string ping_msg = BencodeEncoder::encode(message);

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

        CLOSESOCKET(sock);
        return (bytes_received > 0); // If data received, the node is alive
    }

    /**
     * @brief Handle an incoming "ping" query and send back a "pong" response.
     *
     * @param request     The parsed Bencoded request.
     * @param sender_addr The sockaddr of the sender (to reply).
     */
    void DHTBootstrap::handle_ping(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract transaction ID
            std::string transaction_id = request.asDict().at("t").asString();

            // Create the pong response
            BencodedDict response;
            response["t"] = BencodedValue(transaction_id); // Same transaction ID
            response["y"] = BencodedValue("r");            // Response type
            response["r"] = BencodedValue(BencodedDict{
                {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))}
            });

            // Encode and send response
            std::string response_str = BencodeEncoder::encode(response);
            sendto(sock_, response_str.c_str(), response_str.size(), 0,
                   reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));

            std::cout << "Sent PONG response to: "
                      << inet_ntoa(sender_addr.sin_addr) << ":" 
                      << ntohs(sender_addr.sin_port) << std::endl;
            std::cout << "PONG RESPONSE: \n" << response_str << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling ping request: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Utility function to convert a NodeID to a hex string.
     *
     * @param id The NodeID.
     *
     * @return Hexadecimal representation of the NodeID.
     */
    std::string node_id_to_hex(const NodeID& id) {
        std::ostringstream oss;
        for (uint8_t byte : id) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return oss.str();
    }

    /**
     * @brief Handle an incoming "find_node" query. Respond with the closest known nodes.
     *
     * @param request     The parsed Bencoded request.
     * @param sender_addr The sockaddr of the sender (to reply).
     */
    void DHTBootstrap::handle_find_node(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract transaction ID
            std::string transaction_id = request.asDict().at("t").asString();

            // Extract target ID
            std::string target_id_str = request.asDict().at("a").asDict().at("target").asString();
            NodeID target_id;
            std::memcpy(target_id.data(), target_id_str.data(), NODE_ID_SIZE);

            // Find the K closest nodes
            std::vector<Node> closest_nodes = find_closest_nodes(target_id, K);

            // Create the response
            BencodedDict response;
            response["t"] = BencodedValue(transaction_id);  // Same transaction ID
            response["y"] = BencodedValue("r");             // Response type
            response["r"] = BencodedValue(BencodedDict{
                {"id",    BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))},
                {"nodes", BencodedValue(encode_nodes(closest_nodes))}
            });

            // Encode and send response
            std::string response_str = BencodeEncoder::encode(response);
            sendto(sock_, response_str.c_str(), response_str.size(), 0,
                   reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));

            std::cout << "************Sent FIND_NODE response to: "
                      << inet_ntoa(sender_addr.sin_addr) << ":" << ntohs(sender_addr.sin_port) 
                      << std::endl
                      << "Response sent: " << response_str << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling find_node request: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Find the K closest nodes in the routing table to a given target ID.
     *
     * @param target_id The NodeID we want to find.
     * @param k         The maximum number of closest nodes to return.
     *
     * @return A vector of up to K closest Node objects.
     */
    std::vector<Node> DHTBootstrap::find_closest_nodes(const NodeID& target_id, size_t k) {
        std::vector<Node> closest_nodes;

        // Gather all nodes from all buckets
        for (const auto& bucket : routing_table_) {
            for (const auto& node : bucket) {
                closest_nodes.push_back(node);
            }
        }

        // Sort nodes by XOR distance to the target ID
        std::sort(closest_nodes.begin(), closest_nodes.end(), [&](const Node& a, const Node& b) {
            return xor_distance(a.id, target_id) < xor_distance(b.id, target_id);
        });

        // Return the K closest
        if (closest_nodes.size() > k) {
            closest_nodes.resize(k);
        }
        return closest_nodes;
    }

    /**
     * @brief Encode a vector of Node objects into the compact node format.
     *
     * @param nodes A list of Node objects.
     *
     * @return A string containing the compact representation of these nodes.
     */
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

    /**
     * @brief Encode a list of peers into compact format (IP + Port).
     *
     * @param peers A list of Node objects representing peers.
     *
     * @return A compact string representation of these peers.
     */
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

    /**
     * @brief Handle an incoming "get_peers" query. If we know peers for the given infohash,
     *        return them; otherwise, return the K closest nodes.
     *
     * @param request     The parsed Bencoded request.
     * @param sender_addr The sockaddr of the sender (to reply).
     */
    void DHTBootstrap::handle_get_peers(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract transaction ID
            std::string transaction_id = request.asDict().at("t").asString();

            // Extract infohash
            std::string infohash = request.asDict().at("a").asDict().at("info_hash").asString();

            // Check if peers are available for the infohash
            auto it = peer_store_.find(infohash);
            if (it != peer_store_.end()) {
                // We have peers for this infohash
                BencodedDict response;
                response["t"] = BencodedValue(transaction_id); // Same transaction ID
                response["y"] = BencodedValue("r");            // Response type
                response["r"] = BencodedValue(BencodedDict{
                    {"id",     BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))},
                    {"values", BencodedValue(encode_peers(it->second))}
                });

                std::string response_str = BencodeEncoder::encode(response);
                sendto(sock_, response_str.c_str(), response_str.size(), 0,
                       reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));

                std::cout << "Sent GET_PEERS response (peers) to: "
                          << inet_ntoa(sender_addr.sin_addr) << ":" 
                          << ntohs(sender_addr.sin_port) << std::endl;
            } else {
                // Return the K closest nodes
                NodeID target_id;
                std::memcpy(target_id.data(), infohash.data(), NODE_ID_SIZE);

                std::vector<Node> closest_nodes = find_closest_nodes(target_id, K);

                BencodedDict response;
                response["t"] = BencodedValue(transaction_id); // Same transaction ID
                response["y"] = BencodedValue("r");            // Response type
                response["r"] = BencodedValue(BencodedDict{
                    {"id",    BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))},
                    {"nodes", BencodedValue(encode_nodes(closest_nodes))}
                });

                std::string response_str = BencodeEncoder::encode(response);
                sendto(sock_, response_str.c_str(), response_str.size(), 0,
                       reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));

                std::cout << "Sent GET_PEERS response (nodes) to: "
                          << inet_ntoa(sender_addr.sin_addr) << ":" 
                          << ntohs(sender_addr.sin_port) << std::endl;
                std::cout << "RESPONSE STRING - GET PEERS:\n" << response_str << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling get_peers request: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Convert a std::string of length 20 into a NodeID (20-byte array).
     *
     * @param str The 20-byte string to be converted.
     *
     * @return A NodeID equivalent to the string.
     */
    NodeID DHTBootstrap::string_to_node_id(const std::string& str) {
        NodeID node_id;
        if (str.size() != NODE_ID_SIZE) {
            throw std::runtime_error("Invalid string length for NodeID");
        }
        std::memcpy(node_id.data(), str.data(), NODE_ID_SIZE);
        return node_id;
    }

    /**
     * @brief Handle an incoming "announce_peer" query. Store the announcing peer
     *        in the peer_store_ under the given infohash.
     *
     * @param request     The parsed Bencoded request.
     * @param sender_addr The sockaddr of the sender (to reply).
     */
    void DHTBootstrap::handle_announce_peer(const BencodedValue& request, const sockaddr_in& sender_addr) {
        try {
            // Extract infohash
            std::string infohash = request.asDict().at("a").asDict().at("info_hash").asString();

            // Build Node struct for the peer
            Node peer;
            peer.ip   = inet_ntoa(sender_addr.sin_addr);
            peer.port = ntohs(sender_addr.sin_port);

            // Store the peer information
            peer_store_[infohash].push_back(peer);

            // Log the announcement
            std::cout << "Stored peer " << peer.ip << ":" << peer.port
                      << " for infohash "
                      << node_id_to_hex(string_to_node_id(infohash)) << std::endl;

            // Send a response
            BencodedDict response;
            response["t"] = BencodedValue(request.asDict().at("t").asString()); // Same transaction ID
            response["y"] = BencodedValue("r");                                // Response type
            response["r"] = BencodedValue(BencodedDict{
                {"id", BencodedValue(std::string(reinterpret_cast<const char*>(my_node_id_.data()), 20))}
            });

            std::string response_str = BencodeEncoder::encode(response);
            sendto(sock_, response_str.c_str(), response_str.size(), 0,
                   reinterpret_cast<const sockaddr*>(&sender_addr), sizeof(sender_addr));

            std::cout << "Sent ANNOUNCE_PEER response to: "
                      << inet_ntoa(sender_addr.sin_addr) << ":"
                      << ntohs(sender_addr.sin_port) << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling announce_peer request: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Main loop that listens for incoming DHT messages and dispatches them
     *        to the appropriate handler functions (ping, find_node, get_peers, announce_peer).
     */
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

            std::cout << "[DHT] Received " << bytes_received << " bytes from "
                      << inet_ntoa(sender_addr.sin_addr) << ":"
                      << ntohs(sender_addr.sin_port) << std::endl;

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
                        std::cout << "[DHT] Handling ANNOUNCE_PEER request" << std::endl;
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

} // namespace DHT
