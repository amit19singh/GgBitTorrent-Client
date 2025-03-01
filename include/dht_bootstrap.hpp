#ifndef DHT_BOOTSTRAP_HPP
#define DHT_BOOTSTRAP_HPP

#include "bencode_parser.hpp"
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
    static void init_winsock();
    static void cleanup_winsock();
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
        // ~DHTBootstrap();
        void add_bootstrap_node(const std::string& ip, uint16_t port);
        void bootstrap();
        const std::vector<Bucket>& get_routing_table() const;
        const std::vector<Node> get_bootstrap_nodes();
        static NodeID generate_random_node_id();
        std::vector<Node> send_find_node_request(const Node& remote_node, const NodeID& target_id);
        void run(); // Main loop for handling incoming messages

        const NodeID& getMyNodeId() const {
            return my_node_id_;
        }


    private:
        int sock_;
        // static NodeID generate_random_node_id();
        static NodeID xor_distance(const NodeID& a, const NodeID& b);
        // std::vector<Node> send_find_node_request(const Node& remote_node, const NodeID& target_id);
        void add_to_routing_table(const Node& node);
        void parse_compact_nodes(const std::string& compact, std::vector<Node>& nodes);
        bool ping(const Node& node);
        void handle_ping(const BencodedValue& request, const sockaddr_in& sender_addr);
        std::vector<Node> find_closest_nodes(const NodeID& target_id, size_t k);
        std::string encode_nodes(const std::vector<Node>& nodes);
        std::string encode_peers(const std::vector<Node>& peers);
        void handle_find_node(const BencodedValue& request, const sockaddr_in& sender_addr);
        void handle_get_peers(const BencodedValue& request, const sockaddr_in& sender_addr);
        void handle_announce_peer(const BencodedValue& request, const sockaddr_in& sender_addr);
        NodeID string_to_node_id(const std::string& str);

        NodeID my_node_id_;
        std::vector<Bucket> routing_table_;
        std::vector<Node> bootstrap_nodes_;
        std::map<std::string, std::vector<Node>> peer_store_; // Infohash -> List of peers
    };

    std::string node_id_to_hex(const NodeID& id);
    bool ip_to_binary(const std::string& ip, uint32_t& binary_ip);

} // namespace DHT

#endif // DHT_BOOTSTRAP_HPP

