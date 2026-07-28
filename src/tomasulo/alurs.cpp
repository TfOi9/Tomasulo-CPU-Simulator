#include "../../include/tomasulo/alurs.hpp"
#include "../../include/alu/alucomp.hpp"

#include <cassert>

int ALURS::alloc(const Instr &ins, const RegAliasTab &rat,
        uint32_t rob_tag, uint32_t pc) {
    for (size_t i = 0; i < ALURS_SIZE; i++) {
        if (next_alurs[i].busy) {
            continue;
        }

        RATEntry rj = rat.read(ins.rs1);
        RATEntry rk = rat.read(ins.rs2);

        ALURSEntry entry {
            true,
            ins.header.type,
            rj.ready,
            rj.val,
            rj.tag,
            rk.ready,
            rk.val,
            rk.tag,
            rob_tag,
            ins.imm,
            pc,
            false,
            false,
            0,
            ins.header.place == InstrPlace::BRANCH
        };

        next_alurs[i] = entry;
        return i;
    }
    return -1;
}

void ALURS::listen_cdb(uint32_t cdb_tag, uint32_t cdb_val) {
    for (size_t i = 0; i < ALURS_SIZE; i++) {
        if (!next_alurs[i].busy) {
            continue;
        }
        if (next_alurs[i].qj == cdb_tag) {
            next_alurs[i].vj_ready = true;
            next_alurs[i].vj = cdb_val;
            next_alurs[i].qj = 0;
        }
        if (next_alurs[i].qk == cdb_tag) {
            next_alurs[i].vk_ready = true;
            next_alurs[i].vk = cdb_val;
            next_alurs[i].qk = 0;
        }
    }
}

void ALURS::execute() {
    for (size_t i = 0; i < ALURS_SIZE; i++) {
        ALURSEntry& entry = next_alurs[i];
        if (!entry.busy || entry.done) continue;
        if (entry.qj || entry.qk) continue;
        if (entry.is_branch) {
            switch (entry.type) {
                case InstrType::BEQ:
                    entry.branch_actual_taken = (entry.vj == entry.vk);
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::BGE:
                    entry.branch_actual_taken = (int32_t(entry.vj) >= int32_t(entry.vk));
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::BGEU:
                    entry.branch_actual_taken = (entry.vj >= entry.vk);
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::BLT:
                    entry.branch_actual_taken = (int32_t(entry.vj) < int32_t(entry.vk));
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::BLTU:
                    entry.branch_actual_taken = (entry.vj < entry.vk);
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::BNE:
                    entry.branch_actual_taken = (entry.vj != entry.vk);
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::JAL:
                    entry.result = entry.pc + 4;
                    entry.branch_actual_taken = true;
                    entry.branch_target = entry.pc + entry.imm;
                    break;
                case InstrType::JALR:
                    entry.result = entry.pc + 4;
                    entry.branch_actual_taken = true;
                    entry.branch_target = (entry.vj + entry.imm) & ~1u;
                    break;
                default:
                    assert(false);
            }
        } else {
            InstrClass clas = instr_class_mapping[static_cast<int>(entry.type)];
            if (clas == InstrClass::R) {
                entry.result = alu_comp(entry.type, entry.vj, entry.vk);
            } else if (clas == InstrClass::I) {
                entry.result = alu_compi(entry.type, entry.vj, entry.imm);
            } else {
                assert(clas == InstrClass::U);
                if (entry.type == InstrType::AUIPC) {
                    entry.result = entry.pc + entry.imm;
                } else if (entry.type == InstrType::LUI) {
                    entry.result = entry.imm;
                } else {
                    assert(false);
                }
            }
        }
        entry.done = true;
    }
}

std::array<CDBEntry, ALURS::ALURS_SIZE> ALURS::write_back() {
    std::array<CDBEntry, ALURS_SIZE> arr;
    int index = 0;
    for (size_t i = 0; i < ALURS_SIZE; i++) {
        if (next_alurs[i].done) {
            ALURSEntry& entry = next_alurs[i];
            arr[index] = {
                true,
                entry.rob_tag,
                entry.result,
                entry.is_branch,
                entry.branch_actual_taken,
                entry.branch_target
            };
            index++;
        }
    }
    return arr;
}

bool ALURS::is_full() const {
    for (size_t i = 0; i < ALURS_SIZE; i++) {
        if (!alurs[i].busy) return false;
    }
    return true;
}

void ALURS::flush_next() {
    for (size_t i = 0; i < ALURS_SIZE; i++) {
        next_alurs[i].busy = false;
    }
}

void ALURS::compute_next() {
    next_alurs = alurs;
}

void ALURS::update() {
    std::swap(alurs, next_alurs);
}