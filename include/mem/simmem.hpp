#pragma once

#include <array>
#include <string>
#include <cstdint>

// Simulator memory class
class SimMemory {
    // 1MB fixed memory size
    constexpr static size_t MEM_SIZE = 1024 * 1024;
    // 1MB fixed memory
    // note that the index indicates the index-th byte
    std::array<uint8_t, MEM_SIZE> mem {};

public:
    // load raw data from a string
    void load_hex_data(const std::string& data);
    // read out a byte
    uint8_t read_byte(size_t index) const;
    // write in a byte
    void write_byte(size_t index, uint8_t val);

};