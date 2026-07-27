#pragma once

#include "simmem.hpp"

// Data Memory class
// designed for interpreter
class DataMemory {
    // simulated memory space
    SimMemory mem;

public:
    // load raw data from a string
    void load_hex_data(const std::string& data);
    // read a word from the memory
    uint32_t read_word(size_t index) const;
    // read a half-word from the memory
    uint32_t read_half(size_t index, bool is_unsigned) const;
    // read a byte from the memory
    uint32_t read_byte(size_t index, bool is_unsigned) const;
    // write a word to the memory
    void write_word(size_t index, uint32_t val);
    // write a half-word to the memory
    void write_half(size_t index, uint16_t val);
    // write a byte to the memory
    void write_byte(size_t index, uint8_t val);

};