#pragma once

#include "../decoder/instr.hpp"
#include "regaliastab.hpp"
#include "cdb.hpp"

#include <cstdint>
#include <array>

class ReorderBuf;

// ALU Reservation Station Entry
struct ALURSEntry {
    // is the rs busy
    bool busy;
    // the instruction type
    InstrType type;

    // --- operand Vj ---
    bool vj_ready;
    uint32_t vj;
    // the ROB tag of vj
    uint32_t qj;

    // --- operand Vk ---
    bool vk_ready;
    uint32_t vk;
    // the ROB tag of vk
    uint32_t qk;

    // the instruction's ROB tag
    uint32_t rob_tag;
    
    int32_t imm;
    uint32_t pc;
    bool executing;
    bool done;
    uint32_t result;
    bool is_branch;
    bool branch_actual_taken;
    uint32_t branch_target;
};

// ALU Reservation Station
class ALURS {
public:
    // alurs size
    constexpr static size_t ALURS_SIZE = 8;
    
private:
    // current alurs entries
    std::array<ALURSEntry, ALURS_SIZE> alurs{};
    // next state alurs entries
    std::array<ALURSEntry, ALURS_SIZE> next_alurs{};

public:
    // allocate a rs entry, read source state from rat
    // returns the slot index, or -1 for failure
    int alloc(const Instr& ins, const RegAliasTab& rat,
        uint32_t rob_tag, uint32_t pc);
    int alloc_resolved(const Instr& ins, const RATEntry& rj,
        const RATEntry& rk, uint32_t rob_tag, uint32_t pc);
    // listen to the cdb broadcast, catch if match
    void listen_cdb(uint32_t cdb_tag, uint32_t cdb_val);
    // ROB forwarding is a safety net for an RS allocated after a CDB pulse.
    void resolve_from_rob(const ReorderBuf& rob);
    // execute entries that are ready
    void execute();
    // write back done entries, returning cdb broadcast list
    std::array<CDBEntry, ALURS_SIZE> write_back();
    // free all done entries
    void free_done_entries();
    void free_entry_by_tag(uint32_t rob_tag);
    // check if alurs is full
    bool is_full() const;
    // flush all entries for restoring
    void flush_next();
    // compute next state
    void compute_next();
    // flip curr and next states
    void update();

};
