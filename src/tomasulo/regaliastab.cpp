#include "../../include/tomasulo/regaliastab.hpp"

#include <cassert>

RegAliasTab::RegAliasTab() {
    for (size_t i = 1; i < RAT_SIZE; i++) {
        rat[i] = {true, 0, 0};
        next_rat[i] = {true, 0, 0};
    }
    rat[0] = {true, 0, 0};
    next_rat[0] = {true, 0, 0};
}

RATEntry RegAliasTab::read(size_t index) const {
    assert(index < RAT_SIZE);
    return rat[index];
}

void RegAliasTab::set_tag_next(uint8_t dest_reg, uint32_t rob_tag) {
    assert(dest_reg < RAT_SIZE);
    if (dest_reg == 0) return;
    next_rat[dest_reg] = {false, 0, rob_tag};
}

void RegAliasTab::commit_clear_next(uint8_t reg, uint32_t rob_tag, uint32_t val) {
    assert(reg < RAT_SIZE);
    if (reg == 0) return;
    if (next_rat[reg].ready == false && next_rat[reg].tag == rob_tag) {
        next_rat[reg] = {true, val, 0};
    }
}

void RegAliasTab::restore_from_next(uint32_t flush_tag, const ArchRegFile &arch) {
    for (size_t i = 0; i < RAT_SIZE; i++) {
        if (next_rat[i].tag > flush_tag) {
            next_rat[i] = {true, arch.read_reg(i), 0};
        }
    }
    next_rat[0] = {true, 0, 0};
}

void RegAliasTab::compute_next() {
    for (size_t i = 0; i < RAT_SIZE; i++) {
        next_rat[i] = rat[i];
    }
}

void RegAliasTab::update() {
    std::swap(rat, next_rat);
}