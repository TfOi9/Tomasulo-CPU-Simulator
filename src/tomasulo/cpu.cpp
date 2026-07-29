#include "../../include/tomasulo/cpu.hpp"
#include "../../include/tomasulo/branchpred.hpp"

#include <fstream>
#include <sstream>
#include <cassert>

TomasuloCPU::TomasuloCPU(bool trace): tf(regf, trace) {
    bp = new NaivePredictor();
}

TomasuloCPU::~TomasuloCPU() {
    delete bp;
}

void TomasuloCPU::load_program(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        assert(false);
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    imem.load_hex_data(ss.str());
    dmem.load_hex_data(ss.str());

    pc = 0;
    halted = false;
}