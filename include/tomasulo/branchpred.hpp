#pragma once

#include <utility>
#include <cstdint>
#include <array>

class BranchPredictor {
public:
    // predict whether the branch will be taken
    // given program counter and jump offset
    // returns taken or not(bool) and new pc(uint32_t)
    virtual std::pair<bool, uint32_t> predict(uint32_t pc, int32_t imm) = 0;

    // update status by give branch taken information
    virtual void update(uint32_t pc, bool actual_taken) = 0;
    virtual void compute_next() = 0;
    virtual void clock() = 0;

    // virtual destructor
    virtual ~BranchPredictor();

};

class NaivePredictor: public BranchPredictor {
public:
    // naive predictor will always return not taken
    std::pair<bool, uint32_t> predict(uint32_t pc, int32_t imm) override;
    // naive predictor does nothing
    void update(uint32_t pc, bool actual_taken) override;
    void compute_next() override;
    void clock() override;
    
};

// Small bimodal 2-bit predictor.  It keeps loop back-edges predicted taken
// after the first iteration while retaining a well-defined fall-through
// prediction for unseen branches.
class BimodalPredictor: public BranchPredictor {
    static constexpr size_t TABLE_SIZE = 1024;
    std::array<uint8_t, TABLE_SIZE> counters{};
    std::array<uint8_t, TABLE_SIZE> next_counters{};
public:
    BimodalPredictor();
    std::pair<bool, uint32_t> predict(uint32_t pc, int32_t imm) override;
    void update(uint32_t pc, bool actual_taken) override;
    void compute_next() override;
    void clock() override;
};
