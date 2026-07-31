#include "../../include/tomasulo/lsrs.hpp"

#include <cassert>

namespace {
uint32_t load_value(InstrType type, uint32_t value) {
    switch (type) {
        case InstrType::LB: {
            uint32_t byte = value & 0xffu;
            return (byte & 0x80u) ? byte | 0xffffff00u : byte;
        }
        case InstrType::LBU:
            return value & 0xffu;
        case InstrType::LH: {
            uint32_t half = value & 0xffffu;
            return (half & 0x8000u) ? half | 0xffff0000u : half;
        }
        case InstrType::LHU:
            return value & 0xffffu;
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
        if (!lsrs[i].busy) {
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

void LSRS::listen_cdb(const CDBEntry& cdb) {
    if (!cdb.valid) return;

    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (!lsrs[i].busy) {
            continue;
        }
        if (lsrs[i].qj == cdb.rob_tag) {
            next_lsrs[i].vj_ready = true;
            next_lsrs[i].vj = cdb.val;
            next_lsrs[i].qj = 0;
        }
        if (lsrs[i].qk == cdb.rob_tag) {
            next_lsrs[i].vk_ready = true;
            next_lsrs[i].vk = cdb.val;
            next_lsrs[i].qk = 0;
        }
    }
}

void LSRS::execute(StoreBuffer &sb, SimDataMemory &dmem) {
    bool response_consumed = dmem.read_ready();
    if (response_consumed) {
        uint32_t owner = dmem.read_rob_tag();
        for (size_t i = 0; i < LSRS_SIZE; ++i) {
            const LSRSEntry& entry = lsrs[i];
            if (entry.busy && entry.is_load && entry.mem_requested &&
                !entry.done && entry.rob_tag == owner) {
                next_lsrs[i].mem_result = dmem.get_result();
                next_lsrs[i].done = true;
                next_lsrs[i].mem_requested = false;
                break;
            }
        }
        dmem.consume_read_next();
    }

    size_t forwarded_load = LSRS_SIZE;
    uint32_t forwarded_value = 0;
    size_t memory_load = LSRS_SIZE;
    uint32_t memory_addr = 0;

    for (size_t i = 0; i < LSRS_SIZE; i++) {
        const LSRSEntry& current = lsrs[i];
        if (!current.busy || current.done) continue;
        LSRSEntry entry = current;

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
            if (next_lsrs[i].busy &&
                next_lsrs[i].rob_tag == current.rob_tag) {
                next_lsrs[i].addr = entry.addr;
                next_lsrs[i].addr_ready = true;
            }
            if (!entry.is_load) {
                sb.set_addr_ready_next(entry.rob_tag, entry.addr);
            }
        }

        // handle store
        if (!entry.is_load) {
            if (entry.addr_ready && entry.vk_ready && !entry.done) {
                sb.set_val_ready_next(entry.rob_tag, entry.vk);
                if (next_lsrs[i].busy &&
                    next_lsrs[i].rob_tag == current.rob_tag) {
                    next_lsrs[i].done = true;
                }
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
            if (!response_consumed &&
                (forwarded_load == LSRS_SIZE ||
                 entry.rob_tag < lsrs[forwarded_load].rob_tag)) {
                forwarded_load = i;
                forwarded_value = load_value(entry.type, fwd_val);
            }
            continue;
        }
        if (sb.must_stall_for_load(entry.addr, entry.rob_tag)) continue;

        if (!dmem.is_busy() &&
            (memory_load == LSRS_SIZE ||
             entry.rob_tag < lsrs[memory_load].rob_tag)) {
            memory_load = i;
            memory_addr = entry.addr;
        }
    }

    if (forwarded_load != LSRS_SIZE) {
        const LSRSEntry& entry = lsrs[forwarded_load];
        if (next_lsrs[forwarded_load].busy &&
            next_lsrs[forwarded_load].rob_tag == entry.rob_tag) {
            next_lsrs[forwarded_load].mem_result = forwarded_value;
            next_lsrs[forwarded_load].done = true;
        }
    }

    if (memory_load != LSRS_SIZE) {
        const LSRSEntry& entry = lsrs[memory_load];
        dmem.issue_read_next(memory_addr, entry.type, entry.rob_tag);
    }

    size_t new_load_completions = 0;
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (lsrs[i].busy && lsrs[i].is_load && !lsrs[i].done &&
            next_lsrs[i].done) {
            new_load_completions++;
        }
    }
    assert(new_load_completions <= 1);
}

void LSRS::mark_mem_requested(uint32_t rob_tag) {
    for (size_t i = 0; i < LSRS_SIZE; ++i) {
        if (lsrs[i].busy && lsrs[i].rob_tag == rob_tag &&
            next_lsrs[i].busy && next_lsrs[i].rob_tag == rob_tag) {
            next_lsrs[i].mem_requested = true;
            return;
        }
    }
}

bool LSRS::older_store_conflict(const LSRSEntry& load_entry) {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        const LSRSEntry& entry = lsrs[i];
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

CDBEntry LSRS::writeback_candidate() const {
    const LSRSEntry* oldest = nullptr;
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        const LSRSEntry& entry = lsrs[i];
        if (!entry.busy || !entry.done || !entry.is_load) continue;
        if (oldest == nullptr || entry.rob_tag < oldest->rob_tag) {
            oldest = &entry;
        }
    }

    if (oldest == nullptr) return {};
    return {true, oldest->rob_tag, oldest->mem_result, false, false, 0};
}

const LSRSEntry& LSRS::get_entry(size_t idx) const {
    assert(idx < LSRS_SIZE);
    return lsrs[idx];
}

void LSRS::free_entry_by_tag(uint32_t rob_tag) {
    for (size_t i = 0; i < LSRS_SIZE; ++i) {
        if (lsrs[i].busy && lsrs[i].rob_tag == rob_tag &&
            next_lsrs[i].rob_tag == rob_tag) {
            next_lsrs[i].busy = false;
            next_lsrs[i].done = false;
            return;
        }
    }
}

bool LSRS::is_full() const {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        if (!lsrs[i].busy) return false;
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
