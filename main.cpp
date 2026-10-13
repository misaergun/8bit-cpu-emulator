
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    // Address 0: Load 3 into register A
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 3);

    // Address 2: Subtract 1 from register A
    cpu.write_memory(2, CPU::OP_SUB);
    cpu.write_memory(3, 1);

    // Address 4: If A is zero, jump to address 8
    cpu.write_memory(4, CPU::OP_JZ);
    cpu.write_memory(5, 8);

    // Address 6: Otherwise, jump back to address 2
    cpu.write_memory(6, CPU::OP_JMP);
    cpu.write_memory(7, 2);

    // Address 8: Stop the program
    cpu.write_memory(8, CPU::OP_HALT);

    cpu.run();

    std::cout << "Final register A: "
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
