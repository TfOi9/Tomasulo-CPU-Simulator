#include "../../include/tomasulo/lsrs.hpp"

#include <cassert>

int LSRS::alloc(const Instr &ins, const RegAliasTab &rat,
        uint32_t rob_tag, uint32_t pc) {
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

            RATEntry rj = rat.read(ins.rs1);
            RATEntry rk = rat.read(ins.rs2);

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

void LSRS::execute(StoreBuffer &sb, SimDataMemory &dmem) {
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
        if (older_store_conflict(entry)) {
            continue;
        }
        
        uint32_t fwd_val = 0;
        if (sb.try_forward(entry.addr, entry.rob_tag, fwd_val)) {
            entry.mem_result = fwd_val;
            entry.done = true;
            continue;
        }
        if (sb.must_stall_for_load(entry.addr, entry.rob_tag)) continue;

        if (!entry.mem_requested && !dmem.is_busy()) {
            dmem.issue_read_next(entry.addr, entry.type);
            entry.mem_requested = true;
        }

        if (entry.mem_requested && dmem.read_ready()) {
            entry.mem_result = dmem.get_result();
            entry.done = true;
        }
    }
}

bool LSRS::older_store_conflict(const LSRSEntry& load_entry) {
    for (size_t i = 0; i < LSRS_SIZE; i++) {
        LSRSEntry& entry = next_lsrs[i];
        if (!entry.busy || entry.done || entry.is_load) continue;
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
        if (next_lsrs[i].done && next_lsrs[i].is_load) {
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
    }
}

void LSRS::compute_next() {
    next_lsrs = lsrs;
}

void LSRS::update() {
    std::swap(lsrs, next_lsrs);
}