#include "../include/bencode_parser.hpp"
#include <iostream>
#include <sstream>
#include <cctype>

// Parse a bencoded string into a BencodedValue
BencodedValue BencodeParser::parse(const std::string& data) {
    size_t pos = 0;
    return parseValue(data, pos);
}

// Method to Parse Integer data, e.g, i1234e
int64_t BencodeParser::parseInt(const std::string& data, size_t& pos) {
    pos++; // Skip 'i'
    size_t endPos = data.find('e', pos);
    if (endPos == std::string::npos) {
        throw std::runtime_error("Invalid integer format");
    }

    std::string numberStr = data.substr(pos, endPos - pos);
    pos = endPos + 1; // Skip 'e'

    try {
        return std::stoll(numberStr);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid integer value");
    }
}

// Method to Parse String data, e.g, 4:abcd
std::string BencodeParser::parseString(const std::string& data, size_t& pos) {
    size_t colonPos = data.find(':', pos);
    if (colonPos == std::string::npos) {
        throw std::runtime_error("Invalid string format");
    }

    std::string lengthStr = data.substr(pos, colonPos - pos);
    int64_t length = std::stoll(lengthStr);
    pos = colonPos + 1;

    if (pos + length > data.size()) {
        throw std::runtime_error("String length exceeds input size");
    }

    std::string result = data.substr(pos, length);
    pos += length;
    return result;
}

// Method to Parse a list, e.g, li42e5:helloli1ei2eee -> [42, "hello", [1, 2]]
BencodedList BencodeParser::parseList(const std::string& data, size_t& pos) {
    pos++; // Skip 'l'
    BencodedList result;

    while (pos < data.size() && data[pos] != 'e') {
        result.push_back(parseValue(data, pos));
    }

    if (pos >= data.size() || data[pos] != 'e') {
        throw std::runtime_error("Invalid list format");
    }

    pos++; // Skip 'e'
    return result;
}

// Method to Parse a dictionary, e.g, d3:keyi42ee -> {"key": 42}
BencodedDict BencodeParser::parseDict(const std::string& data, size_t& pos) {
    pos++; // Skip 'd'
    BencodedDict result;

    while (pos < data.size() && data[pos] != 'e') {
        std::string key = parseString(data, pos);
        BencodedValue value = parseValue(data, pos);
        result[key] = std::move(value); // Use std::move
    }

    if (pos >= data.size() || data[pos] != 'e') {
        throw std::runtime_error("Invalid dictionary format");
    }

    pos++; // Skip 'e'
    return result;
}

// Main Parse function
BencodedValue BencodeParser::parseValue(const std::string& data, size_t& pos) {
    if (pos >= data.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    char ch = data[pos];
    if (ch == 'i') {
        return BencodedValue(parseInt(data, pos)); // int64_t
    } else if (ch == 'l') {
        return BencodedValue(parseList(data, pos)); // std::unique_ptr<BencodedList>
    } else if (ch == 'd') {
        return BencodedValue(parseDict(data, pos)); // std::unique_ptr<BencodedDict>
    } else if (isdigit(ch)) {
        return BencodedValue(parseString(data, pos)); // std::string
    } else {
        throw std::runtime_error("Invalid bencoded format");
    }
}