#ifndef BENCODE_PARSER_HPP
#define BENCODE_PARSER_HPP

#include <variant>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <stdexcept>

// Forward declaration
struct BencodedValue;

// Define aliases for recursive types
using BencodedList = std::vector<BencodedValue>;
using BencodedDict = std::map<std::string, BencodedValue>;

// Define the BencodedValue struct
struct BencodedValue {
    std::variant<
        int64_t,
        std::string,
        BencodedList,
        BencodedDict
    > value;

    // Default constructor
    BencodedValue() : value(int64_t(0)) {} // Initialize with a default value (e.g., 0)

    // Constructor for easy initialization
    template <typename T>
    BencodedValue(T&& val) : value(std::forward<T>(val)) {}
};

// BencodeParser class declaration
class BencodeParser {
public:
    // Parse a bencoded string into a BencodedValue
    BencodedValue parse(const std::string& data);

private:
    // Helper functions for parsing specific types
    int64_t parseInt(const std::string& data, size_t& pos);
    std::string parseString(const std::string& data, size_t& pos);
    BencodedList parseList(const std::string& data, size_t& pos);
    BencodedDict parseDict(const std::string& data, size_t& pos);

    // Main parsing function
    BencodedValue parseValue(const std::string& data, size_t& pos);
};

#endif // BENCODE_PARSER_HPP