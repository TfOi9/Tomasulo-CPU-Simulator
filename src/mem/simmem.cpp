#include "../../include/mem/simmem.hpp"
#include "../../include/ints.hpp"

#include <cassert>
#include <sstream>
#include <string>

uint8_t SimMemory::read_byte(size_t index) const {
    assert(index >= 0 && index < MEM_SIZE);
    return mem[index];
}

void SimMemory::write_byte(size_t index, uint8_t val) {
    assert(index >= 0 && index < MEM_SIZE);
    mem[index] = val;
}

void SimMemory::load_hex_data(const std::string &data) {
    size_t len = data.size();
    std::stringstream ss(data);
    std::string token;
    size_t mempos = 0;

    while (ss >> token) {
        if (token.empty()) {
            break;
        }
        if (token[0] == '@') {
            size_t newpos = std::stoull(token.substr(1, token.size() - 1));
            assert(newpos > 0 && newpos < MEM_SIZE);
            mempos = newpos;            
        } else {
            uint8_t byte = to_byte(token);
            mem[mempos++] = byte;
        }
    }
}