#include "../include/magnet_link_parser.hpp"
#include <iostream>
#include <cassert>

void testValidMagnetLink() {
    std::string magnetLink = "magnet:?xt=urn:btih:6a9759bffd5c0af65319979fb7832189f4f3c35d&dn=sample_file&tr=udp%3A%2F%2Ftracker.example.com%3A80&xl=1048576";
    MagnetLinkParser parser(magnetLink);

    // Test info hash
    assert(parser.getInfoHash() == "6a9759bffd5c0af65319979fb7832189f4f3c35d");
    std::cout << "Info hash test passed!" << std::endl;

    // Test display name
    assert(parser.getDisplayName() == "sample_file");
    std::cout << "Display name test passed!" << std::endl;

    // Test trackers
    auto trackers = parser.getTrackers();
    assert(trackers.size() == 1);
    assert(trackers[0] == "udp://tracker.example.com:80");
    std::cout << "Trackers test passed!" << std::endl;

    // Test file size
    assert(parser.getFileSize() == 1048576);
    std::cout << "File size test passed!" << std::endl;
}

void testMagnetLinkWithoutOptionalFields() {
    std::string magnetLink = "magnet:?xt=urn:btih:6a9759bffd5c0af65319979fb7832189f4f3c35d";
    MagnetLinkParser parser(magnetLink);

    // Test info hash
    assert(parser.getInfoHash() == "6a9759bffd5c0af65319979fb7832189f4f3c35d");
    std::cout << "Info hash (without optional fields) test passed!" << std::endl;

    // Test display name (should be empty)
    assert(parser.getDisplayName().empty());
    std::cout << "Display name (without optional fields) test passed!" << std::endl;

    // Test trackers (should be empty)
    assert(parser.getTrackers().empty());
    std::cout << "Trackers (without optional fields) test passed!" << std::endl;

    // Test file size (should be 0)
    assert(parser.getFileSize() == 0);
    std::cout << "File size (without optional fields) test passed!" << std::endl;
}

void testInvalidMagnetLink() {
    std::string magnetLink = "magnet:?xt=urn:btih:invalid_hash";
    // std::string magnetLink = "magnet:?xt=urn:btih:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    try {
        MagnetLinkParser parser(magnetLink);
        assert(false); // Should not reach here
    } catch (const std::runtime_error& e) {
        std::cout << "Invalid magnet link test passed! Caught exception: " << e.what() << std::endl;
    }
}

void testMagnetLinkWithMultipleTrackers() {
    std::string magnetLink = "magnet:?xt=urn:btih:6a9759bffd5c0af65319979fb7832189f4f3c35d&tr=udp%3A%2F%2Ftracker1.example.com%3A80&tr=udp%3A%2F%2Ftracker2.example.com%3A80";
    MagnetLinkParser parser(magnetLink);

    // Test trackers
    auto trackers = parser.getTrackers();
    assert(trackers.size() == 2);
    assert(trackers[0] == "udp://tracker1.example.com:80");
    assert(trackers[1] == "udp://tracker2.example.com:80");
    std::cout << "Multiple trackers test passed!" << std::endl;
}

int main() {
    testValidMagnetLink();
    testMagnetLinkWithoutOptionalFields();
    testInvalidMagnetLink();
    testMagnetLinkWithMultipleTrackers();

    std::cout << "All magnet link parser tests passed!" << std::endl;
    return 0;
}