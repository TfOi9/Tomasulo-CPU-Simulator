#pragma once

#include "regfile.hpp"
#include <cstdint>
#include <cstddef>
#include <array>

// RAT Entry
struct RATEntry {
    // is the reg ready
    // true = value is in arch regfile
    bool ready;
    // the value if ready
    uint32_t val;
    // which ROB entry will write the reg if not ready
    uint32_t tag;
};

// Register Alias Table
class RegAliasTab {
    // RAT size
    constexpr static size_t RAT_SIZE = 32;
    // current RAT
    std::array<RATEntry, 32> rat;
    // next state RAT
    std::array<RATEntry, 32> next_rat;

public:
    // default constructor
    RegAliasTab();
    // read out a single entry
    RATEntry read(size_t index) const;
    // map the target register to the ROB tag
    // used in the Issue stage
    void set_tag_next(uint8_t dest_reg, uint32_t rob_tag);
    // commit a change, clearing the rob_tag
    // used in the Commit stage
    void commit_clear_next(uint8_t reg, uint32_t rob_tag,
        uint32_t val);
    // restore a broken branch
    void restore_from_next(uint32_t flush_tag,
        const ArchRegFile& arch);
    // compute next stage
    void compute_next();
    // flip curr and next
    void update();
};
