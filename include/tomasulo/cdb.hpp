#pragma once

#include <cassert>
#include <cstdint>

struct CDBEntry {
    bool valid = false;
    uint32_t rob_tag = 0;
    uint32_t val = 0;
    bool is_branch = false;
    bool branch_actual_taken = false;
    uint32_t branch_target = 0;
};

class CDB {
    CDBEntry entry{};

public:
    bool empty() const {
        return !entry.valid;
    }

    const CDBEntry& inspect() const {
        return entry;
    }

    void publish(const CDBEntry& next) {
        assert(!entry.valid);
        assert(next.valid);
        entry = next;
    }

    void clear() {
        entry = {};
    }

    void invalidate_squashed(uint32_t surviving_tag) {
        if (entry.valid && entry.rob_tag > surviving_tag) {
            clear();
        }
    }
};
