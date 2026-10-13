
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    // Program: A = 5, A = A + 3, HALT
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 5);

    cpu.write_memory(2, CPU::OP_ADD);
    cpu.write_memory(3, 3);

    cpu.write_memory(4, CPU::OP_HALT);

    cpu.run();

    std::cout << "Register A: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "Program Counter: "
              << static_cast<int>(cpu.program_counter)
              << std::endl;

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no")
              << std::endl;

    return 0;
}
