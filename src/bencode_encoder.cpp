#include "../include/bencode_encoder.hpp"

std::string BencodeEncoder::encode(const BencodedValue& value) {
    if (value.isInt()) {
        return encodeInt(value.asInt());
    } else if (value.isString()) {
        return encodeString(value.asString());
    } else if (value.isList()) {
        return encodeList(value.asList());
    } else if (value.isDict()) {
        return encodeDict(value.asDict());
    }
    return "";
}

std::string BencodeEncoder::encodeInt(int64_t value) {
    return "i" + std::to_string(value) + "e";
}

std::string BencodeEncoder::encodeString(const std::string& value) {
    return std::to_string(value.size()) + ":" + value;
}

std::string BencodeEncoder::encodeList(const BencodedList& list) {
    std::string result = "l";
    for (const auto& item : list) {
        result += encode(item);
    }
    return result + "e";
}

std::string BencodeEncoder::encodeDict(const BencodedDict& dict) {
    std::string result = "d";
    // Dictionaries must have keys sorted lexicographically
    std::map<std::string, BencodedValue> ordered(dict.begin(), dict.end());
    for (const auto& [key, value] : ordered) {
        result += encodeString(key);
        result += encode(value);
    }
    return result + "e";
}