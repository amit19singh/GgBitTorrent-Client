#include "../include/torrent_file_parser.hpp"
#include <iostream>
#include <cassert>

void testValidTorrentFile() {
    // Assuming a valid .torrent test file exists
    std::string testFilePath = "C:/Users/amit1/Dropbox/test_torrent.torrent";
    
    try {
        TorrentFileParser parser(testFilePath);
        TorrentFile torrent = parser.parse();
        
        // Print parsed data
        std::cout << "Parsed Torrent File:\n";
        std::cout << "Announce URL: " << torrent.announce << "\n";
        std::cout << "Comment: " << torrent.comment << "\n";
        std::cout << "Creation Date: " << torrent.creationDate << "\n";
        std::cout << "Name: " << torrent.name << "\n";
        std::cout << "Piece Length: " << torrent.pieceLength << "\n";
        std::cout << "Number of Pieces: " << torrent.pieces.size() << "\n";

        std::cout << "Files:\n";
        for (const auto& file : torrent.files) {
            std::cout << "  " << file.first << " (" << file.second << " bytes)\n";
        }

        // Validate extracted metadata (values will depend on the actual test file)
        assert(!torrent.announce.empty());
        assert(!torrent.name.empty());
        assert(torrent.pieceLength > 0);
        assert(!torrent.pieces.empty());

        // Ensure at least one file exists
        assert(!torrent.files.empty());

        std::cout << "Valid .torrent file test passed!" << '\n' << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        assert(false);
    }
}

void testInvalidTorrentFile() {
    std::string invalidFilePath = "C:/Users/amit1/Dropbox/big-buck.torrent";

    try {
        TorrentFileParser parser(invalidFilePath);
        parser.parse(); // Should throw an error
        assert(false); // If no exception is thrown, test fails
    } catch (const std::runtime_error& e) {
        std::cout << "Invalid .torrent file test passed! Caught exception: " << e.what() << std::endl;
    }
}

void testMissingFields() {
    std::string missingFieldsFilePath = "C:/Users/amit1/Dropbox/big-buck-bunny.torrent";

    try {
        TorrentFileParser parser(missingFieldsFilePath);
        TorrentFile torrent = parser.parse();

        // Check if missing fields return expected defaults
        assert(torrent.announce.empty()); // If "announce" was missing, should be empty
        assert(torrent.creationDate == 0); // If missing, should be 0

        std::cout << "Missing fields test passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        assert(false);
    }
}

int main() {
    testValidTorrentFile();
    testInvalidTorrentFile();
    testMissingFields();

    std::cout << "All TorrentFileParser tests passed!" << std::endl;
    return 0;
}
