#pragma once

#include <string>
#include <cstdint>

// RISCV Extension type
enum class InstrExType {
    // base 32-bit integer instructions
    RV32I,
    // extention for integer multiplication and division
    M,
    // extension for single-precision floating-point
    F,
    // special extensions (eg. HALT)
    CUSTOM
};

// Class of instructions
enum class InstrClass {
    R, I, S, B, U, J, UNKNOWN
};

// Detailed instruction types
enum class InstrType {
    // arithmetic
    ADD, SUB, AND, OR, XOR,
    SLL, SRL, SRA, SLT, SLTU,
    ADDI, ANDI, ORI, XORI, SLLI,
    SRLI, SRAI, SLTI, SLTIU, 
    // memory
    LB, LBU, LH, LHU, LW,
    SB, SH, SW,
    // control
    BEQ, BGE, BGEU, BLT, BLTU,
    BNE, JAL, JALR,
    // other
    AUIPC, LUI, NOP, HALT,
    // unknown or unsupported (eg. ebreak, ecall)
    UNKNOWN
};

// Place to execute the instruction
enum class InstrPlace {
    ALU, LSB, REG, BRANCH, NOP,
};

// Header for a instruction
struct InstrHeader {
    uint8_t opcode;
    uint8_t funct7;
    uint8_t funct3;
    // instruction extension type
    InstrExType ex;
    // detailed instruction type
    InstrType type;
    // single-letter instruction class
    InstrClass clas;
    // instruction place
    InstrPlace place;
};

// Struct for a decoded instruction
struct Instr {
    // instruction header
    InstrHeader header;
    // raw instruction code for debugging
    uint32_t raw;
    // 5-bit register destination
    uint8_t rd;
    // 5-bit register sources
    uint8_t rs1, rs2;
    // immediate
    int32_t imm;

    // decode an instruction
    static Instr decode(uint32_t raw);
};

constexpr InstrClass instr_class_mapping[] = {
    // R-type arithmetic
    InstrClass::R, InstrClass::R, InstrClass::R, InstrClass::R, InstrClass::R,
    InstrClass::R, InstrClass::R, InstrClass::R, InstrClass::R, InstrClass::R,
    // I-type arithmetic
    InstrClass::I, InstrClass::I, InstrClass::I, InstrClass::I, InstrClass::I,
    InstrClass::I, InstrClass::I, InstrClass::I, InstrClass::I,
    // I-type load
    InstrClass::I, InstrClass::I, InstrClass::I, InstrClass::I, InstrClass::I,
    // S-type store
    InstrClass::S, InstrClass::S, InstrClass::S,
    // B-type branch
    InstrClass::B, InstrClass::B, InstrClass::B, InstrClass::B, InstrClass::B,
    InstrClass::B, InstrClass::J, InstrClass::I,
    // U-type, pseudo/custom, unknown
    InstrClass::U, InstrClass::U, InstrClass::I, InstrClass::UNKNOWN,
    InstrClass::UNKNOWN,
};

constexpr InstrPlace instr_place_mapping[] = {
    // R-type arithmetic → ALU
    InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU,
    InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU,
    // I-type arithmetic → ALU
    InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU,
    InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU, InstrPlace::ALU,
    // I-type load → LSB
    InstrPlace::LSB, InstrPlace::LSB, InstrPlace::LSB, InstrPlace::LSB, InstrPlace::LSB,
    // S-type store → LSB
    InstrPlace::LSB, InstrPlace::LSB, InstrPlace::LSB,
    // B-type branch → BRANCH
    InstrPlace::BRANCH, InstrPlace::BRANCH, InstrPlace::BRANCH, InstrPlace::BRANCH, InstrPlace::BRANCH,
    InstrPlace::BRANCH, InstrPlace::BRANCH, InstrPlace::BRANCH,
    // U-type → REG, pseudo/custom → NOP, unknown → NOP
    InstrPlace::REG, InstrPlace::REG, InstrPlace::NOP, InstrPlace::NOP,
    InstrPlace::NOP,
};

const std::string instr_name[] = {
    // R-type arithmetic
    "add",  "sub",  "and",  "or",   "xor",
    "sll",  "srl",  "sra",  "slt",  "sltu",
    // I-type arithmetic
    "addi", "andi", "ori",  "xori", "slli",
    "srli", "srai", "slti", "sltiu",
    // I-type load
    "lb",   "lbu",  "lh",   "lhu",  "lw",
    // S-type store
    "sb",   "sh",   "sw",
    // B-type branch
    "beq",  "bge",  "bgeu", "blt",  "bltu",
    "bne",  "jal",  "jalr",
    // U-type, pseudo/custom, unknown
    "auipc","lui",  "nop",  "halt",
    "unknown",
};