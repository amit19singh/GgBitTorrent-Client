#include "../include/torrent_file_parser.hpp"
#include "../include/bencode_encoder.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>


TorrentFileParser::TorrentFileParser(const std::string& filePath)
    : filePath(filePath) {}

TorrentFile TorrentFileParser::parse() {
    // Read the .torrent file into a string
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open .torrent file");
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string data = buffer.str();

    // Parse the Bencoded data
    BencodedValue parsedData = bencodeParser.parse(data);

    // Check if the parsed data is a dictionary
    if (!std::holds_alternative<BencodedDict>(parsedData.value)) {
        throw std::runtime_error("Invalid .torrent file format: Root is not a dictionary");
    }

    // Extract the root dictionary
    auto& rootDict = std::get<BencodedDict>(parsedData.value);

    // Extract info dictionary
    if (rootDict.find("info") == rootDict.end() ||
            !std::holds_alternative<BencodedDict>(rootDict.at("info").value)) {
        throw std::runtime_error("Invalid .torrent file format: Missing 'info' dictionary");
    }

    // Extract metadata
    TorrentFile parsedTorrent;
    parsedTorrent.announce = extractString(rootDict, "announce");
    parsedTorrent.comment = extractString(rootDict, "comment");
    parsedTorrent.creationDate = extractInt(rootDict, "creation date");

    auto& infoDict = std::get<BencodedDict>(rootDict.at("info").value);
    parsedTorrent.name = extractString(infoDict, "name");
    parsedTorrent.pieceLength = extractInt(infoDict, "piece length");
    parsedTorrent.pieces = extractPieces(infoDict);

    // Handle single-file vs multi-file torrents
    // if (infoDict.find("length") != infoDict.end()) {
    //     // Single-file torrent
    //     torrentFile.files.push_back({torrentFile.name, extractInt(infoDict, "length")});
    // } else {
    //     // Multi-file torrent
    //     torrentFile.files = extractFiles(infoDict);
    // }
    // Handle single-file vs multi-file torrents

    int64_t totalFileSize = 0;
    if (infoDict.find("length") != infoDict.end()) {
        // Single-file torrent
        totalFileSize = extractInt(infoDict, "length");
        parsedTorrent.files.push_back({parsedTorrent.name, totalFileSize});
    } else {
        // Multi-file torrent
        parsedTorrent.files = extractFiles(infoDict);
        for (const auto& file : parsedTorrent.files) {
            totalFileSize += file.second;  // Sum up all file sizes
        }
    }

    // Compute number of pieces
    parsedTorrent.numPieces = (totalFileSize / parsedTorrent.pieceLength) +
                            ((totalFileSize % parsedTorrent.pieceLength) > 0 ? 1 : 0);

    // --- Compute the info hash ---
    // Encode the "info" dictionary back into its bencoded form
    std::string encodedInfo = BencodeEncoder::encode(BencodedValue(infoDict));
    // Compute SHA-1 hash of the encoded info string (you must implement or use a library function)
    parsedTorrent.infoHash = computeSHA1(encodedInfo);
    // ----------------------------------

    std::cout << "Total file size: " << totalFileSize << " bytes\n";
    std::cout << "Piece length: " << parsedTorrent.pieceLength << " bytes\n";
    std::cout << "Number of pieces: " << parsedTorrent.numPieces << "\n";

    return parsedTorrent;
}

const int TorrentFileParser::getNumPieces() {
    return parsedTorrent.numPieces;
}

std::string TorrentFileParser::extractString(const BencodedValue& dict, const std::string& key) {
    if (!std::holds_alternative<BencodedDict>(dict.value)) {
        throw std::runtime_error("Expected a dictionary");
    }

    auto& map = std::get<BencodedDict>(dict.value);
    auto it = map.find(key);
    if (it == map.end()) {
        return ""; // Return empty string if key is not found
    }

    if (!std::holds_alternative<std::string>(it->second.value)) {
        throw std::runtime_error("Expected a string for key: " + key);
    }

    return std::get<std::string>(it->second.value);
}

int64_t TorrentFileParser::extractInt(const BencodedValue& dict, const std::string& key) {
    if (!std::holds_alternative<BencodedDict>(dict.value)) {
        throw std::runtime_error("Expected a dictionary");
    }

    auto& map = std::get<BencodedDict>(dict.value);
    auto it = map.find(key);
    if (it == map.end()) {
        return 0; // Return 0 if key is not found
    }

    if (!std::holds_alternative<int64_t>(it->second.value)) {
        throw std::runtime_error("Expected an integer for key: " + key);
    }

    return std::get<int64_t>(it->second.value);
}

std::vector<std::string> TorrentFileParser::extractPieces(const BencodedValue& dict) {
    if (!std::holds_alternative<BencodedDict>(dict.value)) {
        throw std::runtime_error("Expected a dictionary");
    }

    auto& map = std::get<BencodedDict>(dict.value);
    auto it = map.find("pieces");
    if (it == map.end()) {
        throw std::runtime_error("Missing 'pieces' key in info dictionary");
    }

    if (!std::holds_alternative<std::string>(it->second.value)) {
        throw std::runtime_error("Expected a string for 'pieces'");
    }

    std::string piecesStr = std::get<std::string>(it->second.value);
    std::vector<std::string> pieces;

    // Split the pieces string into 20-byte SHA-1 hashes
    for (size_t i = 0; i + 20 <= piecesStr.size(); i += 20) {
        pieces.push_back(piecesStr.substr(i, 20));
    }

    return pieces;
}

std::vector<std::pair<std::string, int64_t>> TorrentFileParser::extractFiles(const BencodedValue& dict) {
    if (!std::holds_alternative<BencodedDict>(dict.value)) {
        throw std::runtime_error("Expected a dictionary");
    }

    auto& map = std::get<BencodedDict>(dict.value);
    auto it = map.find("files");
    if (it == map.end()) {
        throw std::runtime_error("Missing 'files' key in info dictionary");
    }

    if (!std::holds_alternative<std::vector<BencodedValue>>(it->second.value)) {
        throw std::runtime_error("Expected a list for 'files'");
    }

    auto& filesList = std::get<std::vector<BencodedValue>>(it->second.value);
    std::vector<std::pair<std::string, int64_t>> files;

    for (const auto& fileDict : filesList) {
        if (!std::holds_alternative<BencodedDict>(fileDict.value)) {
            throw std::runtime_error("Expected a dictionary for file entry");
        }

        auto& fileMap = std::get<BencodedDict>(fileDict.value);
        int64_t length = extractInt(fileMap, "length");

        // Extract file path
        auto pathIt = fileMap.find("path");
        if (pathIt == fileMap.end()) {
            throw std::runtime_error("Missing 'path' key in file entry");
        }

        if (!std::holds_alternative<std::vector<BencodedValue>>(pathIt->second.value)) {
            throw std::runtime_error("Expected a list for 'path'");
        }

        auto& pathList = std::get<std::vector<BencodedValue>>(pathIt->second.value);
        std::string path;

        for (const auto& pathComponent : pathList) {
            if (!std::holds_alternative<std::string>(pathComponent.value)) {
                throw std::runtime_error("Expected a string for path component");
            }
            if (!path.empty()) {
                path += "/";
            }
            path += std::get<std::string>(pathComponent.value);
        }

        files.emplace_back(path, length);
    }

    return files;
}

