#pragma once

#include <cstdint>
#include <array>
#include <cstddef>
#include <deque>

class TAGEPredictorTestPeer;

struct BranchPrediction {
    bool taken;
    uint32_t target;
    uint32_t context;
};

class BranchPredictor {
public:
    // predict whether the branch will be taken
    // given program counter and jump offset
    // returns taken or not(bool) and new pc(uint32_t)
    virtual BranchPrediction predict(uint32_t pc, int32_t imm) = 0;

    // update status by give branch taken information
    virtual void update(uint32_t pc, uint32_t context, bool actual_taken) = 0;

    // restore the predictor state if we need to flush
    virtual void recover() = 0;

    virtual void compute_next() = 0;
    virtual void clock() = 0;

    // virtual destructor
    virtual ~BranchPredictor();

};

// Naive predictor.
class NaivePredictor: public BranchPredictor {
public:
    // naive predictor will always return not taken
    BranchPrediction predict(uint32_t pc, int32_t imm) override;
    // naive predictor does nothing
    void update(uint32_t pc, uint32_t context, bool actual_taken) override;
    void recover() override;
    void compute_next() override;
    void clock() override;
    
};

// Small bimodal 2-bit predictor.
class BimodalPredictor: public BranchPredictor {
    static constexpr size_t TABLE_SIZE = 1024;
    std::array<uint8_t, TABLE_SIZE> counters{};
    std::array<uint8_t, TABLE_SIZE> next_counters{};
public:
    BimodalPredictor();
    BranchPrediction predict(uint32_t pc, int32_t imm) override;
    void update(uint32_t pc, uint32_t context, bool actual_taken) override;
    void recover() override;
    void compute_next() override;
    void clock() override;
};

// Gshare predictor
class GsharePredictor: public BranchPredictor {
    static constexpr size_t GLEN = 8;
    // global history register, committed
    uint8_t committed_ghr;
    uint8_t next_committed_ghr;
    // global history register, speculative
    uint8_t speculative_ghr;
    uint8_t next_speculative_ghr;
    // pattern history table
    std::array<uint8_t, (1 << GLEN)> pht{};
    std::array<uint8_t, (1 << GLEN)> next_pht{};

    BranchPrediction lookup(uint32_t pc, int32_t imm) const;
    void speculate(bool taken);
    friend class TournamentPredictor;

public:
    GsharePredictor();
    BranchPrediction predict(uint32_t pc, int32_t imm) override;
    void update(uint32_t pc, uint32_t context, bool actual_taken) override;
    void recover() override;
    void compute_next() override;
    void clock() override;
};

// Tournament predictor
class TournamentPredictor: public BranchPredictor {
    static constexpr size_t TABLE_SIZE = 1024;
    static constexpr uint32_t GSHARE_CONTEXT_MASK = 0xFF;
    static constexpr uint32_t GSHARE_PRED_BIT = 1u << 8;
    static constexpr uint32_t BIMODAL_PRED_BIT = 1u << 9;
    std::array<uint8_t, TABLE_SIZE> counters{};
    std::array<uint8_t, TABLE_SIZE> next_counters{};

    BimodalPredictor bp;
    GsharePredictor gp;

public:
    TournamentPredictor();
    BranchPrediction predict(uint32_t pc, int32_t imm) override;
    void update(uint32_t pc, uint32_t context, bool actual_taken) override;
    void recover() override;
    void compute_next() override;
    void clock() override;
};

// TAGE predictor.
class TAGEPredictor final: public BranchPredictor {
    static constexpr size_t NUM_TAGGED_TABLES = 7;
    static constexpr size_t TAGGED_TABLE_SIZE = 4096;
    static constexpr size_t BASE_TABLE_SIZE = 16384;
    static constexpr size_t MAX_HISTORY = 256;

    static constexpr std::array<uint16_t, NUM_TAGGED_TABLES>
        HISTORY_LENGTHS = {5, 9, 15, 25, 44, 76, 130};

    static constexpr std::array<uint8_t, NUM_TAGGED_TABLES>
        TAG_WIDTHS = {9, 9, 10, 10, 11, 11, 12};

    struct TaggedEntry {
        uint16_t tag = 0;

        // Logical signed 3-bit value in [-4, 3].
        // Prediction is taken when counter >= 0.
        int8_t counter = -1;

        // Logical 2-bit value in [0, 3].
        uint8_t useful = 0;

        bool valid = false;
    };

    struct FoldedHistory {
        uint16_t original_length = 0;
        uint8_t compressed_length = 0;
        uint16_t value = 0;

        void update(
            const std::array<uint8_t, MAX_HISTORY>& history,
            uint16_t head,
            bool newest_bit
        );
    };

    struct HistoryState {
        // Circular buffer; head identifies the newest bit.
        std::array<uint8_t, MAX_HISTORY> direction{};
        uint16_t head = 0;

        // One PC-derived bit per predicted conditional branch.
        uint16_t path = 0;

        std::array<FoldedHistory, NUM_TAGGED_TABLES> index_fold;
        std::array<FoldedHistory, NUM_TAGGED_TABLES> tag_fold_a;
        std::array<FoldedHistory, NUM_TAGGED_TABLES> tag_fold_b;

        void push(bool taken, uint32_t pc);
    };

    struct PredictionCheckpoint {
        uint32_t token = 0;
        uint32_t pc = 0;
        uint16_t base_index = 0;

        std::array<uint16_t, NUM_TAGGED_TABLES> indices{};
        std::array<uint16_t, NUM_TAGGED_TABLES> tags{};

        // -1 means the bimodal base predictor.
        int8_t provider = -1;
        int8_t alternate = -1;

        bool provider_prediction = false;
        bool alternate_prediction = false;
        bool final_prediction = false;

        // True when provider.u == 0 and its counter is weak.
        bool provider_was_new = false;
    };

    enum class WriteKind : uint8_t {
        Base,
        Tagged
    };

    struct PendingWrite {
        WriteKind kind = WriteKind::Base;
        uint8_t table = 0;
        uint16_t index = 0;

        uint8_t base_counter = 0;
        TaggedEntry tagged_entry{};
    };

    // Logical 2-bit bimodal counters.
    std::array<uint8_t, BASE_TABLE_SIZE> base_table{};

    std::array<
        std::array<TaggedEntry, TAGGED_TABLE_SIZE>,
        NUM_TAGGED_TABLES
    > tagged_tables{};

    HistoryState committed_history;
    HistoryState speculative_history;
    HistoryState next_committed_history;
    HistoryState next_speculative_history;

    // Conditional branches are committed in prediction order.
    std::deque<PredictionCheckpoint> checkpoints;
    uint32_t next_token = 1;

    // At most one provider plus the longer-history candidates can change.
    std::array<PendingWrite, NUM_TAGGED_TABLES + 1> pending_writes{};
    size_t pending_write_count = 0;

    // Signed logical 4-bit range [-8, 7].
    int8_t use_alternate_on_new = 0;
    int8_t next_use_alternate_on_new = 0;

    uint64_t committed_conditional_branches = 0;
    uint64_t next_committed_conditional_branches = 0;

    // Alternates between clearing useful bit 1 and bit 0.
    uint8_t aging_bit = 1;
    bool age_at_clock = false;

    // Fixed seed makes allocation and tests reproducible.
    uint16_t allocation_lfsr = 1;

    uint16_t compute_index(
        size_t table,
        uint32_t pc,
        const HistoryState& history
    ) const;

    uint16_t compute_tag(
        size_t table,
        uint32_t pc,
        const HistoryState& history
    ) const;

    uint32_t allocate_token();

    PredictionCheckpoint& find_checkpoint(uint32_t token);

    void stage_base_write(uint16_t index, uint8_t value);

    void stage_tagged_write(
        size_t table,
        uint16_t index,
        const TaggedEntry& value
    );

    static bool tagged_prediction(const TaggedEntry& entry);
    static bool is_new_entry(const TaggedEntry& entry);

    static void update_base_counter(uint8_t& counter, bool taken);
    static void update_tagged_counter(int8_t& counter, bool taken);

    size_t choose_allocation_table(
        const PredictionCheckpoint& checkpoint
    );

    void update_use_alternate_counter(
        const PredictionCheckpoint& checkpoint,
        bool actual_taken
    );

    friend class TAGEPredictorTestPeer;

public:
    TAGEPredictor();

    BranchPrediction predict(uint32_t pc, int32_t imm) override;

    void update(
        uint32_t pc,
        uint32_t context,
        bool actual_taken
    ) override;

    void recover() override;
    void compute_next() override;
    void clock() override;
};
