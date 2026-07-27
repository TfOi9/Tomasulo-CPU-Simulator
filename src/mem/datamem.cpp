#include "../../include/mem/datamem.hpp"

#include <cassert>

void DataMemory::load_hex_data(const std::string &data) {
    mem.load_hex_data(data);
}

uint32_t DataMemory::read_word(size_t index) const {
    assert(index % 4 == 0);
    return (mem.read_byte(index + 3) << 24) |
           (mem.read_byte(index + 2) << 16) |
           (mem.read_byte(index + 1) << 8)  |
            mem.read_byte(index);
}

uint32_t DataMemory::read_half(size_t index, bool is_unsigned) const {
    assert(index % 2 == 0);
    uint32_t half = (mem.read_byte(index + 1) << 8) |
                     mem.read_byte(index);
    if (is_unsigned && half & 0x8000) {
        half |= 0xFFFF0000;
    }
    return half;
}

uint32_t DataMemory::read_byte(size_t index, bool is_unsigned) const {
    uint32_t byte = mem.read_byte(index);
    if (is_unsigned && byte & 0x80) {
        byte |= 0xFFFFFF00;
    }
    return byte;
}

void DataMemory::write_word(size_t index, uint32_t val) {
    mem.write_byte(index + 3, uint8_t(val >> 24));
    mem.write_byte(index + 2, uint8_t((val >> 16) & 0xFF));
    mem.write_byte(index + 1, uint8_t((val >> 8) & 0xFF));
    mem.write_byte(index, uint8_t(val & 0xFF));
}

void DataMemory::write_half(size_t index, uint16_t val) {
    mem.write_byte(index + 1, uint8_t(val >> 8));
    mem.write_byte(index, uint8_t(val & 0xFF));
}

void DataMemory::write_byte(size_t index, uint8_t val) {
    mem.write_byte(index, val);
}