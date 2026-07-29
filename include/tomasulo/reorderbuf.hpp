#pragma once

#include "../decoder/instr.hpp"

#include <cstdint>
#include <array>

// Reorder Buffer Entry
struct ROBEntry {
    // is the entry occupied
    bool busy;
    // the instruction type
    InstrType type;
    // the destination arch register
    // for arith, load, JAL/JALR/AUIPC/LUI
    uint8_t dest_reg;
    // the calculation result
    uint32_t val;
    // is the result ready
    bool ready;
    // the instruction's program counter
    // for restoring and debugging
    uint32_t pc;

    // is the instruction a branch
    bool is_branch;
    // the predicted direction
    // true = taken
    bool branch_pred_taken;
    // the actual direction taken
    bool branch_actual_taken;
    // the correct target address
    uint32_t branch_target;

    // is the instruction a store
    bool is_store;
    // the storing address
    uint32_t store_addr;
    // the value to store
    uint32_t store_value;

    // the tag of the entry
    uint32_t tag;
};

// Reorder Buffer
class ReorderBuf {
    // ROB size
    constexpr static size_t ROB_SIZE = 32;
    // the current rob
    std::array<ROBEntry, ROB_SIZE> rob{};
    // the rob in the next cycle
    std::array<ROBEntry, ROB_SIZE> next_rob{};
    // the head and tail ptr
    uint32_t head = 0, tail = 0;
    // the head and tail ptr of next
    uint32_t head_next = 0, tail_next = 0;
    // the next tag to be allocated
    uint32_t next_tag = 1;

    // note. this cycle queue has NO unused slots
    // empty: head == tail, head.busy == false;
    // full:  head == tail, head.busy == true.

public:
    // --- interface for Issue phase ---
    // try allocate one ROB entry
    // returns tag( = tail) if success, else -1
    int alloc(InstrType type, uint8_t dest_reg, uint32_t pc,
        bool is_branch, bool branch_pred_taken,
        uint32_t branch_target, bool is_store);
    // is the current queue full
    bool is_full() const;
    // is the next queue full
    bool is_next_full() const;
    // is the current queue empty
    bool is_empty() const;

    // --- interface for WriteBack phase ---
    // write the result into ROB after executing
    void write_result(uint32_t tag, uint32_t val);
    // write branch result
    void write_branch_result(uint32_t tag, bool taken,
        uint32_t target);
    // write store result
    void write_store_result(uint32_t tag, uint32_t addr,
        uint32_t val);

    // --- interface for Commit phase ---
    // check if head can commit
    bool can_commit() const;
    // return head
    const ROBEntry& head_entry() const;
    // confirm commit. returns true if a branch mispredict occurred
    bool commit();

    // --- mistaken prediction & restore ---
    // flush all entries in branch_tag+1..=tail-1, clear tail
    void flush_from_next(uint32_t branch_tag);

    // --- update ---
    // calculate next state
    void compute_next();
    // flip curr and next
    void update();

};