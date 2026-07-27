#pragma once

#include "simmem.hpp"

// Instruction Memory class
// designed for both simulator and interpreter
class InstrMemory {
    // simulated memory space
    SimMemory mem;

public:
    // load raw data from a string
    void load_hex_data(const std::string& data);
    // read out a single instruction
    uint32_t read_instr(size_t pc) const;

};