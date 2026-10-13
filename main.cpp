
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    std::cout << "Initial register A: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "Initial memory at address 10: "
              << static_cast<int>(cpu.read_memory(10))
              << std::endl;

    cpu.write_memory(10, 5);
    cpu.write_memory(11, 3);

    std::cout << "Memory at address 10: "
              << static_cast<int>(cpu.read_memory(10))
              << std::endl;

    std::cout << "Memory at address 11: "
              << static_cast<int>(cpu.read_memory(11))
              << std::endl;

    cpu.register_a = cpu.read_memory(10);
    cpu.add(cpu.read_memory(11));

    std::cout << "Register A after adding memory values: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    return 0;
}
