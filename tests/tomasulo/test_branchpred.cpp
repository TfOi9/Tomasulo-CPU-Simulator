#include "../../include/tomasulo/branchpred.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

class TAGEPredictorTestPeer {
public:
    static constexpr size_t table_count() {
        return TAGEPredictor::NUM_TAGGED_TABLES;
    }

    static void set_base(
            TAGEPredictor& predictor,
            uint32_t pc,
            uint8_t counter) {
        predictor.base_table[
            (pc >> 2) & (TAGEPredictor::BASE_TABLE_SIZE - 1)
        ] = counter;
    }

    static uint16_t current_index(
            const TAGEPredictor& predictor,
            size_t table,
            uint32_t pc) {
        return predictor.compute_index(
            table,
            pc,
            predictor.next_speculative_history
        );
    }

    static uint16_t current_tag(
            const TAGEPredictor& predictor,
            size_t table,
            uint32_t pc) {
        return predictor.compute_tag(
            table,
            pc,
            predictor.next_speculative_history
        );
    }

    static void install_match(
            TAGEPredictor& predictor,
            size_t table,
            uint32_t pc,
            int8_t counter,
            uint8_t useful) {
        const uint16_t index = current_index(predictor, table, pc);
        predictor.tagged_tables[table][index] = {
            current_tag(predictor, table, pc),
            counter,
            useful,
            true
        };
    }

    static void install_nonmatch(
            TAGEPredictor& predictor,
            size_t table,
            uint32_t pc,
            int8_t counter,
            uint8_t useful) {
        const uint16_t index = current_index(predictor, table, pc);
        const uint16_t tag = current_tag(predictor, table, pc);
        predictor.tagged_tables[table][index] = {
            static_cast<uint16_t>(tag ^ 1u),
            counter,
            useful,
            true
        };
    }

    static int provider(const TAGEPredictor& predictor) {
        return predictor.checkpoints.back().provider;
    }

    static int alternate(const TAGEPredictor& predictor) {
        return predictor.checkpoints.back().alternate;
    }

    static bool provider_prediction(const TAGEPredictor& predictor) {
        return predictor.checkpoints.back().provider_prediction;
    }

    static bool alternate_prediction(const TAGEPredictor& predictor) {
        return predictor.checkpoints.back().alternate_prediction;
    }

    static uint16_t last_index(
            const TAGEPredictor& predictor,
            size_t table) {
        return predictor.checkpoints.back().indices[table];
    }

    static uint16_t last_tag(
            const TAGEPredictor& predictor,
            size_t table) {
        return predictor.checkpoints.back().tags[table];
    }

    static uint8_t useful(
            const TAGEPredictor& predictor,
            size_t table,
            uint16_t index) {
        return predictor.tagged_tables[table][index].useful;
    }

    static int8_t tagged_counter(
            const TAGEPredictor& predictor,
            size_t table,
            uint16_t index) {
        return predictor.tagged_tables[table][index].counter;
    }

    static uint16_t entry_tag(
            const TAGEPredictor& predictor,
            size_t table,
            uint16_t index) {
        return predictor.tagged_tables[table][index].tag;
    }

    static size_t valid_entry_count(const TAGEPredictor& predictor) {
        size_t count = 0;
        for (const auto& table : predictor.tagged_tables) {
            for (const auto& entry : table) {
                count += entry.valid ? 1u : 0u;
            }
        }
        return count;
    }

    static size_t valid_entries_with_counter(
            const TAGEPredictor& predictor,
            int8_t counter) {
        size_t count = 0;
        for (const auto& table : predictor.tagged_tables) {
            for (const auto& entry : table) {
                count += entry.valid && entry.counter == counter ? 1u : 0u;
            }
        }
        return count;
    }

    static void set_monitor(TAGEPredictor& predictor, int8_t value) {
        predictor.use_alternate_on_new = value;
        predictor.next_use_alternate_on_new = value;
    }

    static int8_t next_monitor(const TAGEPredictor& predictor) {
        return predictor.next_use_alternate_on_new;
    }

    static void update_base_counter(uint8_t& counter, bool taken) {
        TAGEPredictor::update_base_counter(counter, taken);
    }

    static void update_tagged_counter(int8_t& counter, bool taken) {
        TAGEPredictor::update_tagged_counter(counter, taken);
    }

    static uint8_t next_direction(
            const TAGEPredictor& predictor,
            size_t distance) {
        const auto& history = predictor.next_speculative_history;
        const size_t index =
            (history.head + TAGEPredictor::MAX_HISTORY - distance) %
            TAGEPredictor::MAX_HISTORY;
        return history.direction[index];
    }

    static uint8_t speculative_direction(
            const TAGEPredictor& predictor,
            size_t distance) {
        const auto& history = predictor.speculative_history;
        const size_t index =
            (history.head + TAGEPredictor::MAX_HISTORY - distance) %
            TAGEPredictor::MAX_HISTORY;
        return history.direction[index];
    }

    static uint16_t speculative_head(const TAGEPredictor& predictor) {
        return predictor.speculative_history.head;
    }

    static size_t checkpoint_count(const TAGEPredictor& predictor) {
        return predictor.checkpoints.size();
    }

    static void displace_last_provider(
            TAGEPredictor& predictor,
            uint16_t replacement_tag,
            int8_t replacement_counter) {
        const auto& checkpoint = predictor.checkpoints.front();
        const size_t table = static_cast<size_t>(checkpoint.provider);
        const uint16_t index = checkpoint.indices[table];
        predictor.tagged_tables[table][index] = {
            replacement_tag,
            replacement_counter,
            2,
            true
        };
    }

    static void set_all_useful(
            TAGEPredictor& predictor,
            uint8_t value) {
        for (auto& table : predictor.tagged_tables) {
            table[0].valid = true;
            table[0].useful = value;
        }
    }

    static void schedule_aging(TAGEPredictor& predictor) {
        predictor.age_at_clock = true;
    }

    static void set_committed_branch_count(
            TAGEPredictor& predictor,
            uint64_t value) {
        predictor.committed_conditional_branches = value;
        predictor.next_committed_conditional_branches = value;
    }

    static uint8_t aging_bit(const TAGEPredictor& predictor) {
        return predictor.aging_bit;
    }

    static void add_live_token(
            TAGEPredictor& predictor,
            uint32_t token) {
        TAGEPredictor::PredictionCheckpoint checkpoint{};
        checkpoint.token = token;
        predictor.checkpoints.push_back(checkpoint);
    }

    static void set_next_token(
            TAGEPredictor& predictor,
            uint32_t token) {
        predictor.next_token = token;
    }
};

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        failures++;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void test_base_provider_and_alternate_selection() {
    constexpr uint32_t pc = 0x100;
    TAGEPredictor predictor;
    predictor.compute_next();
    TAGEPredictorTestPeer::set_base(predictor, pc, 3);

    BranchPrediction base = predictor.predict(pc, 12);
    check(base.taken && base.target == pc + 12,
        "no tagged match uses the taken bimodal prediction");
    check(TAGEPredictorTestPeer::provider(predictor) == -1,
        "no tagged match records the base predictor as provider");

    TAGEPredictor tagged;
    tagged.compute_next();
    TAGEPredictorTestPeer::set_base(tagged, pc, 1);
    TAGEPredictorTestPeer::install_match(tagged, 1, pc, 2, 1);
    TAGEPredictorTestPeer::install_match(tagged, 5, pc, 2, 1);
    BranchPrediction prediction = tagged.predict(pc, 12);

    check(prediction.taken,
        "the longest matching tagged component supplies the prediction");
    check(TAGEPredictorTestPeer::provider(tagged) == 5,
        "the longest matching table becomes provider");
    check(TAGEPredictorTestPeer::alternate(tagged) == 1,
        "the next-longest matching table becomes alternate");
}

void test_new_provider_monitor() {
    constexpr uint32_t pc = 0x180;
    TAGEPredictor use_alternate;
    use_alternate.compute_next();
    TAGEPredictorTestPeer::set_base(use_alternate, pc, 1);
    TAGEPredictorTestPeer::install_match(
        use_alternate, 4, pc, 0, 0
    );
    TAGEPredictorTestPeer::set_monitor(use_alternate, 0);

    BranchPrediction alternate = use_alternate.predict(pc, 8);
    check(!alternate.taken,
        "a weak new provider uses the alternate when the monitor is nonnegative");
    check(TAGEPredictorTestPeer::provider_prediction(use_alternate) &&
          !TAGEPredictorTestPeer::alternate_prediction(use_alternate),
        "the checkpoint preserves provider/alternate disagreement");

    TAGEPredictor use_provider;
    use_provider.compute_next();
    TAGEPredictorTestPeer::set_base(use_provider, pc, 1);
    TAGEPredictorTestPeer::install_match(
        use_provider, 4, pc, 0, 0
    );
    TAGEPredictorTestPeer::set_monitor(use_provider, -1);

    check(use_provider.predict(pc, 8).taken,
        "a negative monitor selects the weak new provider");
}

void test_counter_saturation() {
    uint8_t base = 3;
    TAGEPredictorTestPeer::update_base_counter(base, true);
    check(base == 3, "the bimodal counter saturates at 3");
    base = 0;
    TAGEPredictorTestPeer::update_base_counter(base, false);
    check(base == 0, "the bimodal counter saturates at 0");

    int8_t tagged = 3;
    TAGEPredictorTestPeer::update_tagged_counter(tagged, true);
    check(tagged == 3, "the tagged counter saturates at 3");
    tagged = -4;
    TAGEPredictorTestPeer::update_tagged_counter(tagged, false);
    check(tagged == -4, "the tagged counter saturates at -4");
}

void test_usefulness_and_monitor_training() {
    constexpr uint32_t pc = 0x200;
    TAGEPredictor disagreement;
    disagreement.compute_next();
    TAGEPredictorTestPeer::set_base(disagreement, pc, 0);
    TAGEPredictorTestPeer::install_match(
        disagreement, 3, pc, 1, 1
    );
    BranchPrediction prediction = disagreement.predict(pc, 8);
    const uint16_t index =
        TAGEPredictorTestPeer::last_index(disagreement, 3);
    disagreement.update(pc, prediction.context, true);
    disagreement.clock();
    check(TAGEPredictorTestPeer::useful(disagreement, 3, index) == 2,
        "a correct provider gains usefulness when alternate disagrees");

    TAGEPredictor agreement;
    agreement.compute_next();
    TAGEPredictorTestPeer::set_base(agreement, pc, 3);
    TAGEPredictorTestPeer::install_match(agreement, 3, pc, 1, 1);
    prediction = agreement.predict(pc, 8);
    const uint16_t same_index =
        TAGEPredictorTestPeer::last_index(agreement, 3);
    agreement.update(pc, prediction.context, true);
    agreement.clock();
    check(TAGEPredictorTestPeer::useful(agreement, 3, same_index) == 1,
        "usefulness is unchanged when provider and alternate agree");

    TAGEPredictor monitor;
    monitor.compute_next();
    TAGEPredictorTestPeer::set_base(monitor, pc, 0);
    TAGEPredictorTestPeer::install_match(monitor, 2, pc, 0, 0);
    TAGEPredictorTestPeer::set_monitor(monitor, 0);
    prediction = monitor.predict(pc, 8);
    monitor.update(pc, prediction.context, true);
    check(TAGEPredictorTestPeer::next_monitor(monitor) == -1,
        "the monitor moves toward a correct weak provider");
}

void test_allocation_and_candidate_aging() {
    constexpr uint32_t pc = 0x280;
    TAGEPredictor allocate;
    allocate.compute_next();
    BranchPrediction prediction = allocate.predict(pc, 8);
    allocate.update(pc, prediction.context, true);
    allocate.recover();
    allocate.clock();

    check(TAGEPredictorTestPeer::valid_entry_count(allocate) == 1,
        "a misprediction allocates exactly one tagged entry");
    check(TAGEPredictorTestPeer::valid_entries_with_counter(allocate, 0) == 1,
        "allocation initializes the tagged counter weakly toward taken");

    TAGEPredictor allocate_not_taken;
    allocate_not_taken.compute_next();
    TAGEPredictorTestPeer::set_base(allocate_not_taken, pc, 3);
    prediction = allocate_not_taken.predict(pc, 8);
    allocate_not_taken.update(pc, prediction.context, false);
    allocate_not_taken.recover();
    allocate_not_taken.clock();
    check(
        TAGEPredictorTestPeer::valid_entries_with_counter(
            allocate_not_taken, -1
        ) == 1,
        "allocation initializes the tagged counter weakly toward not taken"
    );

    TAGEPredictor age_candidates;
    age_candidates.compute_next();
    for (size_t table = 0;
         table < TAGEPredictorTestPeer::table_count();
         ++table) {
        TAGEPredictorTestPeer::install_nonmatch(
            age_candidates, table, pc, -1, 2
        );
    }
    prediction = age_candidates.predict(pc, 8);
    std::array<uint16_t, 7> indices{};
    for (size_t table = 0; table < indices.size(); ++table) {
        indices[table] =
            TAGEPredictorTestPeer::last_index(age_candidates, table);
    }
    age_candidates.update(pc, prediction.context, true);
    age_candidates.recover();
    age_candidates.clock();
    for (size_t table = 0; table < indices.size(); ++table) {
        check(
            TAGEPredictorTestPeer::useful(
                age_candidates, table, indices[table]
            ) == 1,
            "no eligible allocation decrements candidate usefulness"
        );
    }
}

void test_periodic_aging_bits() {
    TAGEPredictor predictor;
    predictor.compute_next();
    TAGEPredictorTestPeer::set_all_useful(predictor, 3);
    TAGEPredictorTestPeer::set_committed_branch_count(predictor, 262143);
    BranchPrediction prediction = predictor.predict(0x2c0, 8);
    predictor.update(0x2c0, prediction.context, false);
    predictor.clock();
    check(TAGEPredictorTestPeer::useful(predictor, 0, 0) == 1,
        "the periodic threshold clears useful bit 1");
    check(TAGEPredictorTestPeer::aging_bit(predictor) == 0,
        "the global aging selector alternates to bit 0");

    predictor.compute_next();
    TAGEPredictorTestPeer::set_all_useful(predictor, 3);
    TAGEPredictorTestPeer::schedule_aging(predictor);
    predictor.clock();
    check(TAGEPredictorTestPeer::useful(predictor, 0, 0) == 2,
        "the second global aging pass clears useful bit 0");
    check(TAGEPredictorTestPeer::aging_bit(predictor) == 1,
        "the global aging selector alternates back to bit 1");
}

void test_speculation_commit_and_recovery() {
    constexpr uint32_t older_pc = 0x300;
    constexpr uint32_t younger_pc = 0x304;

    TAGEPredictor ordered;
    ordered.compute_next();
    TAGEPredictorTestPeer::set_base(ordered, older_pc, 3);
    TAGEPredictorTestPeer::set_base(ordered, younger_pc, 0);
    ordered.predict(older_pc, 8);
    ordered.predict(younger_pc, 8);
    check(TAGEPredictorTestPeer::next_direction(ordered, 0) == 0 &&
          TAGEPredictorTestPeer::next_direction(ordered, 1) == 1,
        "multiple predictions append speculative outcomes in order");

    TAGEPredictor correct;
    correct.compute_next();
    TAGEPredictorTestPeer::set_base(correct, older_pc, 3);
    BranchPrediction older = correct.predict(older_pc, 8);
    correct.clock();

    correct.compute_next();
    TAGEPredictorTestPeer::set_base(correct, younger_pc, 0);
    correct.predict(younger_pc, 8);
    correct.update(older_pc, older.context, true);
    correct.clock();
    check(TAGEPredictorTestPeer::speculative_direction(correct, 0) == 0 &&
          TAGEPredictorTestPeer::speculative_direction(correct, 1) == 1,
        "a correct commit retains younger speculative history");
    check(TAGEPredictorTestPeer::checkpoint_count(correct) == 1,
        "a correct commit retains the younger checkpoint");

    TAGEPredictor recovered;
    recovered.compute_next();
    older = recovered.predict(older_pc, 8);
    recovered.clock();
    recovered.compute_next();
    recovered.predict(younger_pc, 8);
    recovered.update(older_pc, older.context, true);
    recovered.recover();
    recovered.clock();
    check(TAGEPredictorTestPeer::checkpoint_count(recovered) == 0,
        "misprediction recovery discards all younger checkpoints");
    check(TAGEPredictorTestPeer::speculative_head(recovered) == 1 &&
          TAGEPredictorTestPeer::speculative_direction(recovered, 0) == 1,
        "recovery restores speculative history through the actual outcome");
}

void test_displaced_provider_and_token_wrap() {
    constexpr uint32_t pc = 0x380;
    TAGEPredictor displaced;
    displaced.compute_next();
    TAGEPredictorTestPeer::install_match(displaced, 4, pc, 2, 2);
    BranchPrediction prediction = displaced.predict(pc, 8);
    const uint16_t index =
        TAGEPredictorTestPeer::last_index(displaced, 4);
    const uint16_t old_tag =
        TAGEPredictorTestPeer::last_tag(displaced, 4);
    const uint16_t replacement_tag =
        static_cast<uint16_t>(old_tag ^ 1u);
    TAGEPredictorTestPeer::displace_last_provider(
        displaced, replacement_tag, -3
    );
    displaced.update(pc, prediction.context, true);
    displaced.clock();
    check(TAGEPredictorTestPeer::entry_tag(displaced, 4, index) ==
              replacement_tag &&
          TAGEPredictorTestPeer::tagged_counter(displaced, 4, index) == -3,
        "a displaced tagged provider is not accidentally trained");

    TAGEPredictor wrapping;
    wrapping.compute_next();
    TAGEPredictorTestPeer::add_live_token(
        wrapping, std::numeric_limits<uint32_t>::max()
    );
    TAGEPredictorTestPeer::set_next_token(
        wrapping, std::numeric_limits<uint32_t>::max()
    );
    BranchPrediction first = wrapping.predict(pc, 8);
    BranchPrediction second = wrapping.predict(pc + 4, 8);
    check(first.context != 0 && second.context != 0,
        "token wraparound never returns token zero");
    check(first.context != second.context &&
          first.context != std::numeric_limits<uint32_t>::max(),
        "token wraparound skips live tokens and avoids duplicates");
}
}

int main() {
    test_base_provider_and_alternate_selection();
    test_new_provider_monitor();
    test_counter_saturation();
    test_usefulness_and_monitor_training();
    test_allocation_and_candidate_aging();
    test_periodic_aging_bits();
    test_speculation_commit_and_recovery();
    test_displaced_provider_and_token_wrap();

    if (failures != 0) {
        std::cerr << failures << " branch-predictor checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "branch-predictor checks passed\n";
    return EXIT_SUCCESS;
}
