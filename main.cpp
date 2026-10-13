
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    // Address 0: Load 10 into register A
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 10);

    // Address 2: Store register A at memory address 20
    cpu.write_memory(2, CPU::OP_STORE);
    cpu.write_memory(3, 20);

    // Address 4: Change register A to 99
    cpu.write_memory(4, CPU::OP_LOAD);
    cpu.write_memory(5, 99);

    // Address 6: Load value from memory address 20
    cpu.write_memory(6, CPU::OP_LOAD_MEM);
    cpu.write_memory(7, 20);

    // Address 8: Stop the program
    cpu.write_memory(8, CPU::OP_HALT);

    cpu.run();

    std::cout << "Final register A: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "Stored value at address 20: "
              << static_cast<int>(cpu.read_memory(20))
              << std::endl;

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no")
              << std::endl;

    return 0;
}
