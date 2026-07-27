#include "../include/ints.hpp"

#include <cassert>

bool is_valid_hex_char(char ch) {
    return ch >= '0' && ch <= '9' || ch >= 'A' && ch <= 'F';
}

uint8_t to_bits(char ch) {
    assert(is_valid_hex_char(ch));

    if (ch >= '0' && ch <= '9') {
        return uint8_t(ch - '0');
    } else {
        return uint8_t(ch - 'A' + 10);
    }
}

uint8_t to_byte(const std::string &hex) {
    assert(hex.size() == 2);

    return (to_bits(hex[0]) << 4) | to_bits(hex[1]);
}