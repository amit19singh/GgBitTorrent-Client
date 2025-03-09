#include "../include/piece_manager.hpp"
#include <cstring>  // for memset
#include <iostream> // for debugging

PieceManager::PieceManager(int numPieces, int pieceLength)
    : numPieces(numPieces), pieceLength(pieceLength) {
    std::cout << "Initializing PieceManager: " << numPieces << " pieces, " << pieceLength << " bytes each.\n";
}

bool PieceManager::getPieceBlock(int pieceIndex, int blockOffset, int blockSize, std::vector<uint8_t>& data) {
    if (pieceIndex < 0 || pieceIndex >= numPieces) {
        std::cerr << "getPieceBlock: Invalid piece index " << pieceIndex << '\n';
        return false;
    }
    if (blockOffset < 0 || blockOffset + blockSize > pieceLength) {
        std::cerr << "getPieceBlock: Invalid block range (offset=" << blockOffset 
                  << ", size=" << blockSize << ") for piece " << pieceIndex << '\n';
        return false;
    }

    // Check if the piece exists
    auto it = pieces.find(pieceIndex);
    if (it == pieces.end()) {
        std::cerr << "getPieceBlock: Requested piece " << pieceIndex << " is missing\n";
        return false;
    }

    // Extract the block
    data.assign(it->second.data.begin() + blockOffset, it->second.data.begin() + blockOffset + blockSize);
    std::cout << "getPieceBlock: Successfully retrieved " << blockSize << " bytes for piece " 
              << pieceIndex << " (offset " << blockOffset << ")\n";
    return true;
}


bool PieceManager::storePieceBlock(int pieceIndex, int blockOffset, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (pieceIndex < 0 || pieceIndex >= numPieces) {
        std::cerr << "storePieceBlock: Invalid piece index " << pieceIndex << '\n';
        return false;
    }
    
    if (blockOffset < 0 || blockOffset + data.size() > pieceLength) {
        std::cerr << "storePieceBlock: Invalid block range (offset=" << blockOffset 
                  << ", size=" << data.size() << ") for piece " << pieceIndex << '\n';
        return false;
    }

    if (pieces.find(pieceIndex) == pieces.end()) {
        std::cout << "storePieceBlock: Initializing storage for piece " << pieceIndex << '\n';
        pieces[pieceIndex].data.resize(pieceLength, 0);
        pieces[pieceIndex].receivedBlocks.resize(getBlockCount(pieceIndex), false);
        pieces[pieceIndex].receivedBlockCount = 0;
    }
    
    int blockIndex = blockOffset / MAX_BLOCK_SIZE;

    if (pieces[pieceIndex].receivedBlocks[blockIndex]) {
        std::cerr << "storePieceBlock: Block " << blockIndex << " for piece " << pieceIndex 
                  << " is already received, skipping.\n";
        return false;
    }

    std::memcpy(pieces[pieceIndex].data.data() + blockOffset, data.data(), data.size());
    
    pieces[pieceIndex].receivedBlocks[blockIndex] = true;
    pieces[pieceIndex].receivedBlockCount++;

    std::cout << "storePieceBlock: Stored block " << blockIndex << " for piece " << pieceIndex 
              << " (" << data.size() << " bytes)\n";
    std::cout << "storePieceBlock: Piece " << pieceIndex << " now has " 
              << pieces[pieceIndex].receivedBlockCount << "/" 
              << getBlockCount(pieceIndex) << " blocks received.\n";

    // **Check if piece is fully received**
    if (pieces[pieceIndex].receivedBlockCount == getBlockCount(pieceIndex)) {
        std::cout << "storePieceBlock: Piece " << pieceIndex << " is now complete!\n";
        completedPieces.insert(pieceIndex);
    }

    return true;
}

bool PieceManager::isPieceComplete(int pieceIndex) {
    std::lock_guard<std::mutex> lock(mutex);

    if (pieces.find(pieceIndex) == pieces.end()) return false;
    
    return pieces[pieceIndex].receivedBlockCount == getBlockCount(pieceIndex);
}

// bool PieceManager::getFullPiece(int pieceIndex, std::vector<uint8_t>& data) {
//     std::lock_guard<std::mutex> lock(mutex);
    
//     if (!isPieceComplete(pieceIndex)) return false;

//     data = pieces[pieceIndex].data;
//     return true;
// }

bool PieceManager::getFullPiece(int pieceIndex, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex);

    if (pieces.find(pieceIndex) == pieces.end()) {
        std::cerr << "❌ getFullPiece: Requested piece " << pieceIndex << " is missing.\n";
        return false;
    }

    data = pieces[pieceIndex].data;

    if (data.empty()) {
        std::cerr << "❌ getFullPiece: Retrieved piece " << pieceIndex << " is empty!\n";
        return false;
    }

    std::cout << "📦 getFullPiece: Retrieved full piece " << pieceIndex << " (" << data.size() << " bytes).\n";
    return true;
}


bool PieceManager:: markPieceAsDownloaded(int pieceIndex) {
    std::lock_guard<std::mutex> lock(mutex);

    if (pieces.find(pieceIndex) == pieces.end()) {
        std::cerr << "Error: Trying to mark non-existent piece " << pieceIndex << " as downloaded!\n";
        return false;
    }
    
    pieces[pieceIndex].receivedBlockCount = getBlockCount(pieceIndex);
    std::fill(pieces[pieceIndex].receivedBlocks.begin(), pieces[pieceIndex].receivedBlocks.end(), true);

    std::cout << "Marked piece " << pieceIndex << " as fully downloaded.\n";
    return true;
}

int PieceManager::getBlockCount(int pieceIndex) {
    int lastPieceSize = (pieceIndex == numPieces - 1) ? (pieceLength % MAX_BLOCK_SIZE) : pieceLength;
    return (lastPieceSize + MAX_BLOCK_SIZE - 1) / MAX_BLOCK_SIZE;
}
