#ifndef PEER_CONNECTION_HPP
#define PEER_CONNECTION_HPP

#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <chrono>
#include <deque>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") // Link Windows Sockets library
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

class PeerConnection {
public:
    // Thread safe Connection state flags
    std::atomic<bool> choked_by_peer{true};
    std::atomic<bool> choked_by_us{true};
    std::atomic<bool> interested{false};
    std::atomic<bool> peer_interested{false};

    // BitTorrent protocol data
    std::vector<bool> bitfield;
    std::array<uint8_t, 20> info_hash;
    std::array<uint8_t, 20> peer_id;

    // Network buffers
    std::vector<uint8_t> input_buffer;
    std::vector<uint8_t> output_buffer;
    std::mutex buffer_mutex;
    
    // Statistics
    std::atomic<double> download_rate{0.0};
    std::atomic<double> upload_rate{0.0};
    std::chrono::time_point<std::chrono::steady_clock> last_activity;

    // Message queue
    std::deque<std::vector<uint8_t>> message_queue;
    std::mutex queue_mutex;

    // Socket descriptor
    int socket = -1;

    // Message handling
    void send_choke();
    void send_unchoke();
    void send_interested();
    void send_not_interested();
    void handle_message(const std::vector<uint8_t>& message);
    void update_rate_counters(size_t downloaded, size_t uploaded);
    // Buffer management
    void append_to_output(const std::vector<uint8_t>& data);
    void process_input_buffer();

private:
    // Protocol message IDs
    enum MessageType {
        CHOKE = 0,
        UNCHOKE,
        INTERESTED,
        NOT_INTERESTED,
        HAVE,
        BITFIELD,
        REQUEST,
        PIECE,
        CANCEL
    };

    // Message construction helpers
    void create_message_header(MessageType type, uint32_t length = 0);
    void create_request_message(uint32_t piece_index, uint32_t block_offset, uint32_t block_length);

};
#endif // PEER_CONNECTION_HPP
