#include "../include/magnet_link_parser.hpp"
#include <sstream>
#include <cctype>
#include <algorithm>
#include <regex>

MagnetLinkParser::MagnetLinkParser(const std::string& magnetLink) {
    parseMagnetLink(magnetLink);
}

std::string MagnetLinkParser::getInfoHash() const {
    return infoHash;
}

std::string MagnetLinkParser::getDisplayName() const {
    return displayName;
}

std::vector<std::string> MagnetLinkParser::getTrackers() const {
    return trackers;
}

int64_t MagnetLinkParser::getFileSize() const {
    return fileSize;
}


std::string urlDecode(const std::string& str) {
    std::ostringstream decoded;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            std::string hex = str.substr(i + 1, 2);
            char ch = static_cast<char>(std::stoul(hex, nullptr, 16));
            decoded << ch;
            i += 2;
        } else if (str[i] == '+') {
            decoded << ' ';
        } else {
            decoded << str[i];
        }
    }
    return decoded.str();
}


void MagnetLinkParser::parseMagnetLink(const std::string& magnetLink) {
    if (magnetLink.substr(0, 8) != "magnet:?") {
        throw std::runtime_error("Invalid magnet link format");
    }

    std::istringstream iss(magnetLink.substr(8)); // Skip "magnet:?"
    std::string param;
    while (std::getline(iss, param, '&')) {
        if (param.substr(0, 3) == "xt=") {
            // Extract info hash
            size_t pos = param.find("urn:btih:");
            if (pos != std::string::npos) {
                infoHash = param.substr(pos + 9); // Skip "urn:btih:"
            }
        } else if (param.substr(0, 3) == "dn=") {
            // Extract display name
            displayName = param.substr(3);
        } else if (param.substr(0, 3) == "tr=") {
            // Extract and decode tracker URL
            std::string trackerUrl = param.substr(3);
            trackers.push_back(urlDecode(trackerUrl));
        } else if (param.substr(0, 3) == "xl=") {
            // Extract file size
            fileSize = std::stoll(param.substr(3));
        }
    }

    if (infoHash.empty()) {
        throw std::runtime_error("Info hash not found in magnet link");
    }

    //  Validate info hash format
    validateInfoHash(infoHash);
}

// Validate info hash (40 hex chars or 32 Base32 chars)
void MagnetLinkParser::validateInfoHash(const std::string& hash) {
    bool isHex = (hash.length() == 40 && std::regex_match(hash, std::regex("^[0-9a-fA-F]{40}$")));
    bool isBase32 = (hash.length() == 32 && std::regex_match(hash, std::regex("^[A-Z2-7]{32}$")));

    if (!isHex && !isBase32) {
        throw std::runtime_error("Invalid info hash format");
    }
}