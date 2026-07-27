#pragma once

#include "../mem/instrmem.hpp"
#include "../mem/datamem.hpp"
#include "regfile.hpp"
#include "tracefile.hpp"

#include <cstdint>

// RISC-V CPU Interpreter
class CPUInterpreter {
    // instruction memory
    InstrMemory imem;
    // data memory
    DataMemory dmem;
    // register file
    RegFile regf;
    // trace file
    TraceFile tf;
    // program counter
    uint32_t pc;
    // halt flag
    bool halted;
    // trace flag
    bool trace;

public:
    CPUInterpreter(bool trace = true) : tf(regf), pc(0), halted(false), trace(trace) {}
    // load program file from path
    void load_program(const std::string& path);
    // run one step
    void step();
    // run until halt or reach max_step
    void run(int max_step);

};