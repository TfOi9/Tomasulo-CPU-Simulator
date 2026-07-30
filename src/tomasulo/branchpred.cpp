#include "../../include/tomasulo/branchpred.hpp"

#include <utility>
#include <cstddef>

BranchPredictor::~BranchPredictor() {}

BranchPrediction NaivePredictor::predict(uint32_t pc, int32_t imm) {
    return {
        false,
        pc + 4,
        0
    };
}

void NaivePredictor::update(uint32_t pc, uint32_t context, bool actual_taken) {}

void NaivePredictor::recover() {}

void NaivePredictor::compute_next() {}

void NaivePredictor::clock() {}

BimodalPredictor::BimodalPredictor() {
    counters.fill(1);
    next_counters = counters;
}

BranchPrediction BimodalPredictor::predict(uint32_t pc, int32_t imm) {
    bool taken = counters[(pc >> 2) % TABLE_SIZE] >= 2;
    return {
        taken,
        taken ? pc + uint32_t(imm) : pc + 4,
        0
    };
}

void BimodalPredictor::update(uint32_t pc, uint32_t context, bool actual_taken) {
    uint8_t& counter = next_counters[(pc >> 2) % TABLE_SIZE];
    if (actual_taken) {
        if (counter < 3) ++counter;
    } else if (counter > 0) {
        --counter;
    }
}

void BimodalPredictor::recover() {}

void BimodalPredictor::compute_next() {
    next_counters = counters;
}

void BimodalPredictor::clock() {
    std::swap(counters, next_counters);
}

GsharePredictor::GsharePredictor() {
    committed_ghr = 0;
    next_committed_ghr = committed_ghr;
    speculative_ghr = 0;
    next_speculative_ghr = speculative_ghr;
    pht.fill(1);
    next_pht = pht;
}

BranchPrediction GsharePredictor::lookup(uint32_t pc, int32_t imm) const {
    uint32_t index = ((pc >> 2) & ((1 << GLEN) - 1) ^ next_speculative_ghr) & ((1 << GLEN) - 1);
    bool taken = pht[index] >= 2;
    return {
        taken,
        taken ? pc + uint32_t(imm) : pc + 4,
        index
    };
}

void GsharePredictor::speculate(bool taken) {
    next_speculative_ghr =
        ((next_speculative_ghr << 1) | taken) & ((1 << GLEN) - 1);
}

BranchPrediction GsharePredictor::predict(uint32_t pc, int32_t imm) {
    BranchPrediction prediction = lookup(pc, imm);
    speculate(prediction.taken);
    return prediction;
}

void GsharePredictor::update(uint32_t pc, uint32_t context, bool actual_taken) {
    uint8_t& counter = next_pht[context];
    if (actual_taken) {
        if (counter < 3) ++counter;
    } else if (counter > 0) {
        --counter;
    }
    next_committed_ghr = ((next_committed_ghr << 1) | actual_taken) & ((1 << GLEN) - 1);
}

void GsharePredictor::recover() {
    next_speculative_ghr = next_committed_ghr;
}

void GsharePredictor::compute_next() {
    next_committed_ghr = committed_ghr;
    next_speculative_ghr = speculative_ghr;
    next_pht = pht;
}

void GsharePredictor::clock() {
    std::swap(committed_ghr, next_committed_ghr);
    std::swap(speculative_ghr, next_speculative_ghr);
    std::swap(pht, next_pht);
}

TournamentPredictor::TournamentPredictor() {
    counters.fill(1);
    next_counters = counters;
}

BranchPrediction TournamentPredictor::predict(uint32_t pc, int32_t imm) {
    BranchPrediction gpred = gp.lookup(pc, imm);
    BranchPrediction bpred = bp.predict(pc, imm);
    bool use_bp = counters[(pc >> 2) % TABLE_SIZE] >= 2;
    const BranchPrediction& selected = use_bp ? bpred : gpred;
    gp.speculate(selected.taken);

    uint32_t context = gpred.context & GSHARE_CONTEXT_MASK;
    if (gpred.taken) context |= GSHARE_PRED_BIT;
    if (bpred.taken) context |= BIMODAL_PRED_BIT;
    return {selected.taken, selected.target, context};
}

void TournamentPredictor::update(uint32_t pc, uint32_t context, bool actual_taken) {
    bool gpred = (context & GSHARE_PRED_BIT) != 0;
    bool bpred = (context & BIMODAL_PRED_BIT) != 0;
    uint8_t& counter = next_counters[(pc >> 2) % TABLE_SIZE];
    if (gpred != actual_taken && bpred == actual_taken) {
        if (counter < 3) counter++;
    } else if (gpred == actual_taken && bpred != actual_taken) {
        if (counter > 0) counter--;
    }

    gp.update(pc, context & GSHARE_CONTEXT_MASK, actual_taken);
    bp.update(pc, 0, actual_taken);
}

void TournamentPredictor::recover() {
    gp.recover();
    bp.recover();
}

void TournamentPredictor::compute_next() {
    next_counters = counters;
    gp.compute_next();
    bp.compute_next();
}

void TournamentPredictor::clock() {
    std::swap(counters, next_counters);
    gp.clock();
    bp.clock();
}
