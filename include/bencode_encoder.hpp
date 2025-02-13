#include "../include/bencode_parser.hpp"
#include <string>
#include <vector>
#include <map>

class BencodeEncoder {
public:
    static std::string encode(const BencodedValue& value);
    
private:
    static std::string encodeInt(int64_t value);
    static std::string encodeString(const std::string& value);
    static std::string encodeList(const BencodedList& list);
    static std::string encodeDict(const BencodedDict& dict);
};