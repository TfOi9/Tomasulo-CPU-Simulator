#include "../../include/tomasulo/branchpred.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        failures++;
        std::cerr << "FAIL: " << message << '\n';
    }
}

BranchPrediction resolve(TournamentPredictor& predictor, uint32_t pc,
        bool actual_taken) {
    predictor.compute_next();
    BranchPrediction prediction = predictor.predict(pc, 8);
    predictor.update(pc, prediction.context, actual_taken);
    if (prediction.taken != actual_taken) {
        predictor.recover();
    }
    predictor.clock();
    return prediction;
}

void test_component_training_and_speculative_history() {
    TournamentPredictor predictor;

    BranchPrediction first = resolve(predictor, 0, true);
    BranchPrediction second = resolve(predictor, 0, true);
    BranchPrediction third = resolve(predictor, 0, true);

    check(!first.taken && !second.taken,
        "the initially weak-not-taken predictor misses twice");
    check(third.taken,
        "the chooser switches to the trained bimodal component");
    check((third.context & 0x300u) == 0x200u,
        "context preserves disagreeing Gshare and bimodal predictions");

    predictor.compute_next();
    BranchPrediction fourth = predictor.predict(0, 8);
    check((fourth.context & 0xFFu) == 7,
        "Gshare history advances with the selected tournament prediction");
}
}

int main() {
    test_component_training_and_speculative_history();

    if (failures != 0) {
        std::cerr << failures << " branch-predictor checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "branch-predictor checks passed\n";
    return EXIT_SUCCESS;
}
