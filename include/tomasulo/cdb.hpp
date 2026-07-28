#pragma once

#include <cstdint>

// Common Data Bus Entry
struct CDBEntry {
    bool valid;
    uint32_t rob_tag;
    uint32_t val;
    bool is_branch;
    bool branch_actual_taken;
    uint32_t branch_target;
};