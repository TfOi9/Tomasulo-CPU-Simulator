#include "../../include/interpreter/interpreter.hpp"

#include <fstream>
#include <sstream>
#include <cassert>

#include "../../include/decoder/instr.hpp"
#include "../../include/alu/alucomp.hpp"

void CPUInterpreter::load_program(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        assert(false);
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    imem.load_hex_data(ss.str());
    dmem.load_hex_data(ss.str());

    pc = 0;
    halted = false;
}

void CPUInterpreter::step() {
    // fetch the instruction from imem
    uint32_t raw = imem.read_instr(pc);
    Instr ins = Instr::decode(raw);

    // check halt
    if (ins.header.type == InstrType::HALT) {
        halted = true;
        return;
    }

    // fetch the desired registers
    uint32_t src1 = regf.read_reg(ins.rs1);
    uint32_t src2 = regf.read_reg(ins.rs2);

    // temporary register
    uint32_t temp;

    // pc increment flag
    bool pcif = true;

    // run the instruction
    switch (ins.header.place) {
        // alu operation
        case InstrPlace::ALU:
            if (ins.header.clas == InstrClass::R) {
                temp = alu_comp(ins.header.type, src1, src2);
            } else if (ins.header.clas == InstrClass::I) {
                temp = alu_compi(ins.header.type, src1, ins.imm);
            } else {
                assert(false);
            }
            regf.write_reg(ins.rd, temp);
            break;

        // save & load operation
        case InstrPlace::LSB:
            switch (ins.header.type) {
                case InstrType::LB:
                    temp = dmem.read_byte(ins.imm + src1, false);
                    regf.write_reg(ins.rd, temp);
                    break;
                case InstrType::LBU:
                    temp = dmem.read_byte(ins.imm + src1, true);
                    regf.write_reg(ins.rd, temp);
                    break;
                case InstrType::LH:
                    temp = dmem.read_half(ins.imm + src1, false);
                    regf.write_reg(ins.rd, temp);
                    break;
                case InstrType::LHU:
                    temp = dmem.read_half(ins.imm + src1, true);
                    regf.write_reg(ins.rd, temp);
                    break;
                case InstrType::LW:
                    temp = dmem.read_word(ins.imm + src1);
                    regf.write_reg(ins.rd, temp);
                    break;
                case InstrType::SB:
                    dmem.write_byte(ins.imm + src1, src2);
                    break;
                case InstrType::SH:
                    dmem.write_half(ins.imm + src1, src2);
                    break;
                case InstrType::SW:
                    dmem.write_word(ins.imm + src1, src2);
                    break;
                default:
                    assert(false);
            }
            break;
        
        // register operation
        case InstrPlace::REG:
            switch (ins.header.type) {
                case InstrType::AUIPC:
                    regf.write_reg(ins.rd, pc + uint32_t(ins.imm));
                    break;
                case InstrType::LUI:
                    regf.write_reg(ins.rd, uint32_t(ins.imm));
                    break;
                default:
                    assert(false);
            }
            break;

        // control operation
        case InstrPlace::BRANCH:
            switch (ins.header.type) {
                case InstrType::BEQ:
                    if (src1 == src2) {
                        pc += ins.imm;
                        pcif = false;
                    }
                    break;
                case InstrType::BGE:
                    if (int32_t(src1) >= int32_t(src2)) {
                        pc += ins.imm;
                        pcif = false;
                    }
                    break;
                case InstrType::BGEU:
                    if (src1 >= src2) {
                        pc += ins.imm;
                        pcif = false;
                    }
                    break;
                case InstrType::BLT:
                    if (int32_t(src1) < int32_t(src2)) {
                        pc += ins.imm;
                        pcif = false;
                    }
                    break;
                case InstrType::BLTU:
                    if (src1 < src2) {
                        pc += ins.imm;
                        pcif = false;
                    }
                    break;
                case InstrType::BNE:
                    if (src1 != src2) {
                        pc += ins.imm;
                        pcif = false;
                    }
                    break;
                case InstrType::JAL:
                    regf.write_reg(ins.rd, pc + 4);
                    pc += ins.imm;
                    pcif = false;
                    break;
                case InstrType::JALR:
                    regf.write_reg(ins.rd, pc + 4);
                    pc = (src1 + ins.imm) & ~1u;
                    pcif = false;
                    break;
                default:
                    assert(false);
            }
            break;
        
        // no operation
        case InstrPlace::NOP:
            break;
        
        default:
            assert(false);
    }

    // dump the state
    tf.dump(pc);

    // increase pc
    if (pcif) {
        pc += 4;
    }

    // set zero register to 0
    regf.write_reg(0, 0);
}

void CPUInterpreter::run(int max_step = 10000) {
    for (int i = 0; i < max_step; i++) {
        step();
        if (halted) {
            return;
        }
    }
}