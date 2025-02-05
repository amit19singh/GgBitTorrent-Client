#ifndef MAGNET_LINK_PARSER_HPP
#define MAGNET_LINK_PARSER_HPP

#include <string>
#include <map>
#include <vector>
#include <cstdint>

class MagnetLinkParser {
public:
    explicit MagnetLinkParser(const std::string& magnetLink);

    // Getters for magnet link components
    std::string getInfoHash() const;
    std::string getDisplayName() const;
    std::vector<std::string> getTrackers() const;
    int64_t getFileSize() const;

private:
    std::string infoHash; // The info hash (40-character hex or Base32)
    std::string displayName; // Display name of the file(s)
    std::vector<std::string> trackers; // List of tracker URLs
    int64_t fileSize = 0; // Optional file size

    // Helper function to parse the magnet link
    void parseMagnetLink(const std::string& magnetLink);
    // Helper function to validate the info hash
    void validateInfoHash(const std::string& hash);
};

#endif // MAGNET_LINK_PARSER_HPP
