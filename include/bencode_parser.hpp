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

    // Type-checking methods
    bool isInt() const { return std::holds_alternative<int64_t>(value); }
    bool isString() const { return std::holds_alternative<std::string>(value); }
    bool isList() const { return std::holds_alternative<BencodedList>(value); }
    bool isDict() const { return std::holds_alternative<BencodedDict>(value); }

    // Value access methods
    int64_t asInt() const {
        if (!isInt()) throw std::runtime_error("Not an integer");
        return std::get<int64_t>(value);
    }

    const std::string& asString() const {
        if (!isString()) throw std::runtime_error("Not a string");
        return std::get<std::string>(value);
    }

    const BencodedList& asList() const {
        if (!isList()) throw std::runtime_error("Not a list");
        return std::get<BencodedList>(value);
    }

    const BencodedDict& asDict() const {
        if (!isDict()) throw std::runtime_error("Not a dictionary");
        return std::get<BencodedDict>(value);
    }
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