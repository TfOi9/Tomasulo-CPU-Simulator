#pragma once

#include "regfile.hpp"

#include <fstream>

// a file for keeping traces of register dump
class TraceFile {
    // the register file to keep trace
    const RegFile& regf;
    // the file to dump to
    std::ofstream file;

public:
    TraceFile(const RegFile& regf);
    ~TraceFile();
    // dump the registers to the file
    void dump(uint32_t pc);

};