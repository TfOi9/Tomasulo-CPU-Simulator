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

    BranchPredictor *bp;

    TraceFile tf;

    uint32_t pc;
    uint32_t next_pc;
    uint32_t fetched_pc;

    Instr fetched_instr;
    bool fetch_valid;
    bool halted;

    uint32_t cycle_count;
    uint32_t total_branches;
    uint32_t mispredicted_branches;

    // initialize the next state
    void init_next_states();
    // listen to common data bus
    void cdb_listen();
    // write back
    void writeback();
    // execute
    void execute();
    // commit
    void commit();
    // issue
    void issue();
    // fetch
    void fetch();
    // finalize
    void finalize();

public:
    // constructor
    TomasuloCPU(bool trace = false);
    // destructor
    ~TomasuloCPU();
    // load a program .data from path
    void load_program(const std::string& path);
    // run a cycle
    void cycle();
    // run until halt or reached limits
    void run(int max_cycles = 1000000);

};