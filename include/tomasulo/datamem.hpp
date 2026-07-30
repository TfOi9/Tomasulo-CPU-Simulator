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
    // ROB owner for a load response (zero for stores/no request)
    uint32_t rob_tag;
};

// Data Memory Simulator
// simulates memory with only one data wire with latency
class SimDataMemory {
    // the simulated memory
    DataMemory mem;
    // the current request
    MemRq rq{};
    // the request in the next cycle
    MemRq next{};
    // independent phase outputs, arbitrated after all phases compute
    MemRq pending_load{};
    MemRq pending_store{};

public:
    // load hexdecimal memory data
    void load_hex_data(const std::string& data);
    // issue a read request
    void issue_read_next(size_t addr, InstrType type, uint32_t rob_tag);
    // issue a write request
    bool issue_write_next(size_t addr, uint32_t val, InstrType type);
    // decrease the left cycles
    void decrease_left_cycles();
    // check if exists a pending request
    bool is_busy() const;
    // check if read result is ready
    bool read_ready() const;
    uint32_t read_rob_tag() const;
    void consume_read_next();
    // return the read result
    uint32_t get_result() const;
    void compute_next();
    // Returns the accepted load's ROB tag, or zero if no load was accepted.
    uint32_t resolve_requests(bool allow_load = true);
    // flip the two states
    void update();

};
