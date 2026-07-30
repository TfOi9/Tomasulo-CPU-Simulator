#include "../../include/tomasulo/cpu.hpp"
#include "../../include/tomasulo/branchpred.hpp"

#include <fstream>
#include <sstream>
#include <cassert>
#include <iostream>

TomasuloCPU::TomasuloCPU(bool trace)
    : cdb{false, 0, 0, false, false, 0},
      next_cdb{false, 0, 0, false, false, 0},
      bp(new BimodalPredictor()),
      tf(regf, trace),
      pc(0),
      next_pc(0),
      fetched_pc(0),
      next_fetched_pc(0),
      ras{},
      ras_top(0),
      ras_predicted_target(0),
      fetched_instr{},
      next_fetched_instr{},
      fetch_valid(false),
      next_fetch_valid(false),
      redirect(false),
      next_redirect(false),
      halted(false),
      next_halted(false),
      cycle_count(0),
      total_branches(0),
      mispredicted_branches(0),
      squash_pending(false),
      squash_tag(0),
      squash_pc(0) {}

TomasuloCPU::~TomasuloCPU() {
    delete bp;
}

void TomasuloCPU::load_program(const std::string &path) {
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
    next_pc = 0;
    fetched_pc = 0;
    next_fetched_pc = 0;
    cdb = {false, 0, 0, false, false, 0};
    next_cdb = {false, 0, 0, false, false, 0};
    fetch_valid = false;
    next_fetch_valid = false;
    redirect = false;
    next_redirect = false;
    halted = false;
    next_halted = false;
    cycle_count = 0;
    total_branches = 0;
    mispredicted_branches = 0;
    squash_pending = false;
    squash_tag = 0;
    squash_pc = 0;
}

void TomasuloCPU::init_next_states() {
    next_pc = pc;
    next_fetched_pc = fetched_pc;
    next_fetched_instr = fetched_instr;
    next_fetch_valid = fetch_valid;
    next_redirect = redirect;
    next_halted = halted;
    squash_pending = false;
    squash_tag = 0;
    squash_pc = 0;
    regf.prepare_next();
    rat.compute_next();
    rob.compute_next();
    alurs.compute_next();
    lsrs.compute_next();
    sb.compute_next();
    dmem.compute_next();
    bp->compute_next();
}

void TomasuloCPU::cdb_listen() {
    if (cdb.valid) {
        alurs.listen_cdb(cdb.rob_tag, cdb.val);
        lsrs.listen_cdb(cdb.rob_tag, cdb.val);
    }
    alurs.resolve_from_rob(rob);
    lsrs.resolve_from_rob(rob);
}

void TomasuloCPU::writeback() {
    CDBEntry alu_cdb = {false, 0, 0, false, false, 0};
    CDBEntry ls_cdb = {false, 0, 0, false, false, 0};

    // write back ALURS
    auto alu_cdbs = alurs.write_back();
    for (size_t i = 0; i < ALURS::ALURS_SIZE; i++) {
        const CDBEntry& entry = alu_cdbs[i];
        if (!entry.valid) continue;

        rob.write_result(entry.rob_tag, entry.val);
        if (entry.is_branch) {
            rob.write_branch_result(
                entry.rob_tag,
                entry.branch_actual_taken,
                entry.branch_target
            );
        }
        if (!alu_cdb.valid) {
            alu_cdb = entry;
        }
    }
    // write back LSRS
    auto ls_cdbs = lsrs.write_back();
    for (size_t i = 0; i < LSRS::LSRS_SIZE; i++) {
        const CDBEntry& entry = ls_cdbs[i];
        if (!entry.valid) continue;

        rob.write_result(entry.rob_tag, entry.val);
        if (!ls_cdb.valid) {
            ls_cdb = entry;
        }
    }

    // store LSRS entries
    for (size_t i = 0; i < LSRS::LSRS_SIZE; i++) {
        const LSRSEntry& e = lsrs.get_entry(i);
        if (!e.busy || !e.done || e.is_load) continue;

        rob.write_store_result(e.rob_tag, e.addr, e.vk);
    }
    // A single CDB can broadcast only one result per clock.  Keep every
    // unselected producer in its RS so it remains eligible next cycle;
    // dropping it here loses wakeups for already-issued dependants.
    if (ls_cdb.valid) {
        next_cdb = ls_cdb;
        lsrs.free_entry_by_tag(ls_cdb.rob_tag);
    } else if (alu_cdb.valid) {
        next_cdb = alu_cdb;
        alurs.free_entry_by_tag(alu_cdb.rob_tag);
    } else {
        next_cdb = {false, 0, 0, false, false, 0};
    }

    // Stores never use the CDB: their ROB entry contains address and data.
    for (size_t i = 0; i < LSRS::LSRS_SIZE; i++) {
        const LSRSEntry& e = lsrs.get_entry(i);
        if (e.busy && e.done && !e.is_load) {
            lsrs.free_entry_by_tag(e.rob_tag);
        }
    }
}

void TomasuloCPU::execute() {
    dmem.decrease_left_cycles();
    alurs.execute();
    lsrs.execute(sb, dmem);
}

void TomasuloCPU::commit() {
    while (rob.can_commit()) {
        const ROBEntry& entry = rob.head_entry();

        bool is_store = entry.is_store;
        bool is_branch = entry.is_branch;
        uint32_t tag = entry.tag;
        uint8_t dest_reg = entry.dest_reg;
        uint32_t val = entry.val;
        uint32_t entry_pc = entry.pc;
        bool actual_taken = entry.branch_actual_taken;
        uint32_t br_target = entry.branch_target;
        bool pred_taken = entry.branch_pred_taken;

        // write data memory
        if (is_store) {
            if (!dmem.issue_write_next(
                entry.store_addr,
                entry.store_value,
                entry.type
            )) break;
        }

        // write arch register
        if (!is_store && dest_reg != 0) {
            regf.write_reg(dest_reg, val);
            rat.commit_clear_next(dest_reg, tag, val);
        }

        // update branch predictor
        if (is_branch) {
            total_branches++;
            bp->update(entry_pc, actual_taken);
        }

        // ROB commit
        bool mis = rob.commit();

        if (is_store) {
            sb.commit_next(tag);
        }

        tf.dump(entry_pc);

        if (mis) {
            mispredicted_branches++;
            squash_pending = true;
            squash_tag = tag;
            squash_pc = actual_taken ? br_target : (entry_pc + 4);
            break;
        }

        // ... tf here (optional)
    }
}

void TomasuloCPU::issue() {
    if (!fetch_valid || halted) return;

    const Instr& ins = fetched_instr;

    if (ins.header.type == InstrType::HALT) {
        next_halted = true;
        next_fetch_valid = false;
        return;
    }

    // A decoded RV32I instruction always carries 5-bit register indices.
    // Treat malformed speculative instruction bytes as a frontend bubble
    // instead of allowing them to corrupt the RAT/ROB state.
    if (ins.rd >= 32 || ins.rs1 >= 32 || ins.rs2 >= 32) {
        next_fetch_valid = false;
        return;
    }

    if (ins.header.type == InstrType::NOP) {
        next_fetch_valid = false;
        return;
    }
    if (ins.header.type == InstrType::UNKNOWN) {
        next_fetch_valid = false;
        return;
    }

    InstrPlace place = ins.header.place;
    InstrType type = ins.header.type;

    bool needs_alu = (place == InstrPlace::ALU ||
                      place == InstrPlace::BRANCH ||
                      place == InstrPlace::REG);
    bool needs_ls = (place == InstrPlace::LSB);
    bool is_load = false;
    bool is_store = false;

    if (needs_ls) {
        is_store = (ins.header.clas == InstrClass::S);
        is_load = !is_store;
    }

    if (rob.is_full()) return;
    if (needs_alu && alurs.is_full()) return;
    if (needs_ls && lsrs.is_full()) return;
    if (is_store && sb.is_full()) return;

    bool is_branch = (ins.header.place == InstrPlace::BRANCH);
    bool pred_taken = false;
    uint32_t pred_target = fetched_pc + 4;
    if (is_branch) {
        if (type == InstrType::JAL) {
            pred_taken = true;
            pred_target = fetched_pc + ins.imm;
        } else if (type == InstrType::JALR) {
            pred_taken = true;
            pred_target = ras_predicted_target;
        } else {
            auto [taken, target] = bp->predict(fetched_pc, ins.imm);
            pred_taken = taken;
            pred_target = target;
        }
    }

    uint8_t dest_reg = ins.rd;
    int rob_tag = rob.alloc(
        type,
        dest_reg,
        fetched_pc,
        is_branch,
        pred_taken,
        is_branch ? (fetched_pc + ins.imm) : 0,
        is_store,
        is_branch ? pred_target : 0
    );

    if (rob_tag < 0) return;

    if (!is_store && dest_reg != 0) {
        rat.set_tag_next(dest_reg, rob_tag);
    }

    // The issue phase reads only current state.  A result already resident in
    // the current ROB can be forwarded even if its one-cycle CDB pulse passed.
    auto resolve_operand = [this](uint8_t reg) {
        RATEntry operand = rat.read(reg);
        uint32_t value = 0;
        if (!operand.ready && operand.tag != 0 &&
            rob.get_result_if_ready(operand.tag, value)) {
            operand = {true, value, 0};
        }
        return operand;
    };
    RATEntry rj = resolve_operand(ins.rs1);
    RATEntry rk = resolve_operand(ins.rs2);

    if (needs_alu) {
        alurs.alloc_resolved(ins, rj, rk, rob_tag, fetched_pc);
    } else if (needs_ls) {
        lsrs.alloc_resolved(ins, rj, rk, rob_tag, fetched_pc);
        if (is_store) {
            sb.insert_next(rob_tag, ins.header.type);
        }
    }

    next_fetch_valid = false;
}

void TomasuloCPU::fetch() {
    if (redirect) {
        next_redirect = false;
        return;
    }
    if (fetch_valid || halted) return;
    uint32_t raw = imem.read_instr(pc);
    next_fetched_instr = Instr::decode(raw);
    next_fetched_pc = pc;
    next_fetch_valid = true;

    InstrType type = next_fetched_instr.header.type;
    bool is_branch = (next_fetched_instr.header.place == InstrPlace::BRANCH);

    if (is_branch) {
        if (type == InstrType::JAL) {
            next_pc = pc + uint32_t(next_fetched_instr.imm);
        } else if (type == InstrType::JALR) {
            // A speculative RAS must itself be checkpointed and restored on
            // every branch flush.  Until that state is modeled, use the
            // deterministic fall-through prediction for indirect jumps;
            // execute supplies the authoritative target at commit.
            ras_predicted_target = pc + 4;
            next_pc = pc + 4;
        } else {
            auto [taken, target] = bp->predict(pc, next_fetched_instr.imm);
            next_pc = target;
        }
    } else {
        next_pc = pc + 4;
    }
}

void TomasuloCPU::finalize() {
    regf.flush_zero();
}

void TomasuloCPU::resolve_cycle_outputs() {
    if (!squash_pending) return;

    // A branch recovery has priority over every younger phase output.  This
    // merge runs after all phases, so it has the same result regardless of
    // whether issue/fetch happened before or after commit in the C++ source.
    rob.flush_from_next(squash_tag);
    alurs.flush_next();
    lsrs.flush_next();
    sb.flush_from_next(squash_tag);
    rat.restore_from_next(squash_tag, regf);
    if (next_cdb.valid && next_cdb.rob_tag > squash_tag) {
        next_cdb = {false, 0, 0, false, false, 0};
    }
    next_pc = squash_pc;
    next_fetch_valid = false;
    next_redirect = true;
    next_halted = false;
}

void TomasuloCPU::cycle() {
    // initialize
    init_next_states();

    // pipeline
    fetch();
    commit();
    execute();
    cdb_listen();
    writeback();
    issue();
    finalize();

    resolve_cycle_outputs();
    uint32_t accepted_load = dmem.resolve_requests(!squash_pending);
    if (accepted_load != 0) {
        lsrs.mark_mem_requested(accepted_load);
    }

    // update
    regf.update();
    rat.update();
    rob.update();
    alurs.update();
    lsrs.update();
    sb.update();
    dmem.update();
    bp->clock();
    pc = next_pc;
    fetched_pc = next_fetched_pc;
    fetched_instr = next_fetched_instr;
    fetch_valid = next_fetch_valid;
    redirect = next_redirect;
    halted = next_halted;
    cdb = next_cdb;
    cycle_count++;
}

void TomasuloCPU::run(int max_cycles) {
    for (int i = 0; i < max_cycles; i++) {
        cycle();
        if (halted && !fetch_valid && rob.is_empty()) {
            std::cerr << "HALTED at cycle=" << i << " a0=" << regf.read_reg(10) << std::endl;
            break;
        }
    }
    if (!halted) {
        std::cerr << "REACHING MAX STEPS halted=" << halted << " fetch_valid=" << fetch_valid << " rob_empty=" << rob.is_empty() << " pc=0x" << std::hex << pc << std::dec << std::endl;
        return;
    }
    uint8_t ret = uint8_t(regf.read_reg(10) & 0xFF);
    std::cout << int(ret) << std::endl;
}
