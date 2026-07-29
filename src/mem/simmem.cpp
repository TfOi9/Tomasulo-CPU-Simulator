#include "../../include/mem/simmem.hpp"
#include "../../include/ints.hpp"

#include <cassert>
#include <sstream>
#include <string>

uint8_t SimMemory::read_byte(size_t index) const {
    if (index >= MEM_SIZE) return 0;
    return mem[index];
}

void SimMemory::write_byte(size_t index, uint8_t val) {
    if (index >= MEM_SIZE) return;
    mem[index] = val;
}

void SimMemory::load_hex_data(const std::string &data) {
    std::stringstream ss(data);
    std::string token;
    size_t mempos = 0;

    while (ss >> token) {
        if (token.empty()) {
            break;
        }
        if (token[0] == '@') {
            size_t newpos = std::stoull(token.substr(1, token.size() - 1), nullptr, 16);
            assert(newpos < MEM_SIZE);
            mempos = newpos;            
        } else {
            uint8_t byte = to_byte(token);
            mem[mempos++] = byte;
        }
    }
}