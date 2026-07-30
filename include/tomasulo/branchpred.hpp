#pragma once

#include <cstdint>
#include <array>
#include <cstddef>

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
public:
    GsharePredictor();
    BranchPrediction predict(uint32_t pc, int32_t imm) override;
    void update(uint32_t pc, uint32_t context, bool actual_taken) override;
    void recover() override;
    void compute_next() override;
    void clock() override;
};
