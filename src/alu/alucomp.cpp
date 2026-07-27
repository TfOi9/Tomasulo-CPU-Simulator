#include "../../include/alu/alucomp.hpp"

#include <cassert>

uint32_t alu_comp(InstrType type, uint32_t src1, uint32_t src2) {
    uint32_t ans;
    switch (type) {
        case InstrType::ADD:
            ans = src1 + src2; break;
        case InstrType::SUB:
            ans = src1 - src2; break;
        case InstrType::AND:
            ans = src1 & src2; break;
        case InstrType::OR:
            ans = src1 | src2; break;
        case InstrType::XOR:
            ans = src1 ^ src2; break;
        case InstrType::SLL:
            ans = src1 << src2; break;
        case InstrType::SRL:
            ans = src1 >> src2; break;
        case InstrType::SRA:
            ans = int32_t(src1) >> src2; break;
        case InstrType::SLT:
            ans = (int32_t(src1) < int32_t(src2)) ? 1 : 0; break;
        case InstrType::SLTU:
            ans = (src1 < src2) ? 1 : 0; break;
        default:
            assert(false);
    }
    return ans;
}

uint32_t alu_compi(InstrType type, uint32_t src, int32_t imm) {
    uint32_t ans;
    switch (type) {
        case InstrType::ADDI:
            ans = src + uint32_t(imm); break;
        case InstrType::ANDI:
            ans = src & uint32_t(imm); break;
        case InstrType::ORI:
            ans = src | uint32_t(imm); break;
        case InstrType::XORI:
            ans = src ^ uint32_t(imm); break;
        case InstrType::SLLI:
            ans = src << uint32_t(imm); break;
        case InstrType::SRLI:
            ans = src >> uint32_t(imm); break;
        case InstrType::SRAI:
            ans = int32_t(src) >> uint32_t(imm); break;
        case InstrType::SLTI:
            ans = (int32_t(src) < imm) ? 1 : 0; break;
        case InstrType::SLTIU:
            ans = (src < uint32_t(imm)) ? 1 : 0; break;
        default:
            assert(false);
    }
    return ans;
}