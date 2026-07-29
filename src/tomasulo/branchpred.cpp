#include "../../include/tomasulo/branchpred.hpp"

std::pair<bool, uint32_t> NaivePredictor::predict(uint32_t pc, int32_t imm) {
    return std::make_pair(false, pc + 4);
}

void NaivePredictor::update(uint32_t pc, bool actual_taken) {}