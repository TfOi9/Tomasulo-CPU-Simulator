#include "../../include/tomasulo/storebuf.hpp"

int StoreBuffer::insert_next(uint32_t rob_tag, InstrType type) {
    for (size_t i = 0; i < SB_SIZE; i++) {
        if (!next_sb[i].busy) {
            next_sb[i] = {
                true,
                rob_tag,
                0,
                0,
                false,
                false,
                type
            };
            return i;
        }
    }
    return -1;
}

void StoreBuffer::set_addr_ready_next(uint32_t rob_tag, uint32_t addr) {
    for (size_t i = 0; i < SB_SIZE; i++) {
        if (next_sb[i].busy && next_sb[i].rob_tag == rob_tag) {
            next_sb[i].addr = addr;
            next_sb[i].addr_ready = true;
            break;
        }
    }
}

void StoreBuffer::set_val_ready_next(uint32_t rob_tag, uint32_t val) {
    for (size_t i = 0; i < SB_SIZE; i++) {
        if (next_sb[i].busy && next_sb[i].rob_tag == rob_tag) {
            next_sb[i].val = val;
            next_sb[i].val_ready = true;
            break;
        }
    }
}

bool StoreBuffer::try_forward(uint32_t load_addr,
        uint32_t load_rob_tag, uint32_t &forward_val) const {
    // find an earlier entry with address identcal to load_addr
    uint32_t tag = 0;
    bool found = false;

    for (size_t i = 0; i < SB_SIZE; i++) {
        const SBEntry& entry = sb[i];
        if (!entry.busy) continue;
        if (!entry.addr_ready || !entry.val_ready) continue;
        if (entry.rob_tag >= load_rob_tag) continue;
        if (entry.addr != load_addr) continue;

        if (entry.rob_tag > tag) {
            tag = entry.rob_tag;
            forward_val = entry.val;
            found = true;
        }
    }
    return found;
}

bool StoreBuffer::must_stall_for_load(uint32_t load_addr,
        uint32_t load_rob_tag) const {
    for (size_t i = 0; i < SB_SIZE; i++) {
        const SBEntry& entry = sb[i];
        if (!entry.busy) continue;
        if (entry.rob_tag >= load_rob_tag) continue;

        if (!entry.addr_ready) return true;
        if (entry.addr == load_addr && !entry.val_ready) {
            return true;
        }
    }
    return false;
}

void StoreBuffer::commit_next(uint32_t rob_tag) {
    for (size_t i = 0; i < SB_SIZE; i++) {
        SBEntry& entry = next_sb[i];
        if (!entry.busy) continue;
        if (entry.rob_tag != rob_tag) continue;

        entry.busy = false;
    }
}

void StoreBuffer::flush_from_next(uint32_t flush_tag) {
    for (size_t i = 0; i < SB_SIZE; i++) {
        if (sb[i].busy && sb[i].rob_tag > flush_tag) {
            next_sb[i].busy = false;
        }
    }
}

bool StoreBuffer::all_addrs_known() const {
    for (auto& entry : sb) {
        if (entry.busy && !entry.addr_ready) return false;
    }
    return true;
}

bool StoreBuffer::is_full() const {
    for (size_t i = 0; i < SB_SIZE; i++) {
        if (!next_sb[i].busy) return false;
    }
    return true;
}

void StoreBuffer::compute_next() {
    next_sb = sb;
}

void StoreBuffer::update() {
    std::swap(sb, next_sb);
}