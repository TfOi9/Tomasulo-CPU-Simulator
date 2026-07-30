#include "../include/tomasulo/cpu.hpp"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <object file path> [trace: true/false]" << std::endl;
        return 1;
    }
    std::string path = argv[1];
    bool trace = true;
    if (argc >= 3) {
        std::string trace_arg = argv[2];
        trace = (trace_arg == "true" || trace_arg == "1");
    }
    TomasuloCPU cpu(trace);
    cpu.load_program(path);
    cpu.run(250000000);
    cpu.report();
    return 0;
}
