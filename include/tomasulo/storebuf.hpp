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
    std::array<SBEntry, SB_SIZE> sb{};
    // next state store buffer
    std::array<SBEntry, SB_SIZE> next_sb{};

public:
    // insert a store entry
    int insert_next(uint32_t rob_tag, InstrType type);
    // mark the address ready
    void set_addr_ready_next(uint32_t rob_tag, uint32_t addr);
    // mark the value ready
    void set_val_ready_next(uint32_t rob_tag, uint32_t val);
    
    // --- store & load conflict check ---
    // check if we can relay a value from sb
    bool try_forward(uint32_t load_addr, uint32_t load_rob_tag,
        uint32_t& forward_val) const;
    // check if load has to wait
    bool must_stall_for_load(uint32_t load_addr,
        uint32_t load_rob_tag) const;
    // commit, clearing sb entry
    void commit_next(uint32_t rob_tag);
    // restore from a branch prediction error
    void flush_from_next(uint32_t flush_tag);
    // check if all store has known addresses
    bool all_addrs_known() const;

    // check if sb is full
    bool is_full() const;
    // compute next state
    void compute_next();
    // flip curr and next states
    void update();

};