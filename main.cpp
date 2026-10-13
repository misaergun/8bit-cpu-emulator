
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    // Load the first number into register A
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 8);

    // Load the second number into register B
    cpu.write_memory(2, CPU::OP_LOAD_B);
    cpu.write_memory(3, 5);

    // Compare A and B
    cpu.write_memory(4, CPU::OP_CMP);

    // If equal, jump to address 12
    cpu.write_memory(5, CPU::OP_JE);
    cpu.write_memory(6, 12);

    // If not equal, check whether A is smaller than B.
    // For now, this example assumes A is greater.
    cpu.write_memory(7, CPU::OP_JMP);
    cpu.write_memory(8, 12);

    // Stop the program
    cpu.write_memory(12, CPU::OP_HALT);

    cpu.run();

    std::cout << "Register A: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "Register B: "
              << static_cast<int>(cpu.register_b)
              << std::endl;

    std::cout << "Registers equal: "
              << (cpu.zero_flag ? "yes" : "no")
              << std::endl;

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no")
              << std::endl;

    return 0;
}
