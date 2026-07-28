#pragma once

#include <cstdint>
#include <array>
#include <string>

// Architecture Register File
// keeps two groups of physical registers
class ArchRegFile {
    // file size
    constexpr static size_t FILE_SIZE = 32;
    // current state registers
    std::array<uint32_t, FILE_SIZE> regs;
    // next state registers
    std::array<uint32_t, FILE_SIZE> new_regs;

public:
    // prepare next state as copy of current state
    void prepare_next();
    // flush zero into x0 in next
    void flush_zero();
    // read register data from current state
    uint32_t read_reg(size_t index) const;
    // write the value into a next state register
    void write_reg(size_t index, uint32_t val);
    // update the state, flipping curr and next
    void update();
    // dump all the registers for debugging
    std::string dump() const;

};

const std::string reg_indices[] = {
    "x0", "x1", "x2", "x3",
    "x4", "x5", "x6", "x7",
    "x8", "x9", "x10", "x11",
    "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19",
    "x20", "x21", "x22", "x23",
    "x24", "x25", "x26", "x27",
    "x28", "x29", "x30", "x31",
};

const std::string reg_names[] = {
    "zero", "ra", "sp", "gp",
    "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1",
    "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3",
    "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11",
    "t3", "t4", "t5", "t6",
};