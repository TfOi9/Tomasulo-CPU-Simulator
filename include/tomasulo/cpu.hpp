#pragma once

#include "../decoder/instr.hpp"
#include "../mem/instrmem.hpp"
#include "datamem.hpp"
#include "regfile.hpp"
#include "regaliastab.hpp"
#include "reorderbuf.hpp"
#include "alurs.hpp"
#include "lsrs.hpp"
#include "storebuf.hpp"
#include "cdb.hpp"
#include "branchpred.hpp"
#include "tracefile.hpp"

#include <cstdint>

// RISC-V Tomasulo CPU Simulator
class TomasuloCPU {
    InstrMemory imem;
    SimDataMemory dmem;

    ArchRegFile regf;

    RegAliasTab rat;
    ReorderBuf rob;
    ALURS alurs;
    LSRS lsrs;
    StoreBuffer sb;

    CDBEntry cdb;
    CDBEntry next_cdb;

    NaivePredictor bp;

    TraceFile tf;

    uint32_t pc;
    uint32_t next_pc;

    Instr fetched_instr;
    bool fetch_valid;
    bool halted;

    uint32_t cycle_count;
    uint32_t total_branches;
    uint32_t mispredicted_branches;

public:
    // load a program .data from path
    void load_program(const std::string& path);
    // run a cycle
    void cycle();
    // run until halt or reached limits
    void run(int max_cycles);

};