#pragma once

#include "../decoder/instr.hpp"

#include <cstdint>

// alu computation of two registers
uint32_t alu_comp(InstrType type, uint32_t src1, uint32_t src2);

// alu computation of a register and an immediate
uint32_t alu_compi(InstrType type, uint32_t src, int32_t imm);