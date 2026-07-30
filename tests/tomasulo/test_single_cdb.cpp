#include "../../include/tomasulo/alurs.hpp"
#include "../../include/tomasulo/cdb.hpp"
#include "../../include/tomasulo/lsrs.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        failures++;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Instr make_instr(InstrType type, InstrClass clas, InstrPlace place,
        int32_t imm = 0) {
    return {{0, 0, 0, type, clas, place}, 0, 0, 0, 0, imm};
}

int add_alu(ALURS& rs, const Instr& instr, const RATEntry& rj,
        const RATEntry& rk, uint32_t tag, uint32_t pc = 0) {
    rs.compute_next();
    int slot = rs.alloc_resolved(instr, rj, rk, tag, pc);
    rs.update();
    return slot;
}

int add_ls(LSRS& rs, const Instr& instr, const RATEntry& rj,
        const RATEntry& rk, uint32_t tag) {
    rs.compute_next();
    int slot = rs.alloc_resolved(instr, rj, rk, tag, 0);
    rs.update();
    return slot;
}

void wake(ALURS& rs, uint32_t tag, uint32_t value) {
    rs.compute_next();
    rs.listen_cdb({true, tag, value, false, false, 0});
    rs.update();
}

void wake(LSRS& rs, uint32_t tag, uint32_t value) {
    rs.compute_next();
    rs.listen_cdb({true, tag, value, false, false, 0});
    rs.update();
}

void execute(ALURS& rs) {
    rs.compute_next();
    rs.execute();
    rs.update();
}

uint32_t execute(LSRS& rs, StoreBuffer& sb, SimDataMemory& dmem) {
    rs.compute_next();
    dmem.compute_next();
    dmem.decrease_left_cycles();
    rs.execute(sb, dmem);
    uint32_t accepted = dmem.resolve_requests();
    if (accepted != 0) rs.mark_mem_requested(accepted);
    rs.update();
    dmem.update();
    return accepted;
}

void free_entry(ALURS& rs, uint32_t tag) {
    rs.compute_next();
    rs.free_entry_by_tag(tag);
    rs.update();
}

void free_entry(LSRS& rs, uint32_t tag) {
    rs.compute_next();
    rs.free_entry_by_tag(tag);
    rs.update();
}

void prepare_store(StoreBuffer& sb, uint32_t tag, uint32_t addr,
        uint32_t value) {
    sb.compute_next();
    check(sb.insert_next(tag, InstrType::SW) >= 0, "store buffer allocates");
    sb.update();
    sb.compute_next();
    sb.set_addr_ready_next(tag, addr);
    sb.set_val_ready_next(tag, value);
    sb.update();
}

const CDBEntry& older(const CDBEntry& first, const CDBEntry& second) {
    if (!first.valid) return second;
    if (!second.valid) return first;
    return first.rob_tag < second.rob_tag ? first : second;
}

void test_cdb() {
    CDB cdb;
    check(cdb.empty(), "CDB starts empty");

    CDBEntry branch{true, 7, 11, true, true, 44};
    cdb.publish(branch);
    check(!cdb.empty(), "publish occupies the CDB");
    check(cdb.inspect().rob_tag == 7 && cdb.inspect().val == 11,
        "inspect returns the published value");
    check(cdb.inspect().is_branch && cdb.inspect().branch_actual_taken &&
        cdb.inspect().branch_target == 44,
        "inspect preserves branch metadata");

    cdb.invalidate_squashed(7);
    check(!cdb.empty(), "surviving entry is not invalidated");
    cdb.invalidate_squashed(6);
    check(cdb.empty(), "younger squashed entry is invalidated");

    cdb.publish({true, 3, 9, false, false, 0});
    cdb.clear();
    check(cdb.empty(), "clear empties the CDB");

#if defined(__unix__) || defined(__APPLE__)
    pid_t child = fork();
    check(child >= 0, "assertion child starts");
    if (child == 0) {
        std::freopen("/dev/null", "w", stderr);
        CDB occupied;
        occupied.publish({true, 1, 1, false, false, 0});
        occupied.publish({true, 2, 2, false, false, 0});
        std::_Exit(0);
    }
    if (child > 0) {
        int status = 0;
        waitpid(child, &status, 0);
        check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT,
            "publishing over an occupied CDB asserts");
    }
#endif
}

void test_alu_single_completion() {
    ALURS rs;
    Instr add = make_instr(
        InstrType::ADD, InstrClass::R, InstrPlace::ALU);
    RATEntry waiting{false, 0, 5};

    check(add_alu(rs, add, waiting, waiting, 10) >= 0, "ALU tag 10 allocates");
    check(add_alu(rs, add, waiting, waiting, 20) >= 0, "ALU tag 20 allocates");
    check(add_alu(rs, add, waiting, waiting, 30) >= 0, "ALU tag 30 allocates");
    wake(rs, 5, 4);

    execute(rs);
    check(rs.writeback_candidate().rob_tag == 10,
        "ALU completes the oldest ready tag first");
    free_entry(rs, 10);
    check(!rs.writeback_candidate().valid,
        "ALU creates only one new completion per cycle");

    execute(rs);
    check(rs.writeback_candidate().rob_tag == 20,
        "second ALU completion follows in the next cycle");
    free_entry(rs, 20);
    execute(rs);
    check(rs.writeback_candidate().rob_tag == 30,
        "ready ALU entries cannot starve");
}

void test_branch_metadata() {
    RATEntry one{true, 1, 0};
    RATEntry ready{true, 0, 0};

    ALURS conditional;
    Instr beq = make_instr(
        InstrType::BEQ, InstrClass::B, InstrPlace::BRANCH, 12);
    add_alu(conditional, beq, one, one, 1, 100);
    execute(conditional);
    CDBEntry result = conditional.writeback_candidate();
    check(result.valid && result.is_branch && result.branch_actual_taken &&
        result.branch_target == 112,
        "conditional branch metadata stays on the ALU candidate");

    ALURS jal_rs;
    Instr jal = make_instr(
        InstrType::JAL, InstrClass::J, InstrPlace::BRANCH, 20);
    add_alu(jal_rs, jal, ready, ready, 2, 200);
    execute(jal_rs);
    result = jal_rs.writeback_candidate();
    check(result.val == 204 && result.branch_actual_taken &&
        result.branch_target == 220,
        "JAL value and target stay on the ALU candidate");

    ALURS jalr_rs;
    Instr jalr = make_instr(
        InstrType::JALR, InstrClass::I, InstrPlace::BRANCH, 3);
    add_alu(jalr_rs, jalr, {true, 100, 0}, ready, 3, 300);
    execute(jalr_rs);
    result = jalr_rs.writeback_candidate();
    check(result.val == 304 && result.branch_actual_taken &&
        result.branch_target == 102,
        "JALR value and aligned target stay on the ALU candidate");
}

void test_forwarded_load_limit() {
    LSRS rs;
    StoreBuffer sb;
    SimDataMemory dmem;
    prepare_store(sb, 5, 128, 0x12345678);

    Instr load = make_instr(
        InstrType::LW, InstrClass::I, InstrPlace::LSB);
    RATEntry address{true, 128, 0};
    RATEntry ready{true, 0, 0};
    add_ls(rs, load, address, ready, 10);
    add_ls(rs, load, address, ready, 20);

    execute(rs, sb, dmem);
    CDBEntry result = rs.writeback_candidate();
    check(result.valid && result.rob_tag == 10 &&
        result.val == 0x12345678,
        "oldest eligible forwarded load completes first");
    free_entry(rs, 10);
    check(!rs.writeback_candidate().valid,
        "only one forwarded load completes per cycle");

    execute(rs, sb, dmem);
    check(rs.writeback_candidate().rob_tag == 20,
        "second forwarded load completes later");
}

void test_memory_response_priority() {
    LSRS rs;
    StoreBuffer sb;
    SimDataMemory dmem;
    dmem.load_hex_data("@00000100\n78 56 34 12\n");
    prepare_store(sb, 15, 512, 0xaabbccdd);

    Instr load = make_instr(
        InstrType::LW, InstrClass::I, InstrPlace::LSB);
    Instr forwarded_load = make_instr(
        InstrType::LW, InstrClass::I, InstrPlace::LSB, 512);
    Instr store = make_instr(
        InstrType::SW, InstrClass::S, InstrPlace::LSB, 768);
    RATEntry ready{true, 0, 0};
    add_ls(rs, load, {true, 256, 0}, ready, 10);
    add_ls(rs, forwarded_load, {false, 0, 99}, ready, 20);
    int store_slot = add_ls(
        rs, store, {false, 0, 99}, {false, 0, 99}, 30);

    check(execute(rs, sb, dmem) == 10,
        "oldest memory load owns the request");
    execute(rs, sb, dmem);
    execute(rs, sb, dmem);
    execute(rs, sb, dmem);

    rs.compute_next();
    dmem.compute_next();
    dmem.decrease_left_cycles();
    rs.execute(sb, dmem);
    rs.listen_cdb({true, 99, 0, false, false, 0});
    dmem.resolve_requests();
    rs.update();
    dmem.update();

    execute(rs, sb, dmem);
    CDBEntry result = rs.writeback_candidate();
    check(result.valid && result.rob_tag == 10 &&
        result.val == 0x12345678,
        "memory response completes before an eligible forwarded load");
    check(rs.get_entry(store_slot).done,
        "store completion remains independent of the CDB");

    free_entry(rs, 10);
    check(!rs.writeback_candidate().valid,
        "response priority blocks a forwarded completion that cycle");
    execute(rs, sb, dmem);
    result = rs.writeback_candidate();
    check(result.valid && result.rob_tag == 20 &&
        result.val == 0xaabbccdd,
        "forwarded load completes after the response");
}

void test_repeated_contention() {
    ALURS alu;
    LSRS loads;
    StoreBuffer sb;
    SimDataMemory dmem;
    prepare_store(sb, 5, 64, 7);

    Instr add = make_instr(
        InstrType::ADD, InstrClass::R, InstrPlace::ALU);
    Instr load = make_instr(
        InstrType::LW, InstrClass::I, InstrPlace::LSB);
    RATEntry alu_waiting{false, 0, 6};
    RATEntry load_waiting{false, 0, 7};
    RATEntry ready{true, 0, 0};

    add_alu(alu, add, alu_waiting, alu_waiting, 10);
    add_alu(alu, add, alu_waiting, alu_waiting, 30);
    add_ls(loads, load, load_waiting, ready, 20);
    add_ls(loads, load, load_waiting, ready, 40);
    wake(alu, 6, 1);
    wake(loads, 7, 64);
    execute(alu);
    execute(loads, sb, dmem);

    CDB cdb;
    uint32_t expected[] = {10, 20, 30, 40};
    for (uint32_t tag : expected) {
        CDBEntry alu_candidate = alu.writeback_candidate();
        CDBEntry load_candidate = loads.writeback_candidate();
        CDBEntry winner = older(alu_candidate, load_candidate);
        check(winner.valid && winner.rob_tag == tag,
            "global contention broadcasts monotonically oldest tags");

        cdb.clear();
        cdb.publish(winner);
        if (winner.rob_tag == alu_candidate.rob_tag) {
            free_entry(alu, winner.rob_tag);
        } else {
            free_entry(loads, winner.rob_tag);
        }

        if (tag == 10) {
            execute(alu);
            execute(loads, sb, dmem);
        }
    }
    check(cdb.inspect().rob_tag == 40,
        "the final losing candidate eventually broadcasts");
}
}

int main() {
    test_cdb();
    test_alu_single_completion();
    test_branch_metadata();
    test_forwarded_load_limit();
    test_memory_response_priority();
    test_repeated_contention();

    if (failures != 0) {
        std::cerr << failures << " single-CDB checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "single-CDB checks passed\n";
    return EXIT_SUCCESS;
}
