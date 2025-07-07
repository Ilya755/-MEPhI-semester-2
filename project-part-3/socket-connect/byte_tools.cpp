#include "byte_tools.h"

int BytesToInt(std::string_view bytes) {
    int res = 0;
    for (const char& byte : bytes) {
        res = (res << 8) + (int)(unsigned char)(byte);
    }
    return res;
}
