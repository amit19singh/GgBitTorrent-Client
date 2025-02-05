#ifndef TORRENT_FILE_PARSER_HPP
#define TORRENT_FILE_PARSER_HPP

#include "bencode_parser.hpp"
#include <string>
#include <vector>
#include <utility> // for std::pair

struct TorrentFile {
    std::string announce; // Tracker URL
    std::string comment;  // Optional comment
    int64_t creationDate; // Creation timestamp
    std::string name;     // File name (for single-file) or directory name (for multi-file)
    int64_t pieceLength;  // Size of each piece
    std::vector<std::string> pieces; // SHA-1 hashes of pieces
    std::vector<std::pair<std::string, int64_t>> files; // File list (for multi-file torrents)
};

class TorrentFileParser {
public:
    explicit TorrentFileParser(const std::string& filePath);

    // Parse the .torrent file and return a TorrentFile struct
    TorrentFile parse();

private:
    std::string filePath;
    BencodeParser bencodeParser;

    // Helper functions to extract data from the Bencoded dictionary
    std::string extractString(const BencodedValue& dict, const std::string& key);
    int64_t extractInt(const BencodedValue& dict, const std::string& key);
    std::vector<std::string> extractPieces(const BencodedValue& dict);
    std::vector<std::pair<std::string, int64_t>> extractFiles(const BencodedValue& dict);
};

#endif // TORRENT_FILE_PARSER_HPP