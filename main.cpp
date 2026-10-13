
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    std::cout << "Initial PC: "
              << static_cast<int>(cpu.program_counter)
              << std::endl;

    cpu.advance_program_counter();

    std::cout << "PC after one step: "
              << static_cast<int>(cpu.program_counter)
              << std::endl;

    cpu.advance_program_counter();

    std::cout << "PC after two steps: "
              << static_cast<int>(cpu.program_counter)
              << std::endl;

    cpu.write_memory(10, 5);
    cpu.write_memory(11, 3);

    cpu.register_a = cpu.read_memory(10);
    cpu.add(cpu.read_memory(11));

    std::cout << "Register A after addition: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    return 0;
}
