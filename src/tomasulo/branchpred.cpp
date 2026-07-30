#include "../../include/tomasulo/branchpred.hpp"

BranchPredictor::~BranchPredictor() {}

std::pair<bool, uint32_t> NaivePredictor::predict(uint32_t pc, int32_t imm) {
    return std::make_pair(false, pc + 4);
}

void NaivePredictor::update(uint32_t pc, bool actual_taken) {}

BimodalPredictor::BimodalPredictor() {
    counters.fill(1);
}

std::pair<bool, uint32_t> BimodalPredictor::predict(uint32_t pc, int32_t imm) {
    bool taken = counters[(pc >> 2) % TABLE_SIZE] >= 2;
    return {taken, taken ? pc + uint32_t(imm) : pc + 4};
}

void BimodalPredictor::update(uint32_t pc, bool actual_taken) {
    uint8_t& counter = counters[(pc >> 2) % TABLE_SIZE];
    if (actual_taken) {
        if (counter < 3) ++counter;
    } else if (counter > 0) {
        --counter;
    }
}
