
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    // a = 5
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 5);

    // jump to address 6
    cpu.write_memory(2, CPU::OP_JMP);
    cpu.write_memory(3, 6);

    // these instructions should be skipped
    cpu.write_memory(4, CPU::OP_LOAD);
    cpu.write_memory(5, 99);

    // stop at address 6
    cpu.write_memory(6, CPU::OP_HALT);

    cpu.run();

    std::cout << "Register A: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "Program Counter: "
              << cpu.program_counter
              << std::endl;

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no")
              << std::endl;

    return 0;
}
