#pragma once

#include <utility>
#include <cstdint>

class BranchPredictor {
public:
    // predict whether the branch will be taken
    // given program counter and jump offset
    // returns taken or not(bool) and new pc(uint32_t)
    virtual std::pair<bool, uint32_t> predict(uint32_t pc, int32_t imm) = 0;

    // update status by give branch taken information
    virtual void update(uint32_t pc, bool actual_taken) = 0;

};

class NaivePredictor: public BranchPredictor {
public:
    // naive predictor will always return not taken
    std::pair<bool, uint32_t> predict(uint32_t pc, int32_t imm);
    // naive predictor does nothing
    void update(uint32_t pc, bool actual_taken);
    
};