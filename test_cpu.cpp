
#include <iostream>
#include <stdexcept>
#include "cpu.h"

int main() {
    CPU cpu;

    // Test 1: Normal program
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 5);
    cpu.write_memory(2, CPU::OP_ADD);
    cpu.write_memory(3, 3);
    cpu.write_memory(4, CPU::OP_HALT);

    cpu.run();

    if (cpu.register_a != 8) {
        std::cerr << "FAIL: Normal program" << std::endl;
        return 1;
    }

    std::cout << "PASS: Normal program" << std::endl;

    // Test 2: Invalid memory address
    try {
        cpu.read_memory(256);

        std::cerr << "FAIL: Memory boundary check"
                  << std::endl;
        return 1;
    } catch (const std::out_of_range&) {
        std::cout << "PASS: Memory boundary check"
                  << std::endl;
    }

    // Test 3: Unknown instruction
    CPU invalid_cpu;
    invalid_cpu.write_memory(0, 99);

    try {
        invalid_cpu.run();

        std::cerr << "FAIL: Unknown instruction"
                  << std::endl;
        return 1;
    } catch (const std::runtime_error&) {
        std::cout << "PASS: Unknown instruction"
                  << std::endl;
    }

    // Test 4: Missing instruction parameter
    CPU incomplete_cpu;
    incomplete_cpu.write_memory(255, CPU::OP_LOAD);
    incomplete_cpu.program_counter = 255;

    try {
        incomplete_cpu.run();

        std::cerr << "FAIL: Missing parameter"
                  << std::endl;
        return 1;
    } catch (const std::out_of_range&) {
        std::cout << "PASS: Missing parameter"
                  << std::endl;
    }

    std::cout << "All tests passed." << std::endl;
    return 0;
}
