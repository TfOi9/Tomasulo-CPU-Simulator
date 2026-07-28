#include "../../include/tomasulo/regfile.hpp"

#include <cassert>
#include <ios>
#include <string>
#include <sstream>
#include <iomanip>

void ArchRegFile::prepare_next() {
    new_regs = regs;
}

void ArchRegFile::flush_zero() {
    new_regs[0] = 0;
}

uint32_t ArchRegFile::read_reg(size_t index) const {
    assert(index >= 0 && index < FILE_SIZE);
    if (index == 0) return 0;
    return regs[index];
}

void ArchRegFile::write_reg(size_t index, uint32_t val) {
    assert(index >= 0 && index < FILE_SIZE);
    if (index == 0) return;
    new_regs[index] = val;
}

void ArchRegFile::update() {
    std::swap(regs, new_regs);
}

std::string ArchRegFile::dump() const {
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