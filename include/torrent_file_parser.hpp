#ifndef TORRENT_FILE_PARSER_HPP
#define TORRENT_FILE_PARSER_HPP

#include "bencode_parser.hpp"
#include <openssl/sha.h>
#include <array>
#include <string>
#include <vector>
#include <utility> // for std::pair

struct TorrentFile {
    std::string announce; // Tracker URL
    std::string comment;  // Optional comment
    int64_t creationDate; // Creation timestamp
    std::string name;     // File name (for single-file) or directory name (for multi-file)
    int64_t pieceLength;  // Size of each piece
    int numPieces;        // Number of pieces

    std::array<uint8_t, 20> infoHash; // Stores the torrent's info hash
    std::vector<std::string> pieces; // SHA-1 hashes of pieces
    std::vector<std::pair<std::string, int64_t>> files; // File list (for multi-file torrents)
};

class TorrentFileParser {
public:
    explicit TorrentFileParser(const std::string& filePath);

    // Parse the .torrent file and return a TorrentFile struct
    TorrentFile parse();
    const int getNumPieces(); 

private:
    std::string filePath;
    BencodeParser bencodeParser;
    TorrentFile parsedTorrent;

    // Helper functions to extract data from the Bencoded dictionary
    std::string extractString(const BencodedValue& dict, const std::string& key);
    int64_t extractInt(const BencodedValue& dict, const std::string& key);
    std::vector<std::string> extractPieces(const BencodedValue& dict);
    std::vector<std::pair<std::string, int64_t>> extractFiles(const BencodedValue& dict);

    std::array<uint8_t, 20> computeSHA1(const std::string& data) {
        std::array<uint8_t, 20> hash{};
        SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash.data());
        return hash;
    }
};

#endif // TORRENT_FILE_PARSER_HPP
