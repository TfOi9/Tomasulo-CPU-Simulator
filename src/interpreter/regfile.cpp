#include "../../include/interpreter/regfile.hpp"

#include <cassert>
#include <ios>
#include <string>
#include <sstream>
#include <iomanip>

uint32_t RegFile::read_reg(size_t index) const {
    assert(index >= 0 && index < FILE_SIZE);
    return regs[index];
}

void RegFile::write_reg(size_t index, uint32_t val) {
    assert(index >= 0 && index < FILE_SIZE);
    regs[index] = val;
}

std::string RegFile::dump() const {
    std::stringstream ss;

    ss << std::setw(4) << "#"
       << std::setw(5) << "name"
       << std::setw(9) << "value"
       << std::endl;
    for (size_t i = 0; i < FILE_SIZE; i++) {
        ss << std::setw(4) << reg_indices[i].c_str()
           << std::setw(5) << reg_names[i].c_str()
           << std::setw(9) << std::uppercase << std::hex << regs[i]
           << std::endl;
    }

    return ss.str();
}