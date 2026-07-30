#pragma once

#include "../decoder/instr.hpp"
#include "regaliastab.hpp"
#include "datamem.hpp"
#include "cdb.hpp"
#include "storebuf.hpp"

#include <cstdint>
#include <array>

// Load & Save Reservation Station Entry
struct LSRSEntry {
    // is the rs busy
    bool busy;
    // the instruction type
    InstrType type;
    // is this a load instruction
    bool is_load;

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
    bool addr_ready;
    uint32_t addr;
    bool mem_requested;
    bool done;
    uint32_t mem_result;
};

// Load & Save Reservation Station
class LSRS {
public:
    // lsrs size
    constexpr static size_t LSRS_SIZE = 8;

private:
    // current lsrs entries
    std::array<LSRSEntry, LSRS_SIZE> lsrs{};
    // next state lsrs entries
    std::array<LSRSEntry, LSRS_SIZE> next_lsrs{};
    // helper: check if older store conflicts with load
    bool older_store_conflict(const LSRSEntry& load_entry);

public:
    // allocates a new lsrs entry
    // returns the slot index, or -1 for failure
    int alloc(const Instr& ins, const RegAliasTab& rat,
        uint32_t rob_tag, uint32_t pc);
    int alloc_resolved(const Instr& ins, const RATEntry& rj,
        const RATEntry& rk, uint32_t rob_tag, uint32_t pc);
    // listen to the cdb broadcast, catch if match
    void listen_cdb(const CDBEntry& cdb);
    // execute entries that are ready
    void execute(StoreBuffer& sb, SimDataMemory& dmem);
    void mark_mem_requested(uint32_t rob_tag);
    // return the oldest completed load
    CDBEntry writeback_candidate() const;
    // fetch an lsrs entry
    const LSRSEntry& get_entry(size_t idx) const;
    void free_entry_by_tag(uint32_t rob_tag);
    // check if lsrs is full
    bool is_full() const;
    // flush all entries for restoring
    void flush_next();
    // compute next state
    void compute_next();
    // flip curr and next states
    void update();

};
