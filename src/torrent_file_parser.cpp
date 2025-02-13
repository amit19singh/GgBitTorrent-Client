#include "../include/torrent_file_parser.hpp"
#include <fstream>
#include <sstream>
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

    // Extract metadata
    TorrentFile torrentFile;
    torrentFile.announce = extractString(rootDict, "announce");
    torrentFile.comment = extractString(rootDict, "comment");
    torrentFile.creationDate = extractInt(rootDict, "creation date");

    // Extract info dictionary
    if (rootDict.find("info") == rootDict.end()) {
        throw std::runtime_error("Invalid .torrent file format: Missing 'info' dictionary");
    }

    if (!std::holds_alternative<BencodedDict>(rootDict.at("info").value)) {
        throw std::runtime_error("Invalid .torrent file format: 'info' is not a dictionary");
    }

    auto& infoDict = std::get<BencodedDict>(rootDict.at("info").value);
    torrentFile.name = extractString(infoDict, "name");
    torrentFile.pieceLength = extractInt(infoDict, "piece length");
    torrentFile.pieces = extractPieces(infoDict);

    // Handle single-file vs multi-file torrents
    if (infoDict.find("length") != infoDict.end()) {
        // Single-file torrent
        torrentFile.files.push_back({torrentFile.name, extractInt(infoDict, "length")});
    } else {
        // Multi-file torrent
        torrentFile.files = extractFiles(infoDict);
    }

    return torrentFile;
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

