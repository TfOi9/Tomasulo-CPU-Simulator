#include <iostream>
#include "../../include/decoder/instr.hpp"

#include <cassert>

Instr Instr::decode(uint32_t raw) {
    if (raw == 0x0ff00513) {
        return {
            {0, 0, 0, InstrType::HALT, InstrClass::UNKNOWN, InstrPlace::NOP},
            raw,
            0, 0, 255
        };
    }

    uint8_t opcode = raw & 0x7F;
    uint8_t funct7 = raw >> 25;
    uint8_t funct3 = (raw >> 12) & 0x7;
    uint8_t rd = (raw >> 7) & 0x1F;
    uint8_t rs1 = (raw >> 15) & 0x1F;
    uint8_t rs2 = (raw >> 20) & 0x1F;
    int32_t imm = 0;
    InstrType type = InstrType::UNKNOWN;

    switch (opcode) {
        case 0b0110011: // R
            switch (funct3) {
                case 0b000:
                    if (funct7 == 0b0000000) {
                        // ADD
                        type = InstrType::ADD;
                    } else if (funct7 == 0b0100000) {
                        // SUB
                        type = InstrType::SUB;
                    } else {
                        assert(false);
                    }
                    break;
                case 0b111:
                    assert(funct7 == 0b0000000);
                    type = InstrType::AND;
                    break;
                case 0b110:
                    assert(funct7 == 0b0000000);
                    type = InstrType::OR;
                    break;
                case 0b100:
                    assert(funct7 == 0b0000000);
                    type = InstrType::XOR;
                    break;
                case 0b001:
                    assert(funct7 == 0b0000000);
                    type = InstrType::SLL;
                    break;
                case 0b101:
                    if (funct7 == 0b0000000) {
                        // SRL
                        type = InstrType::SRL;
                    } else if (funct7 == 0b0100000) {
                        // SRA
                        type = InstrType::SRA;
                    } else {
                        assert(false);
                    }
                    break;
                case 0b010:
                    assert(funct7 == 0b0000000);
                    type = InstrType::SLT;
                    break;
                case 0b011:
                    assert(funct7 == 0b0000000);
                    type = InstrType::SLTU;
                    break;
                default:
                    assert(false);
            }
            break;
        case 0b0010011: // arith-I
            imm = int32_t(raw) >> 20;
            rs2 = 0;
            switch (funct3) {
                case 0b000:
                    // ADDI
                    type = InstrType::ADDI;
                    break;
                case 0b111:
                    // ANDI
                    type = InstrType::ANDI;
                    break;
                case 0b110:
                    // ORI
                    type = InstrType::ORI;
                    break;
                case 0b100:
                    // XORI
                    type = InstrType::XORI;
                    break;
                case 0b001:
                    // SLLI
                    imm = int32_t((raw >> 20) & 0x1F);
                    assert(funct7 == 0b0000000);
                    type = InstrType::SLLI;
                    break;
                case 0b101:
                    imm = int32_t((raw >> 20) & 0x1F);
                    if (funct7 == 0b0000000) {
                        // SRLI
                        type = InstrType::SRLI;
                    } else if (funct7 == 0b0100000) {
                        // SRAI
                        type = InstrType::SRAI;
                    } else {
                        assert(false);
                    }
                    break;
                case 0b010:
                    // SLTI
                    type = InstrType::SLTI;
                    break;
                case 0b011:
                    // SLTIU
                    type = InstrType::SLTIU;
                    break;
                default:
                    assert(false);
            }
            if (type == InstrType::ADDI && rd == 0 && rs1 == 0 && imm == 0) {
                type = InstrType::NOP;
            }
            break;
        case 0b0000011: // mem-I
            imm = int32_t(raw) >> 20;
            rs2 = 0;
            switch (funct3) {
                case 0b000:
                    // LB
                    type = InstrType::LB;
                    break;
                case 0b100:
                    // LBU
                    type = InstrType::LBU;
                    break;
                case 0b001:
                    // LH
                    type = InstrType::LH;
                    break;
                case 0b101:
                    // LHU
                    type = InstrType::LHU;
                    break;
                case 0b010:
                    // LW
                    type = InstrType::LW;
                    break;
                default:
                    assert(false);
            }
            break;
        case 0b0100011: // S
            rd = 0;
            imm = int32_t(
                (((raw >> 25) & 0x7F) << 5)|
                ((raw >> 7) & 0x1F)
            );
            imm = (imm << 20) >> 20;
            switch (funct3) {
                case 0b000:
                    // SB
                    type = InstrType::SB;
                    break;
                case 0b001:
                    // SH
                    type = InstrType::SH;
                    break;
                case 0b010:
                    // SW
                    type = InstrType::SW;
                    break;
                default:
                    assert(false);
            }
            break;
        case 0b1100011: // B
            imm = int32_t(
                ((raw >> 31) << 12) |
                (((raw >> 7) & 0x1) << 11) |
                (((raw >> 25) & 0x3F) << 5) |
                (((raw >> 8) & 0xF) << 1)
            );
            imm = (imm << 19) >> 19;
            rd = 0;
            switch (funct3) {
                case 0b000:
                    // BEQ
                    type = InstrType::BEQ;
                    break;
                case 0b101:
                    // BGE
                    type = InstrType::BGE;
                    break;
                case 0b111:
                    // BGEU
                    type = InstrType::BGEU;
                    break;
                case 0b100:
                    // BLT
                    type = InstrType::BLT;
                    break;
                case 0b110:
                    // BLTU
                    type = InstrType::BLTU;
                    break;
                case 0b001:
                    // BNE
                    type = InstrType::BNE;
                    break;
                default:
                    assert(false);
            }
            break;
        case 0b1101111: // JAL
            imm = int32_t(
                (((raw >> 31) & 0x1) << 20) |
                (((raw >> 12) & 0xFF) << 12) |
                (((raw >> 20) & 0x1) << 11) |
                (((raw >> 21) & 0x3FF) << 1) 
            );
            imm = (imm << 11) >> 11;
            rs1 = rs2 = 0;
            type = InstrType::JAL;
            break;
        case 0b1100111: // JALR
            assert(funct3 == 0b000);
            imm = int32_t(raw) >> 20;
            rs2 = 0;
            type = InstrType::JALR;
            break;
        case 0b0010111: // AUIPC
            imm = int32_t(raw & 0xFFFFF000);
            rs1 = rs2 = 0;
            type = InstrType::AUIPC;
            break;
        case 0b0110111: // LUI
            imm = int32_t(raw & 0xFFFFF000);
            rs1 = rs2 = 0;
            type = InstrType::LUI;
            break;
        default:
            type = InstrType::UNKNOWN;
            break;
    }

    int type_int = static_cast<int>(type);

    return {
        {
            opcode,
            funct7,
            funct3,
            type,
            instr_class_mapping[type_int],
            instr_place_mapping[type_int]
        },
        raw,
        rd,
        rs1, rs2,
        imm
    };
}