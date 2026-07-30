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

    CDB cdb;

    BranchPredictor *bp;

    TraceFile tf;

    uint32_t pc;
    uint32_t next_pc;
    uint32_t fetched_pc;
    uint32_t next_fetched_pc;
    bool fetched_pred_taken;
    bool next_fetched_pred_taken;
    uint32_t fetched_pred_target;
    uint32_t next_fetched_pred_target;
    uint32_t fetched_pred_context;
    uint32_t next_fetched_pred_context;

    uint32_t ras[16];
    int ras_top;
    uint32_t ras_predicted_target;

    Instr fetched_instr;
    Instr next_fetched_instr;
    bool fetch_valid;
    bool next_fetch_valid;
    bool redirect;
    bool next_redirect;
    bool halted;
    bool next_halted;

    uint32_t cycle_count;
    uint32_t total_branches;
    uint32_t mispredicted_branches;

    bool squash_pending;
    uint32_t squash_tag;
    uint32_t squash_pc;

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
    // merge priority signals produced independently by the phases
    void resolve_cycle_outputs();

public:
    // constructor
    TomasuloCPU(bool trace = false);
    // destructor
    ~TomasuloCPU();
    // load a program .data from path
    void load_program(const std::string& path);
    // load a program .data from stdin
    void input_program();
    // run a cycle
    void cycle();
    // run until halt or reached limits
    void run(int max_cycles = 250000000);
    // report run data
    void report();

};
