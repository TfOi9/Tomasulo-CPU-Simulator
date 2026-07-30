#include "../../include/interpreter/tracefile.hpp"

#include <sstream>
#include <cassert>
#include <ctime>
#include <iomanip>

std::string get_time() {
    auto now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(localTime, "%Y-%m-%d-%H:%M:%S");
    return oss.str();
}

TraceFile::TraceFile(const RegFile& regf, bool trace_enabled): regf(regf), trace_enabled(trace_enabled) {
    if (trace_enabled) {
        file.open("tr-" + get_time() + ".trace", std::ios::trunc);
        if (!file) {
            assert(false);
        }
    }
}

TraceFile::~TraceFile() {
    if (file.is_open()) {
        file.close();
    }
}

void TraceFile::dump(uint32_t pc) {
    if (!trace_enabled) return;
    file << "PC = " << pc << std::endl;
    file << regf.dump() << std::endl;
}