#ifndef PEER_WIRE_PROTOCOL_HPP
#define PEER_WIRE_PROTOCOL_HPP

#include "dht_bootstrap.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") // Link Windows Sockets library
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif


// Protocol constants
constexpr uint8_t HANDSHAKE_PROTOCOL_LEN = 19;
constexpr char HANDSHAKE_PROTOCOL_STR[] = "BitTorrent protocol";
constexpr int MESSAGE_LENGTH_SIZE = 4;

// Forward declaration
class PeerConnection;

class PeerWireProtocol {
public:
    // Constructor & Destructor
    PeerWireProtocol();
    ~PeerWireProtocol();

    // Establishes a connection with a peer
    void connectToPeer(const std::string& peerIP, int peerPort);

    // Sends handshake message to initiate the protocol
    void sendHandshake(int peerSocket);

    // Handles an incoming handshake from a peer
    void handleHandshake(int peerSocket);

    // Sends the bitfield message to inform peers about available pieces
    void sendBitfield(int peerSocket, const std::vector<bool>& bitfield);

    // Handles an incoming bitfield message
    void handleBitfield(int peerSocket, const std::vector<bool>& bitfield);

    // Sends a request for a specific piece
    void sendRequest(int peerSocket, int pieceIndex, int blockOffset, int blockSize);

    // Handles an incoming request for a piece
    void handleRequest(int peerSocket, int pieceIndex, int blockOffset, int blockSize);

    // Sends a piece to a peer in response to a request
    void sendPiece(int peerSocket, int pieceIndex, int blockOffset, const std::vector<uint8_t>& blockData);

    // Handles an incoming piece message
    void handlePiece(int peerSocket, int pieceIndex, int blockOffset, const std::vector<uint8_t>& blockData);

    // Implements choking logic (tit-for-tat)
    void manageChoking();

    // Periodically unchokes a random peer (Optimistic Unchoking)
    void optimisticUnchoke();

    void handlePeerInput(int sock);
    void handlePeerOutput(int sock);

    // Runs the PWP event loop
    void run();

    void setInfoHash(const std::array<uint8_t, 20>& hash) {
        info_hash = hash;
    }

    std::array<uint8_t, 20> getInfoHash() {
        return info_hash;
    }

private:
    std::unordered_map<int, std::shared_ptr<PeerConnection>> peers; // Stores peer connections
    std::mutex peerMutex; // Synchronization for multi-threading
    std::array<uint8_t, 20> info_hash; // Stores the torrent's info hash
    DHT::DHTBootstrap* dht_instance;   // Pointer to DHT instance
};

#endif // PEER_WIRE_PROTOCOL_HPP
