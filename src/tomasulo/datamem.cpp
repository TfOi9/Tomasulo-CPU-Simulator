#include "../../include/tomasulo/datamem.hpp"

#include <cassert>

void SimDataMemory::load_hex_data(const std::string &data) {
    mem.load_hex_data(data);
}

void SimDataMemory::issue_read_next(size_t addr, InstrType type) {
    assert(!rq.valid && !next.valid);

    next = MemRq {
        true,
        true,
        false,
        type,
        addr,
        0,
        3
    };
}

void SimDataMemory::issue_write_next(size_t addr, uint32_t val, InstrType type) {
    assert(!rq.valid && !next.valid);

    next = MemRq {
        true,
        false,
        false,
        type,
        addr,
        val,
        0
    };
}

void SimDataMemory::decrease_left_cycles() {
    if (!rq.valid) {
        return;
    }

    if (rq.cycles_left > 0) {
        rq.cycles_left--;
    } else {
        if (rq.is_load == true) {
            switch (rq.type) {
                case InstrType::LB:
                    rq.val = mem.read_byte(rq.addr, false);
                    break;
                case InstrType::LBU:
                    rq.val = mem.read_byte(rq.addr, true);
                    break;
                case InstrType::LH:
                    rq.val = mem.read_half(rq.addr, false);
                    break;
                case InstrType::LHU:
                    rq.val = mem.read_half(rq.addr, true);
                    break;
                case InstrType::LW:
                    rq.val = mem.read_word(rq.addr);
                    break;
                default:
                    assert(false);
            }
            rq.read_ready = true;
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
        rq.valid = false;
    }
}

bool SimDataMemory::is_busy() const {
    return rq.valid || next.valid;
}

bool SimDataMemory::read_ready() const {
    return rq.read_ready;
}

uint32_t SimDataMemory::get_result() const {
    assert(read_ready());
    return rq.val;
}

void SimDataMemory::update() {
    std::swap(rq, next);
}