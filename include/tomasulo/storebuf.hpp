#pragma once

#include "../decoder/instr.hpp"

#include <cstdint>
#include <cstddef>
#include <array>

// Store Buffer Entry
struct SBEntry {
    bool busy;
    uint32_t rob_tag;
    uint32_t addr;
    uint32_t val;
    bool addr_ready;
    bool val_ready;
    InstrType type;
};

// Store Buffer
struct StoreBuffer {
    // store buffer size
    constexpr static size_t SB_SIZE = 8;
    // current store buffer
    std::array<SBEntry, SB_SIZE> sb;
    // next state store buffer
    std::array<SBEntry, SB_SIZE> next_sb;

public:
    
};