#include "../../include/tomasulo/branchpred.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace {
uint32_t rotate_left(uint32_t value, unsigned amount) {
    amount &= 31u;
    if (amount == 0) return value;
    return (value << amount) | (value >> (32u - amount));
}
}

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

void TAGEPredictor::FoldedHistory::update(
        const std::array<uint8_t, MAX_HISTORY>& history,
        uint16_t head,
        bool newest_bit) {
    assert(compressed_length > 0);
    assert(original_length > 0);

    const uint16_t outgoing_distance =
        static_cast<uint16_t>(original_length - 1);
    const uint16_t outgoing_index = static_cast<uint16_t>(
        (head + MAX_HISTORY - outgoing_distance) % MAX_HISTORY
    );
    const uint16_t outgoing_bit = history[outgoing_index] & 1u;

    uint32_t compressed =
        (static_cast<uint32_t>(value) << 1) |
        static_cast<uint32_t>(newest_bit);
    compressed ^=
        static_cast<uint32_t>(outgoing_bit)
        << (original_length % compressed_length);
    compressed ^= compressed >> compressed_length;
    compressed &= (1u << compressed_length) - 1u;
    value = static_cast<uint16_t>(compressed);
}

void TAGEPredictor::HistoryState::push(bool taken, uint32_t pc) {
    for (size_t i = 0; i < NUM_TAGGED_TABLES; ++i) {
        index_fold[i].update(direction, head, taken);
        tag_fold_a[i].update(direction, head, taken);
        tag_fold_b[i].update(direction, head, taken);
    }

    head = static_cast<uint16_t>((head + 1) % MAX_HISTORY);
    direction[head] = static_cast<uint8_t>(taken);

    const uint16_t path_bit =
        static_cast<uint16_t>((pc >> 2) & 1u);
    path = static_cast<uint16_t>((path << 1) | path_bit);
}

TAGEPredictor::TAGEPredictor() {
    base_table.fill(1);

    for (size_t i = 0; i < NUM_TAGGED_TABLES; ++i) {
        committed_history.index_fold[i] = {
            HISTORY_LENGTHS[i],
            12,
            0
        };
        committed_history.tag_fold_a[i] = {
            HISTORY_LENGTHS[i],
            TAG_WIDTHS[i],
            0
        };
        committed_history.tag_fold_b[i] = {
            HISTORY_LENGTHS[i],
            static_cast<uint8_t>(TAG_WIDTHS[i] - 1),
            0
        };
    }

    speculative_history = committed_history;
    next_committed_history = committed_history;
    next_speculative_history = speculative_history;
}

uint16_t TAGEPredictor::compute_index(
        size_t table,
        uint32_t pc,
        const HistoryState& history) const {
    assert(table < NUM_TAGGED_TABLES);
    const uint32_t word_pc = pc >> 2;
    const uint32_t mixed_path =
        rotate_left(history.path, static_cast<unsigned>(table + 1));

    return static_cast<uint16_t>(
        (word_pc ^
         (word_pc >> (table + 1)) ^
         history.index_fold[table].value ^
         mixed_path) &
        (TAGGED_TABLE_SIZE - 1)
    );
}

uint16_t TAGEPredictor::compute_tag(
        size_t table,
        uint32_t pc,
        const HistoryState& history) const {
    assert(table < NUM_TAGGED_TABLES);
    const uint32_t word_pc = pc >> 2;
    const uint32_t width = TAG_WIDTHS[table];
    const uint32_t mask = (1u << width) - 1u;

    return static_cast<uint16_t>(
        (word_pc ^
         history.tag_fold_a[table].value ^
         (history.tag_fold_b[table].value << 1) ^
         rotate_left(history.path, static_cast<unsigned>(table))) &
        mask
    );
}

uint32_t TAGEPredictor::allocate_token() {
    for (;;) {
        uint32_t candidate = next_token++;
        if (next_token == 0) next_token = 1;
        if (candidate == 0) continue;

        bool live = false;
        for (const PredictionCheckpoint& checkpoint : checkpoints) {
            if (checkpoint.token == candidate) {
                live = true;
                break;
            }
        }
        if (!live) return candidate;
    }
}

TAGEPredictor::PredictionCheckpoint&
TAGEPredictor::find_checkpoint(uint32_t token) {
    auto found = std::find_if(
        checkpoints.begin(),
        checkpoints.end(),
        [token](const PredictionCheckpoint& checkpoint) {
            return checkpoint.token == token;
        }
    );
    if (found == checkpoints.end()) {
        throw std::logic_error("unknown TAGE prediction checkpoint");
    }
    return *found;
}

void TAGEPredictor::stage_base_write(uint16_t index, uint8_t value) {
    for (size_t i = pending_write_count; i > 0; --i) {
        PendingWrite& write = pending_writes[i - 1];
        if (write.kind == WriteKind::Base && write.index == index) {
            write.base_counter = value;
            return;
        }
    }

    assert(pending_write_count < pending_writes.size());
    PendingWrite& write = pending_writes[pending_write_count++];
    write.kind = WriteKind::Base;
    write.index = index;
    write.base_counter = value;
}

void TAGEPredictor::stage_tagged_write(
        size_t table,
        uint16_t index,
        const TaggedEntry& value) {
    assert(table < NUM_TAGGED_TABLES);
    for (size_t i = pending_write_count; i > 0; --i) {
        PendingWrite& write = pending_writes[i - 1];
        if (write.kind == WriteKind::Tagged &&
            write.table == table &&
            write.index == index) {
            write.tagged_entry = value;
            return;
        }
    }

    assert(pending_write_count < pending_writes.size());
    PendingWrite& write = pending_writes[pending_write_count++];
    write.kind = WriteKind::Tagged;
    write.table = static_cast<uint8_t>(table);
    write.index = index;
    write.tagged_entry = value;
}

bool TAGEPredictor::tagged_prediction(const TaggedEntry& entry) {
    return entry.counter >= 0;
}

bool TAGEPredictor::is_new_entry(const TaggedEntry& entry) {
    return entry.useful == 0 &&
        (entry.counter == 0 || entry.counter == -1);
}

void TAGEPredictor::update_base_counter(
        uint8_t& counter,
        bool taken) {
    if (taken) {
        counter = std::min<uint8_t>(3, counter + 1);
    } else if (counter > 0) {
        --counter;
    }
}

void TAGEPredictor::update_tagged_counter(
        int8_t& counter,
        bool taken) {
    if (taken) {
        counter = std::min<int8_t>(3, counter + 1);
    } else {
        counter = std::max<int8_t>(-4, counter - 1);
    }
}

size_t TAGEPredictor::choose_allocation_table(
        const PredictionCheckpoint& checkpoint) {
    const size_t first_candidate = checkpoint.provider < 0
        ? 0
        : static_cast<size_t>(checkpoint.provider + 1);

    std::array<size_t, NUM_TAGGED_TABLES> eligible{};
    size_t eligible_count = 0;
    for (size_t table = first_candidate;
         table < NUM_TAGGED_TABLES;
         ++table) {
        const TaggedEntry& entry =
            tagged_tables[table][checkpoint.indices[table]];
        if (!entry.valid || entry.useful == 0) {
            eligible[eligible_count++] = table;
        }
    }

    if (eligible_count == 0) return NUM_TAGGED_TABLES;

    const uint16_t feedback = static_cast<uint16_t>(
        ((allocation_lfsr >> 0) ^
         (allocation_lfsr >> 2) ^
         (allocation_lfsr >> 3) ^
         (allocation_lfsr >> 5)) & 1u
    );
    allocation_lfsr = static_cast<uint16_t>(
        (allocation_lfsr >> 1) | (feedback << 15)
    );
    if (allocation_lfsr == 0) allocation_lfsr = 1;

    const uint32_t total_weight = (1u << eligible_count) - 1u;
    uint32_t choice = allocation_lfsr % total_weight;
    for (size_t i = 0; i < eligible_count; ++i) {
        const uint32_t weight = 1u << (eligible_count - i - 1);
        if (choice < weight) return eligible[i];
        choice -= weight;
    }

    return eligible[eligible_count - 1];
}

void TAGEPredictor::update_use_alternate_counter(
        const PredictionCheckpoint& checkpoint,
        bool actual_taken) {
    if (!checkpoint.provider_was_new ||
        checkpoint.provider_prediction ==
            checkpoint.alternate_prediction) {
        return;
    }

    if (checkpoint.alternate_prediction == actual_taken) {
        next_use_alternate_on_new =
            std::min<int8_t>(7, next_use_alternate_on_new + 1);
    } else if (checkpoint.provider_prediction == actual_taken) {
        next_use_alternate_on_new =
            std::max<int8_t>(-8, next_use_alternate_on_new - 1);
    }
}

BranchPrediction TAGEPredictor::predict(uint32_t pc, int32_t imm) {
    PredictionCheckpoint checkpoint{};
    checkpoint.token = allocate_token();
    checkpoint.pc = pc;
    checkpoint.base_index = static_cast<uint16_t>(
        (pc >> 2) & (BASE_TABLE_SIZE - 1)
    );

    const bool base_prediction =
        base_table[checkpoint.base_index] >= 2;

    int provider = -1;
    int alternate = -1;
    for (size_t table = 0; table < NUM_TAGGED_TABLES; ++table) {
        checkpoint.indices[table] =
            compute_index(table, pc, next_speculative_history);
        checkpoint.tags[table] =
            compute_tag(table, pc, next_speculative_history);

        const TaggedEntry& entry =
            tagged_tables[table][checkpoint.indices[table]];
        if (!entry.valid || entry.tag != checkpoint.tags[table]) {
            continue;
        }

        alternate = provider;
        provider = static_cast<int>(table);
    }

    checkpoint.provider = static_cast<int8_t>(provider);
    checkpoint.alternate = static_cast<int8_t>(alternate);

    if (provider >= 0) {
        const TaggedEntry& provider_entry =
            tagged_tables[provider][checkpoint.indices[provider]];
        checkpoint.provider_prediction =
            tagged_prediction(provider_entry);
        checkpoint.provider_was_new =
            is_new_entry(provider_entry);
    } else {
        checkpoint.provider_prediction = base_prediction;
    }

    if (alternate >= 0) {
        checkpoint.alternate_prediction = tagged_prediction(
            tagged_tables[alternate][checkpoint.indices[alternate]]
        );
    } else {
        checkpoint.alternate_prediction = base_prediction;
    }

    const bool use_alternate =
        provider >= 0 &&
        checkpoint.provider_was_new &&
        next_use_alternate_on_new >= 0;
    checkpoint.final_prediction = use_alternate
        ? checkpoint.alternate_prediction
        : checkpoint.provider_prediction;

    const bool taken = checkpoint.final_prediction;
    checkpoints.push_back(checkpoint);
    next_speculative_history.push(taken, pc);

    return {
        taken,
        taken ? pc + uint32_t(imm) : pc + 4,
        checkpoint.token
    };
}

void TAGEPredictor::update(
        uint32_t pc,
        uint32_t context,
        bool actual_taken) {
    PredictionCheckpoint checkpoint = find_checkpoint(context);

    assert(checkpoint.pc == pc);
    assert(!checkpoints.empty());
    assert(checkpoints.front().token == context);
    if (checkpoint.pc != pc ||
        checkpoints.empty() ||
        checkpoints.front().token != context) {
        throw std::logic_error("out-of-order TAGE checkpoint update");
    }

    const bool mispredicted =
        checkpoint.final_prediction != actual_taken;

    if (checkpoint.provider >= 0) {
        const size_t table =
            static_cast<size_t>(checkpoint.provider);
        const uint16_t index = checkpoint.indices[table];
        TaggedEntry updated = tagged_tables[table][index];

        // An older branch may have replaced this entry since prediction.
        if (updated.valid &&
            updated.tag == checkpoint.tags[table]) {
            update_tagged_counter(updated.counter, actual_taken);

            if (checkpoint.provider_prediction !=
                checkpoint.alternate_prediction) {
                if (checkpoint.provider_prediction == actual_taken) {
                    updated.useful =
                        std::min<uint8_t>(3, updated.useful + 1);
                } else if (updated.useful > 0) {
                    --updated.useful;
                }
            }

            stage_tagged_write(table, index, updated);
        }
    } else {
        uint8_t updated = base_table[checkpoint.base_index];
        update_base_counter(updated, actual_taken);
        stage_base_write(checkpoint.base_index, updated);
    }

    update_use_alternate_counter(checkpoint, actual_taken);

    if (mispredicted) {
        const size_t allocated_table =
            choose_allocation_table(checkpoint);

        if (allocated_table < NUM_TAGGED_TABLES) {
            const uint16_t index =
                checkpoint.indices[allocated_table];
            TaggedEntry allocated{};
            allocated.valid = true;
            allocated.tag =
                checkpoint.tags[allocated_table];
            allocated.counter = actual_taken ? 0 : -1;
            allocated.useful = 0;
            stage_tagged_write(allocated_table, index, allocated);
        } else {
            const size_t first_longer = checkpoint.provider < 0
                ? 0
                : static_cast<size_t>(checkpoint.provider + 1);
            for (size_t table = first_longer;
                 table < NUM_TAGGED_TABLES;
                 ++table) {
                const uint16_t index = checkpoint.indices[table];
                TaggedEntry aged = tagged_tables[table][index];
                if (aged.useful > 0) {
                    --aged.useful;
                    stage_tagged_write(table, index, aged);
                }
            }
        }
    }

    next_committed_history.push(actual_taken, pc);

    ++next_committed_conditional_branches;
    if ((next_committed_conditional_branches % 262144) == 0) {
        age_at_clock = true;
    }

    checkpoints.pop_front();
}

void TAGEPredictor::recover() {
    next_speculative_history = next_committed_history;
    checkpoints.clear();
}

void TAGEPredictor::compute_next() {
    next_committed_history = committed_history;
    next_speculative_history = speculative_history;
    next_use_alternate_on_new = use_alternate_on_new;
    next_committed_conditional_branches =
        committed_conditional_branches;
    pending_write_count = 0;
    age_at_clock = false;
}

void TAGEPredictor::clock() {
    for (size_t i = 0; i < pending_write_count; ++i) {
        const PendingWrite& write = pending_writes[i];
        if (write.kind == WriteKind::Base) {
            base_table[write.index] = write.base_counter;
        } else {
            tagged_tables[write.table][write.index] =
                write.tagged_entry;
        }
    }

    if (age_at_clock) {
        const uint8_t mask = static_cast<uint8_t>(
            ~(1u << aging_bit)
        );
        for (auto& table : tagged_tables) {
            for (TaggedEntry& entry : table) {
                entry.useful &= mask;
            }
        }
        aging_bit ^= 1u;
    }

    committed_history = next_committed_history;
    speculative_history = next_speculative_history;
    use_alternate_on_new = next_use_alternate_on_new;
    committed_conditional_branches =
        next_committed_conditional_branches;
}
