// #ifndef DHT_BOOTSTRAP_HPP
// #define DHT_BOOTSTRAP_HPP

// #include <vector>
// #include <array>
// #include <iostream>

// namespace DHT {

//     // Constants
//     constexpr uint16_t DHT_PORT = 6881; // Default DHT port
//     constexpr size_t NODE_ID_SIZE = 20; // 160-bit Node ID (20 bytes)
//     constexpr size_t K = 8; // Kademlia bucket size

//     // Node ID type (160-bit hash)
//     using NodeID = std::array<uint8_t, NODE_ID_SIZE>;

//     // Node information
//     struct Node {
//         NodeID id;
//         std::string ip;
//         uint16_t port;

//         bool operator==(const Node& other) const {
//             return id == other.id && ip == other.ip && port == other.port;
//         }
//     };

//     // Routing table bucket
//     using Bucket = std::vector<Node>;

//     // DHT Bootstrap class
//     class DHTBootstrap {
//     public:
//         // Constructor
//         DHTBootstrap(const NodeID& my_node_id);

//         // Add a bootstrap node
//         void add_bootstrap_node(const std::string& ip, uint16_t port);

//         // Start the bootstrapping process
//         void bootstrap();

//         // Get the routing table
//         const std::vector<Bucket>& get_routing_table() const;

//     private:
//         // Generate a random Node ID
//         static NodeID generate_random_node_id();

//         // XOR distance between two Node IDs
//         static NodeID xor_distance(const NodeID& a, const NodeID& b);

//         // Send a find_node request to a remote node
//         std::vector<Node> send_find_node_request(const Node& remote_node, const NodeID& target_id);

//         // Add a node to the routing table
//         void add_to_routing_table(const Node& node);

//         // My Node ID
//         NodeID my_node_id_;

//         // Routing table (list of buckets)
//         std::vector<Bucket> routing_table_;

//         // List of bootstrap nodes
//         std::vector<Node> bootstrap_nodes_;
//     };

//     // Helper functions

//     // Convert Node ID to a hex string
//     std::string node_id_to_hex(const NodeID& id);

//     // Convert IP address string to binary format
//     bool ip_to_binary(const std::string& ip, uint32_t& binary_ip);

// } // namespace DHT

// #endif // DHT_BOOTSTRAP_HPP


#ifndef DHT_BOOTSTRAP_HPP
#define DHT_BOOTSTRAP_HPP

#include <vector>
#include <array>
#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>  // For inet_pton, inet_ntop
    #pragma comment(lib, "ws2_32.lib")  // Link against Winsock library
    #define CLOSESOCKET closesocket
    void init_winsock();
    void cleanup_winsock();
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #define CLOSESOCKET close
    inline void init_winsock() {}  // No-op for Linux/macOS
    inline void cleanup_winsock() {}  // No-op for Linux/macOS
#endif

namespace DHT {

    constexpr uint16_t DHT_PORT = 6881;
    constexpr size_t NODE_ID_SIZE = 20;
    constexpr size_t K = 8;

    using NodeID = std::array<uint8_t, NODE_ID_SIZE>;

    struct Node {
        NodeID id;
        std::string ip;
        uint16_t port;

        bool operator==(const Node& other) const {
            return id == other.id && ip == other.ip && port == other.port;
        }
    };

    using Bucket = std::vector<Node>;

    class DHTBootstrap {
    public:
        DHTBootstrap(const NodeID& my_node_id);
        void add_bootstrap_node(const std::string& ip, uint16_t port);
        void bootstrap();
        const std::vector<Bucket>& get_routing_table() const;
        const std::vector<Node> get_bootstrap_nodes();
        static NodeID generate_random_node_id();
        std::vector<Node> send_find_node_request(const Node& remote_node, const NodeID& target_id);

    private:
        // static NodeID generate_random_node_id();
        static NodeID xor_distance(const NodeID& a, const NodeID& b);
        // std::vector<Node> send_find_node_request(const Node& remote_node, const NodeID& target_id);
        void add_to_routing_table(const Node& node);
        bool ping(const Node& node);
        void parse_compact_nodes(const std::string& compact, std::vector<Node>& nodes);

        NodeID my_node_id_;
        std::vector<Bucket> routing_table_;
        std::vector<Node> bootstrap_nodes_;
    };

    std::string node_id_to_hex(const NodeID& id);
    bool ip_to_binary(const std::string& ip, uint32_t& binary_ip);

} // namespace DHT

#endif // DHT_BOOTSTRAP_HPP

