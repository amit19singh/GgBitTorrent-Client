#ifndef PIECE_MANAGER_HPP
#define PIECE_MANAGER_HPP

#define MAX_BLOCK_SIZE 16384

#include <vector>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class PieceManager {
public:
    explicit PieceManager(int numPieces, int pieceLength);

    bool getPieceBlock(int pieceIndex, int blockOffset, int blockSize, std::vector<uint8_t>& data);
    bool storePieceBlock(int pieceIndex, int blockOffset, const std::vector<uint8_t>& data);
    bool isPieceComplete(int pieceIndex);
    bool getFullPiece(int pieceIndex, std::vector<uint8_t>& data);
    bool markPieceAsDownloaded(int pieceIndex);
    int getBlockCount(int pieceIndex);

private:
    int numPieces;
    int pieceLength;
    
    struct PieceData {
        std::vector<uint8_t> data;
        std::vector<bool> receivedBlocks;
        int receivedBlockCount = 0;
    };

    std::unordered_map<int, PieceData> pieces;  // Store pieces by index
    std::unordered_set<int> completedPieces; // completed pieces
    std::mutex mutex;  // Protects concurrent access

    // int getBlockCount(int pieceIndex);
};

#endif // PIECE_MANAGER_HPP
