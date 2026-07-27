#include "../../include/mem/instrmem.hpp"

void InstrMemory::load_hex_data(const std::string &data) {
    mem.load_hex_data(data);
}

uint32_t InstrMemory::read_instr(size_t pc) const {
    return (mem.read_byte(pc + 3) << 24) |
           (mem.read_byte(pc + 2) << 16) |
           (mem.read_byte(pc + 1) << 8)  |
            mem.read_byte(pc);
}