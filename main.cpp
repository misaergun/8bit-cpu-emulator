
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

    // If A < B, jump to address 12
    cpu.write_memory(5, CPU::OP_JL);
    cpu.write_memory(6, 12);

    // If A > B, jump to address 16
    cpu.write_memory(7, CPU::OP_JG);
    cpu.write_memory(8, 16);

    // If neither condition is true, A and B are equal
    cpu.write_memory(9, CPU::OP_JE);
    cpu.write_memory(10, 20);

    // A < B: store result 1 in register A
    cpu.write_memory(12, CPU::OP_LOAD);
    cpu.write_memory(13, 1);
    cpu.write_memory(14, CPU::OP_HALT);

    // A > B: store result 2 in register A
    cpu.write_memory(16, CPU::OP_LOAD);
    cpu.write_memory(17, 2);
    cpu.write_memory(18, CPU::OP_HALT);

    // A == B: store result 3 in register A
    cpu.write_memory(20, CPU::OP_LOAD);
    cpu.write_memory(21, 3);
    cpu.write_memory(22, CPU::OP_HALT);

    cpu.run();

    std::cout << "Comparison result: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no")
              << std::endl;

    return 0;
}
