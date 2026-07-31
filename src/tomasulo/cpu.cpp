#include "../../include/tomasulo/cpu.hpp"
#include "../../include/tomasulo/branchpred.hpp"

#include <fstream>
#include <sstream>
#include <cassert>
#include <iostream>

TomasuloCPU::TomasuloCPU(bool trace)
    : bp(new TAGEPredictor()),
      tf(regf, trace),
      pc(0),
      next_pc(0),
      fetched_pc(0),
      next_fetched_pc(0),
      fetched_pred_taken(false),
      next_fetched_pred_taken(false),
      fetched_pred_target(0),
      next_fetched_pred_target(0),
      fetched_pred_context(0),
      next_fetched_pred_context(0),
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
    fetched_pred_taken = false;
    next_fetched_pred_taken = false;
    fetched_pred_target = 0;
    next_fetched_pred_target = 0;
    fetched_pred_context = 0;
    next_fetched_pred_context = 0;
    cdb.clear();
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

void TomasuloCPU::input_program() {
    std::string str, p;
    while (std::getline(std::cin, str)) {
        p += str;
        p += '\n';
    }

    imem.load_hex_data(p);
    dmem.load_hex_data(p);

    pc = 0;
    next_pc = 0;
    fetched_pc = 0;
    next_fetched_pc = 0;
    fetched_pred_taken = false;
    next_fetched_pred_taken = false;
    fetched_pred_target = 0;
    next_fetched_pred_target = 0;
    fetched_pred_context = 0;
    next_fetched_pred_context = 0;
    cdb.clear();
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
    next_fetched_pred_taken = fetched_pred_taken;
    next_fetched_pred_target = fetched_pred_target;
    next_fetched_pred_context = fetched_pred_context;
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
    alurs.listen_cdb(cdb.inspect());
    lsrs.listen_cdb(cdb.inspect());
}

void TomasuloCPU::writeback() {
    cdb.clear();

    CDBEntry alu_candidate = alurs.writeback_candidate();
    CDBEntry load_candidate = lsrs.writeback_candidate();
    bool alu_wins = alu_candidate.valid &&
        (!load_candidate.valid ||
         alu_candidate.rob_tag < load_candidate.rob_tag);
    CDBEntry winner = alu_wins ? alu_candidate : load_candidate;

    for (size_t i = 0; i < LSRS::LSRS_SIZE; i++) {
        const LSRSEntry& e = lsrs.get_entry(i);
        if (!e.busy || !e.done || e.is_load) continue;

        rob.write_store_result(e.rob_tag, e.addr, e.vk);
    }

    if (winner.valid) {
        cdb.publish(winner);
        rob.write_result(winner.rob_tag, winner.val);
        if (winner.is_branch) {
            rob.write_branch_result(
                winner.rob_tag,
                winner.branch_actual_taken,
                winner.branch_target
            );
        }
        if (alu_wins) {
            alurs.free_entry_by_tag(winner.rob_tag);
        } else {
            lsrs.free_entry_by_tag(winner.rob_tag);
        }
    }

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
    if (!rob.can_commit()) {
        return;
    }

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
    uint32_t pred_context = entry.branch_pred_context;

    // write data memory
    if (is_store) {
        if (!dmem.issue_write_next(
            entry.store_addr,
            entry.store_value,
            entry.type
        )) return;
    }

    // write arch register
    if (!is_store && dest_reg != 0) {
        regf.write_reg(dest_reg, val);
        rat.commit_clear_next(dest_reg, tag, val);
    }

    // update branch predictor
    if (is_branch) {
        total_branches++;
        if (entry.type != InstrType::JAL &&
            entry.type != InstrType::JALR) {
            bp->update(entry_pc, pred_context, actual_taken);
        }
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
        return;
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
            pred_taken = fetched_pred_taken;
            pred_target = fetched_pred_target;
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
        is_branch ? pred_target : 0,
        is_branch ? fetched_pred_context : 0
    );

    if (rob_tag < 0) return;

    if (!is_store && dest_reg != 0) {
        rat.set_tag_next(dest_reg, rob_tag);
    }

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
    next_fetched_pred_taken = false;
    next_fetched_pred_target = pc + 4;
    next_fetched_pred_context = 0;

    InstrType type = next_fetched_instr.header.type;
    bool is_branch = (next_fetched_instr.header.place == InstrPlace::BRANCH);

    if (is_branch) {
        if (type == InstrType::JAL) {
            next_pc = pc + uint32_t(next_fetched_instr.imm);
        } else if (type == InstrType::JALR) {
            ras_predicted_target = pc + 4;
            next_pc = pc + 4;
        } else {
            BranchPrediction prediction =
                bp->predict(pc, next_fetched_instr.imm);
            next_fetched_pred_taken = prediction.taken;
            next_fetched_pred_target = prediction.target;
            next_fetched_pred_context = prediction.context;
            next_pc = prediction.target;
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

    rob.flush_from_next(squash_tag);
    alurs.flush_next();
    lsrs.flush_next();
    sb.flush_from_next(squash_tag);
    rat.restore_from_next(squash_tag, regf);
    bp->recover();
    cdb.invalidate_squashed(squash_tag);
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
    fetched_pred_taken = next_fetched_pred_taken;
    fetched_pred_target = next_fetched_pred_target;
    fetched_pred_context = next_fetched_pred_context;
    fetched_instr = next_fetched_instr;
    fetch_valid = next_fetch_valid;
    redirect = next_redirect;
    halted = next_halted;
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

void TomasuloCPU::report() {
    if (!halted) {
        std::cerr << "Tomasulo CPU reached simulation cycle limit.\n";
    } else {
        std::cerr << "Tomasulo CPU halted.\n";
    }

    std::cerr << "CPU cycles: " << cycle_count << std::endl;
    std::cerr << "Branch count: " << total_branches << std::endl;
    std::cerr << "Mispredicted branches: " << mispredicted_branches << std::endl;
    std::cerr << "Branch predict success rate: " << double(total_branches - mispredicted_branches) / double(total_branches) << std::endl;
}
