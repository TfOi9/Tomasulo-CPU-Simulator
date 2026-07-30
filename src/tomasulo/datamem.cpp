#include "../../include/tomasulo/datamem.hpp"

#include <cassert>

void SimDataMemory::load_hex_data(const std::string &data) {
    mem.load_hex_data(data);
}

void SimDataMemory::issue_read_next(size_t addr, InstrType type,
        uint32_t rob_tag) {
    if (is_busy() || pending_load.valid) return;
    pending_load = MemRq {
        true,
        true,
        false,
        type,
        addr,
        0,
        dmem_delay,
        rob_tag
    };
}

bool SimDataMemory::issue_write_next(size_t addr, uint32_t val,
        InstrType type) {
    if (is_busy() || pending_store.valid) return false;
    pending_store = MemRq {
        true,
        false,
        false,
        type,
        addr,
        val,
        0,
        0
    };
    return true;
}

void SimDataMemory::decrease_left_cycles() {
    if (!rq.valid) {
        return;
    }

    if (rq.cycles_left > 0) {
        next.cycles_left = rq.cycles_left - 1;
    } else {
        if (rq.is_load == true) {
            switch (rq.type) {
                case InstrType::LB:
                    next.val = mem.read_byte(rq.addr, false);
                    break;
                case InstrType::LBU:
                    next.val = mem.read_byte(rq.addr, true);
                    break;
                case InstrType::LH:
                    next.val = mem.read_half(rq.addr, false);
                    break;
                case InstrType::LHU:
                    next.val = mem.read_half(rq.addr, true);
                    break;
                case InstrType::LW:
                    next.val = mem.read_word(rq.addr);
                    break;
                default:
                    assert(false);
            }
            next.read_ready = true;
        } else {
            switch (rq.type) {
                case InstrType::SB:
                    mem.write_byte(rq.addr, rq.val);
                    break;
                case InstrType::SH:
                    mem.write_half(rq.addr, rq.val);
                    break;
                case InstrType::SW:
                    mem.write_word(rq.addr, rq.val);
                    break;
                default:
                    assert(false);
            }
        }
        next.valid = false;
    }
}

bool SimDataMemory::is_busy() const {
    return rq.valid || rq.read_ready;
}

bool SimDataMemory::read_ready() const {
    return rq.read_ready;
}

uint32_t SimDataMemory::read_rob_tag() const {
    assert(read_ready());
    return rq.rob_tag;
}

void SimDataMemory::consume_read_next() {
    if (rq.read_ready) next.read_ready = false;
}

uint32_t SimDataMemory::get_result() const {
    assert(read_ready());
    return rq.val;
}

void SimDataMemory::compute_next() {
    next = rq;
    pending_load = {};
    pending_store = {};
}

uint32_t SimDataMemory::resolve_requests(bool allow_load) {
    if (is_busy()) return 0;
    if (pending_store.valid) {
        next = pending_store;
        return 0;
    }
    if (allow_load && pending_load.valid) {
        next = pending_load;
        return pending_load.rob_tag;
    }
    return 0;
}

void SimDataMemory::update() {
    std::swap(rq, next);
}
