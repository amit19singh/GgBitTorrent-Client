#include "../include/peer_connection.hpp"
#include <algorithm>
#include <iostream>

void PeerConnection::send_choke() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    create_message_header(CHOKE);
    choked_by_us = true;
}

void PeerConnection::send_unchoke() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    create_message_header(UNCHOKE);
    choked_by_us = false;
}

void PeerConnection::send_interested() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    create_message_header(INTERESTED);
    interested = true;
}

void PeerConnection::send_not_interested() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    create_message_header(NOT_INTERESTED);
    interested = false;
}

void PeerConnection::create_message_header(MessageType type, uint32_t length) {
    // Message format: [length][id][payload]
    uint32_t network_length = htonl(1 + length); // 1 byte for message ID
    std::vector<uint8_t> message(sizeof(network_length) + 1 + length);
    
    // Copy length
    memcpy(message.data(), &network_length, sizeof(network_length));
    
    // Message ID
    message[4] = static_cast<uint8_t>(type);
    
    append_to_output(message);
}

void PeerConnection::create_request_message(uint32_t piece_index, 
                                          uint32_t block_offset,
                                          uint32_t block_length) {
    std::vector<uint8_t> message(17); // 4 (length) + 1 (id) + 12 (payload)
    
    uint32_t network_length = htonl(13); // 1 + 12
    memcpy(message.data(), &network_length, 4);
    
    message[4] = REQUEST;
    
    uint32_t network_index = htonl(piece_index);
    memcpy(message.data() + 5, &network_index, 4);
    
    uint32_t network_offset = htonl(block_offset);
    memcpy(message.data() + 9, &network_offset, 4);
    
    uint32_t network_block_length  = htonl(block_length);
    memcpy(message.data() + 13, &network_block_length , 4);
    
    append_to_output(message);
}

void PeerConnection::append_to_output(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    output_buffer.insert(output_buffer.end(), data.begin(), data.end());
}

void PeerConnection::process_input_buffer() {
    while (true) {
        if (input_buffer.size() < 4) return;

        // Get message length
        uint32_t message_length;
        memcpy(&message_length, input_buffer.data(), 4);
        message_length = ntohl(message_length);

        // Check if we have complete message
        if (input_buffer.size() < message_length + 4) return;

        // Extract message
        std::vector<uint8_t> message(input_buffer.begin() + 4, 
                                   input_buffer.begin() + 4 + message_length);
        handle_message(message);

        // Remove processed data from buffer
        input_buffer.erase(input_buffer.begin(), 
                         input_buffer.begin() + 4 + message_length);
    }
}

void PeerConnection::handle_message(const std::vector<uint8_t>& message) {
    if (message.empty()) return;

    uint8_t message_id = message[0];
    const uint8_t* payload = message.data() + 1;
    size_t payload_size = message.size() - 1;

    switch (message_id) {
        case CHOKE:
            choked_by_peer = true;
            break;
            
        case UNCHOKE:
            choked_by_peer = false;
            break;
            
        case INTERESTED:
            peer_interested = true;
            break;
            
        case NOT_INTERESTED:
            peer_interested = false;
            break;
            
        case HAVE: {
            if (payload_size != 4) break;
            uint32_t piece_index;
            memcpy(&piece_index, payload, 4);
            piece_index = ntohl(piece_index);
            if (piece_index < bitfield.size()) {
                bitfield[piece_index] = true;
            }
            break;
        }
        
        case BITFIELD: {
            bitfield.resize(payload_size * 8);
            for (size_t i = 0; i < payload_size; ++i) {
                uint8_t byte = payload[i];
                for (int j = 0; j < 8; ++j) {
                    size_t index = i * 8 + j;
                    if (index < bitfield.size()) {
                        bitfield[index] = (byte >> (7 - j)) & 1;
                    }
                }
            }
            break;
        }
        
        case REQUEST: {
            if (payload_size != 12) break;
            uint32_t piece_index, block_offset, block_length;
            memcpy(&piece_index, payload, 4);
            memcpy(&block_offset, payload + 4, 4);
            memcpy(&block_length, payload + 8, 4);
            
            piece_index = ntohl(piece_index);
            block_offset = ntohl(block_offset);
            block_length = ntohl(block_length);

            // Serialize the request into bytes
            std::vector<uint8_t> request_message;
            request_message.resize(12); // 3 * uint32_t = 12 bytes

            // Copy piece_index, block_offset, and block_length into the vector
            memcpy(request_message.data(), &piece_index, 4);
            memcpy(request_message.data() + 4, &block_offset, 4);
            memcpy(request_message.data() + 8, &block_length, 4);
            
            // Add to message queue
            std::lock_guard<std::mutex> lock(queue_mutex);
            message_queue.push_back(request_message);
            break;
        }
        
        case PIECE: {
            if (payload_size < 8) break;
            uint32_t piece_index, block_offset;
            memcpy(&piece_index, payload, 4);
            memcpy(&block_offset, payload + 4, 4);
            
            piece_index = ntohl(piece_index);
            block_offset = ntohl(block_offset);
            
            // Extract block data
            const uint8_t* block_data = payload + 8;
            size_t data_size = payload_size - 8;
            
            // Handle received data
            std::cout << "Received piece " << piece_index
                      << " block " << block_offset
                      << " (" << data_size << " bytes)\n";
            break;
        }
        
        default:
            std::cerr << "Unknown message type: " << static_cast<int>(message_id) << "\n";
    }
}

void PeerConnection::update_rate_counters(size_t downloaded, size_t uploaded) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();
    
    if (elapsed > 0) {
        download_rate = downloaded / elapsed;
        upload_rate = uploaded / elapsed;
        last_activity = now;
    }
}