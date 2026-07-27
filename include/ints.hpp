#pragma once

#include <string>
#include <cstdint>

// check if the char is a valid hexdecimal char
// i.e. 0~9, A~F
bool is_valid_hex_char(char ch);

// convert the hexdecimal char to an integer
// i.e. B to 11
uint8_t to_bits(char ch);

// convert a hexdecimal string lengthed 2 to an integer
// i.e. "10" to 16
uint8_t to_byte(const std::string& hex);