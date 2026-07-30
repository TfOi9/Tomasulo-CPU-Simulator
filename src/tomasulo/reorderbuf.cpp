#include "../../include/tomasulo/reorderbuf.hpp"

#include <cassert>

int ReorderBuf::alloc(InstrType type, uint8_t dest_reg,
        uint32_t pc, bool is_branch, bool branch_pred_taken,
        uint32_t branch_target, bool is_store,
        uint32_t pred_target, uint32_t branch_pred_context) {
    if (is_full()) {
        return -1;
    }

    uint32_t tag = next_tag++;

    ROBEntry entry = {
        true,
        type,
        dest_reg,
        0,
        false,
        pc,
        is_branch,
        branch_pred_taken,
        false,
        branch_target,
        pred_target,
        branch_pred_context,
        is_store,
        0,
        0,
        tag
    };

    next_rob[tail_next] = entry;
    tail_next = (tail_next + 1) % ROB_SIZE;
    return tag;
}

bool ReorderBuf::is_full() const {
    return head == tail && rob[head].busy == true;
}

bool ReorderBuf::is_empty() const {
    return head == tail && !rob[head].busy;
}

bool ReorderBuf::get_result_if_ready(uint32_t tag, uint32_t& val) const {
    for (const ROBEntry& entry : rob) {
        if (entry.busy && entry.tag == tag && entry.ready) {
            val = entry.val;
            return true;
        }
    }
    return false;
}

bool ReorderBuf::contains_tag(uint32_t tag) const {
    for (const ROBEntry& entry : rob) {
        if (entry.busy && entry.tag == tag) return true;
    }
    return false;
}

bool ReorderBuf::is_next_full() const {
    return head_next == tail_next
        && next_rob[head_next].busy == true;
}

void ReorderBuf::write_result(uint32_t tag, uint32_t val) {
    size_t index = ROB_SIZE;
    for (size_t i = 0; i < ROB_SIZE; i++) {
        if (rob[i].busy && rob[i].tag == tag) {
            index = i;
            break;
        }
    }

    if (index == ROB_SIZE) return;
    if (!next_rob[index].busy || next_rob[index].tag != tag) return;

    next_rob[index].ready = true;
    next_rob[index].val = val;
}

void ReorderBuf::write_branch_result(uint32_t tag, bool taken,
        uint32_t target) {
    size_t index = ROB_SIZE;
    for (size_t i = 0; i < ROB_SIZE; i++) {
        if (rob[i].busy && rob[i].tag == tag) {
            index = i;
            break;
        }
    }

    if (index == ROB_SIZE || !rob[index].is_branch) return;
    if (!next_rob[index].busy || next_rob[index].tag != tag) return;

    next_rob[index].ready = true;
    next_rob[index].branch_actual_taken = taken;
    next_rob[index].branch_target = target;
}

void ReorderBuf::write_store_result(uint32_t tag, uint32_t addr,
        uint32_t val) {
    size_t index = ROB_SIZE;
    for (size_t i = 0; i < ROB_SIZE; i++) {
        if (rob[i].busy && rob[i].tag == tag) {
            index = i;
            break;
        }
    }

    if (index == ROB_SIZE || !rob[index].is_store) return;
    if (!next_rob[index].busy || next_rob[index].tag != tag) return;

    next_rob[index].ready = true;
    next_rob[index].store_addr = addr;
    next_rob[index].store_value = val;
}

bool ReorderBuf::can_commit() const {
    return rob[head_next].busy && rob[head_next].ready;
}

const ROBEntry& ReorderBuf::head_entry() const {
    assert(can_commit());
    return rob[head_next];
}

bool ReorderBuf::commit() {
    assert(can_commit());
    const ROBEntry& entry = rob[head_next];
    bool mispredict = false;

    // handle branch mispredict
    if (entry.is_branch) {
        bool direction_mismatch =
            entry.branch_actual_taken != entry.branch_pred_taken;
        bool target_mismatch = entry.branch_actual_taken &&
            entry.branch_pred_taken && entry.branch_target != entry.pred_target;
        if (direction_mismatch || target_mismatch) {
            mispredict = true;
        }
    }

    // Schedule removal at the clock edge; current state remains immutable.
    next_rob[head_next].busy = false;
    head_next = (head_next + 1) % ROB_SIZE;
    return mispredict;
}

void ReorderBuf::flush_from_next(uint32_t branch_tag) {
    size_t index = ROB_SIZE;
    for (size_t i = 0; i < ROB_SIZE; i++) {
        if (rob[i].busy && rob[i].tag == branch_tag) {
            index = i;
            break;
        }
    }

    assert(index != ROB_SIZE);
    assert(rob[index].is_branch == true);

    for (ROBEntry& entry : next_rob) {
        if (entry.busy && entry.tag > branch_tag) {
            entry.busy = false;
        }
    }
    tail_next = (index + 1) % ROB_SIZE;
}

void ReorderBuf::compute_next() {
    for (int i = 0; i < ROB_SIZE; i++) {
        next_rob[i] = rob[i];
    }
    head_next = head;
    tail_next = tail;
}

void ReorderBuf::update() {
    std::swap(rob, next_rob);
    std::swap(head, head_next);
    std::swap(tail, tail_next);
}
