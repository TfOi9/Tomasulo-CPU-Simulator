#include "../../include/tomasulo/lsrs.hpp"
#include "../../include/tomasulo/reorderbuf.hpp"

#include <cassert>

namespace {
uint32_t load_value(InstrType type, uint32_t value) {
    switch (type) {
        // Keep forwarding bit-for-bit consistent with DataMemory's existing
        // API convention (the boolean passed by the interpreter is inverted
        // relative to the ISA mnemonic names).
        case InstrType::LB:  return value & 0xffu;
        case InstrType::LBU: return uint32_t(int32_t(int8_t(value)));
        case InstrType::LH:  return value & 0xffffu;
        case InstrType::LHU: return uint32_t(int32_t(int16_t(value)));
        case InstrType::LW:  return value;
        default: assert(false);
    }
}
}

int LSRS::alloc(const Instr &ins, const RegAliasTab &rat,
        uint32_t rob_tag, uint32_t pc) {
    return alloc_resolved(ins, rat.read(ins.rs1), rat.read(ins.rs2), rob_tag, pc);
}

int LSRS::alloc_resolved(const Instr &ins, const RATEntry &rj,
        const RATEntry &rk, uint32_t rob_tag, uint32_t pc) {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (!next_lsrs[i].busy) {
            bool is_load = false;
            switch (ins.header.type) {
                case InstrType::LB:
                case InstrType::LBU:
                case InstrType::LH:
                case InstrType::LHU:
                case InstrType::LW:
                    is_load = true;
                    break;
                default:
                    break;
            }

            bool addr_ready = false;
            uint32_t addr = 0;
            if (rj.ready) {
                addr = rj.val + uint32_t(ins.imm);
                addr_ready = true;
            }

            next_lsrs[i] = {
                true,
                ins.header.type,
                is_load,
                rj.ready,
                rj.val,
                rj.tag,
                rk.ready,
                rk.val,
                rk.tag,
                rob_tag,
                ins.imm,
                addr_ready,
                addr,
                false,
                false,
                0
            };
            return i;
        }
    }
    return -1;
}

void LSRS::listen_cdb(uint32_t cdb_tag, uint32_t cdb_val) {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (!next_lsrs[i].busy) {
            continue;
        }
        if (next_lsrs[i].qj == cdb_tag) {
            next_lsrs[i].vj_ready = true;
            next_lsrs[i].vj = cdb_val;
            next_lsrs[i].qj = 0;
        }
        if (next_lsrs[i].qk == cdb_tag) {
            next_lsrs[i].vk_ready = true;
            next_lsrs[i].vk = cdb_val;
            next_lsrs[i].qk = 0;
        }
    }
}

void LSRS::resolve_from_rob(const ReorderBuf& rob) {
    for (LSRSEntry& entry : next_lsrs) {
        if (!entry.busy) continue;
        uint32_t value = 0;
        if (entry.qj != 0 && rob.get_result_if_ready(entry.qj, value)) {
            entry.vj_ready = true;
            entry.vj = value;
            entry.qj = 0;
        }
        if (entry.qk != 0 && rob.get_result_if_ready(entry.qk, value)) {
            entry.vk_ready = true;
            entry.vk = value;
            entry.qk = 0;
        }
    }
}

void LSRS::execute(StoreBuffer &sb, SimDataMemory &dmem) {
    // A completed request belongs to exactly one RS entry.  Consume it before
    // considering a new request: otherwise a newly-issued load can observe
    // the previous request's read_ready pulse and steal its value.
    if (dmem.read_ready()) {
        for (LSRSEntry& entry : next_lsrs) {
            if (entry.busy && entry.is_load && entry.mem_requested && !entry.done) {
                entry.mem_result = dmem.get_result();
                entry.done = true;
                entry.mem_requested = false;
                break;
            }
        }
    }

    for (size_t i = 0; i < LSRS_SIZE; i++) {
        LSRSEntry& entry = next_lsrs[i];
        if (!entry.busy || entry.done) continue;

        // update vj and vk status
        if (!entry.vj_ready && entry.qj == 0) {
            entry.vj_ready = true;
        }
        if (!entry.vk_ready && entry.qk == 0) {
            entry.vk_ready = true;
        }

        // calculate address
        if (!entry.addr_ready && entry.vj_ready) {
            entry.addr = entry.vj + uint32_t(entry.imm);
            entry.addr_ready = true;
            if (!entry.is_load) {
                sb.set_addr_ready_next(entry.rob_tag, entry.addr);
            }
        }

        // handle store
        if (!entry.is_load) {
            if (entry.addr_ready && entry.vk_ready && !entry.done) {
                sb.set_val_ready_next(entry.rob_tag, entry.vk);
                entry.done = true;
            }
            continue;
        }

        // handle load
        if (!entry.addr_ready) continue;
        if (entry.mem_requested) continue;
        if (older_store_conflict(entry)) {
            continue;
        }
        
        uint32_t fwd_val = 0;
        if (sb.try_forward(entry.addr, entry.rob_tag, fwd_val)) {
            entry.mem_result = load_value(entry.type, fwd_val);
            entry.done = true;
            continue;
        }
        if (sb.must_stall_for_load(entry.addr, entry.rob_tag)) continue;

        if (!dmem.read_ready() && !dmem.is_busy()) {
            dmem.issue_read_next(entry.addr, entry.type);
            entry.mem_requested = true;
        }
    }
}

bool LSRS::older_store_conflict(const LSRSEntry& load_entry) {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        LSRSEntry& entry = next_lsrs[i];
        if (!entry.busy || entry.is_load) continue;
        if (entry.rob_tag >= load_entry.rob_tag) continue;

        if (!entry.addr_ready) {
            return true;
        }
        if (entry.addr == load_entry.addr) {
            return true;
        }
    }
    return false;
}

std::array<CDBEntry, LSRS::LSRS_SIZE> LSRS::write_back() {
    std::array<CDBEntry, LSRS_SIZE> arr{};
    int index = 0;
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (!next_lsrs[i].busy || !next_lsrs[i].done || !next_lsrs[i].is_load) continue;
        LSRSEntry& entry = next_lsrs[i];
        arr[index] = {
            true,
            entry.rob_tag,
            entry.mem_result,
            false,
            false,
            0
        };
        index++;
    }
    return arr;
}

const LSRSEntry& LSRS::get_entry(size_t idx) const {
    assert(idx < LSRS_SIZE);
    return next_lsrs[idx];
}

void LSRS::free_done_entries() {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (next_lsrs[i].done) {
            next_lsrs[i].busy = false;
            next_lsrs[i].done = false;
        }
    }
}

void LSRS::free_entry_by_tag(uint32_t rob_tag) {
    for (LSRSEntry& entry : next_lsrs) {
        if (entry.busy && entry.rob_tag == rob_tag) {
            entry.busy = false;
            entry.done = false;
            return;
        }
    }
}

bool LSRS::is_full() const {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (!next_lsrs[i].busy) return false;
    }
    return true;
}

void LSRS::flush_next() {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        next_lsrs[i].busy = false;
        next_lsrs[i].done = false;
    }
}

void LSRS::compute_next() {
    next_lsrs = lsrs;
}

void LSRS::update() {
    std::swap(lsrs, next_lsrs);
}
