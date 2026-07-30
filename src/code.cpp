#include "../include/tomasulo/cpu.hpp"

int main() {
    TomasuloCPU cpu(false);
    cpu.input_program();
    cpu.run(500000000);
    return 0;
}
