#pragma once

#include "regfile.hpp"

#include <fstream>

// a file for keeping traces of register dump
class TraceFile {
    // the register file to keep trace
    const ArchRegFile& regf;
    // the file to dump to
    std::ofstream file;
    // trace enabled flag
    bool trace_enabled;

public:
    TraceFile(const ArchRegFile& regf, bool trace_enabled);
    ~TraceFile();
    // dump the registers to the file
    void dump(uint32_t pc);

};