#pragma once

#include "../decoder/instr.hpp"
#include "../mem/datamem.hpp"

#include <cstdint>
#include <string>

// Data Memory Request
struct MemRq {
    // valid flag
    bool valid;
    // true = load, false = save
    bool is_load;
    // read ready flag
    bool read_ready;
    // instruction type: LB/LBU/LH/LHU/LW/SB/SH/SW
    InstrType type;
    // address of related memory
    size_t addr;
    // value to store / read value
    uint32_t val;
    // pending cycles left
    // we assume that load has to wait 3 cycles
    // while store finishes instantly at commit
    uint32_t cycles_left;
};

// Data Memory Simulator
// simulates memory with only one data wire with latency
class SimDataMemory {
    // the simulated memory
    DataMemory mem;
    // the current request
    MemRq rq;
    // the request in the next cycle
    MemRq next;

public:
    // load hexdecimal memory data
    void load_hex_data(const std::string& data);
    // issue a read request
    void issue_read_next(size_t addr, InstrType type);
    // issue a write request
    void issue_write_next(size_t addr, uint32_t val, InstrType type);
    // decrease the left cycles
    void decrease_left_cycles();
    // check if exists a pending request
    bool is_busy() const;
    // check if read result is ready
    bool read_ready() const;
    // return the read result
    uint32_t get_result() const;
    // flip the two states
    void update();

};