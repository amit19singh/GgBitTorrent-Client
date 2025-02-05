#include "../include/bencode_parser.hpp"
#include <iostream>
#include <cassert>

void testInteger() {
    BencodeParser parser;
    std::string data = "i42e";
    BencodedValue result = parser.parse(data);
    assert(std::holds_alternative<int64_t>(result.value));
    assert(std::get<int64_t>(result.value) == 42);
    std::cout << "Integer test passed!" << std::endl;
}

void testString() {
    BencodeParser parser;
    std::string data = "5:hello";
    BencodedValue result = parser.parse(data);
    assert(std::holds_alternative<std::string>(result.value));
    assert(std::get<std::string>(result.value) == "hello");
    std::cout << "String test passed!" << std::endl;
}

void testList() {
    BencodeParser parser;
    std::string data = "li42e5:helloe";
    BencodedValue result = parser.parse(data);
    assert(std::holds_alternative<std::unique_ptr<BencodedList>>(result.value));
    auto& list = std::get<std::unique_ptr<BencodedList>>(result.value);
    assert(list.size() == 2);
    assert(std::holds_alternative<int64_t>(list[0].value));
    assert(std::get<int64_t>(list[0].value) == 42);
    assert(std::holds_alternative<std::string>(list[1].value));
    assert(std::get<std::string>(list[1].value) == "hello");
    std::cout << "List test passed!" << std::endl;
}

void testDict() {
    BencodeParser parser;
    std::string data = "d3:keyi42e5:value5:helloe";
    BencodedValue result = parser.parse(data);
    assert(std::holds_alternative<std::unique_ptr<BencodedDict>>(result.value));
    auto& dict = *std::get<std::unique_ptr<BencodedDict>>(result.value);
    assert(dict.size() == 2);
    assert(std::holds_alternative<int64_t>(dict["key"].value));
    assert(std::get<int64_t>(dict["key"].value) == 42);
    assert(std::holds_alternative<std::string>(dict["value"].value));
    assert(std::get<std::string>(dict["value"].value) == "hello");
    std::cout << "Dictionary test passed!" << std::endl;
}

void testInvalidInput() {
    BencodeParser parser;
    std::string data = "i42"; // Missing 'e'
    try {
        parser.parse(data);
        assert(false); // Should not reach here
    } catch (const std::runtime_error& e) {
        std::cout << "Invalid input test passed! Caught exception: " << e.what() << std::endl;
    }
}

int main() {
    testInteger();
    testString();
    testList();
    testDict();
    testInvalidInput();

    std::cout << "All tests passed!" << std::endl;
    return 0;
}