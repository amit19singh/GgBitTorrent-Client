#ifndef PEER_WIRE_PROTOCOL_HPP
#define PEER_WIRE_PROTOCOL_HPP

// #define MAX_BLOCK_SIZE 16384

#include "../include/torrent_file_parser.hpp"
#include "../include/piece_manager.hpp"
#include "dht_bootstrap.hpp"
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>
#include <sstream>

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
    PeerWireProtocol(const std::string& torrentFilePath);
    ~PeerWireProtocol();

    // Establishes a connection with a peer
    int connectToPeer(const std::string& peerIP, int peerPort);

    // Sends handshake message to initiate the protocol
    void sendHandshake(int peerSocket);

    // Handles an incoming handshake from a peer
    void handleHandshake(int peerSocket);

    // Sends the bitfield message to inform peers about available pieces
    void sendBitfield(int peerSocket, const std::vector<bool>& bitfield);

    // Handles an incoming bitfield message
    void handleBitfield(int peerSocket, const std::vector<uint8_t>& bitfield);

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

    // void setInfoHash(const std::array<uint8_t, 20>& hash) {
    //     info_hash = hash;
    // }

    std::array<uint8_t, 20> getInfoHash() {
        return infoHash;
    }

    std::vector<DHT::Node> queryTracker();

    // Make the following private (public only for testing)
    std::unordered_map<int, std::shared_ptr<PeerConnection>> peers; // Stores peer connections
    TorrentFile torrentFile; 
    std::unique_ptr<PieceManager> pieceStorage;
    DHT::DHTBootstrap* dht_instance;   // Pointer to DHT instance

private:
    // std::unordered_map<int, std::shared_ptr<PeerConnection>> peers; // Stores peer connections
    std::mutex peerMutex; // Synchronization for multi-threading
    // DHT::DHTBootstrap* dht_instance;   // Pointer to DHT instance
    TorrentFileParser torrentFileParser;
    // TorrentFile torrentFile; 
    std::array<uint8_t, 20> infoHash;
    // std::vector<DHT::Node> parseTrackerPeers(const BencodedValue& peersValue);

    std::string computeSHA1(const std::vector<uint8_t>& data);
    // std::string computeSHA1(const std::vector<uint8_t>& data) {
    //     unsigned char hash[SHA_DIGEST_LENGTH];  // SHA-1 produces 20-byte hash
    //     SHA1(data.data(), data.size(), hash);
    
    //     std::ostringstream hashStream;
    //     for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    //         hashStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    //     }
    //     return hashStream.str();
    // }

    std::string rawToHex(const std::string& raw) {
        std::ostringstream oss;
        oss << std::hex << std::setw(2) << std::setfill('0');
        for (unsigned char c : raw) {
            oss << static_cast<int>(c);
        }
        return oss.str();
    }
    

};

#endif // PEER_WIRE_PROTOCOL_HPP

