#include "byte_tools.h"
#include <openssl/sha.h>
#include <vector>

int BytesToInt(std::string_view bytes) {
    int res = 0;
    for (const char& byte : bytes) {
        res = (res << 8) + (int)(unsigned char)(byte);
    }
    return res;
}

std::string IntToBytes(size_t val) {
    std::string payload_length = "0000";
    for (int i = 3; i >= 0; --i) {
        payload_length[i] = (char) (val & 255);
        val >>= 8;
    }
    return payload_length;
}

std::string CalculateSHA1(const std::string& msg) {
    unsigned char hash[20];
    SHA1((const unsigned char *) msg.c_str(), msg.size(), hash);
    std::string result = "";
    for (int i = 0; i < 20; ++i) {
        result += hash[i];
    }
    return result;
}

std::string HexEncode(const std::string& input) {
    static const char hex[] = "0123456789ABCDEF";

    std::string output;
    output.resize(input.size() * 2);

    for (unsigned char c : input) {
        output.push_back(hex[c >> 4]);
        output.push_back(hex[c & 15]);
    }

    return output;
}