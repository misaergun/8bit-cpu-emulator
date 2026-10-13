
#include <iostream>
#include <stdexcept>
#include "cpu.h"

int main() {
    // Test 1: Normal program
    CPU cpu;
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

    // Test 2: Memory boundary
    try {
        cpu.read_memory(256);
        std::cerr << "FAIL: Memory boundary check" << std::endl;
        return 1;
    } catch (const std::out_of_range&) {
        std::cout << "PASS: Memory boundary check" << std::endl;
    }

    // Test 3: Unknown instruction
    CPU invalid_cpu;
    invalid_cpu.write_memory(0, 99);

    try {
        invalid_cpu.run();
        std::cerr << "FAIL: Unknown instruction" << std::endl;
        return 1;
    } catch (const std::runtime_error&) {
        std::cout << "PASS: Unknown instruction" << std::endl;
    }

    // Test 4: Missing parameter at the end of memory
    CPU incomplete_cpu;
    incomplete_cpu.write_memory(255, CPU::OP_LOAD);
    incomplete_cpu.program_counter = 255;

    try {
        incomplete_cpu.run();
        std::cerr << "FAIL: Missing parameter" << std::endl;
        return 1;
    } catch (const std::out_of_range&) {
        std::cout << "PASS: Missing parameter" << std::endl;
    }

    // Test 5: Unconditional jump
    CPU jump_cpu;
    jump_cpu.write_memory(0, CPU::OP_JMP);
    jump_cpu.write_memory(1, 4);
    jump_cpu.write_memory(2, CPU::OP_LOAD);
    jump_cpu.write_memory(3, 99);
    jump_cpu.write_memory(4, CPU::OP_HALT);

    jump_cpu.run();

    if (jump_cpu.register_a != 0 || !jump_cpu.halted) {
        std::cerr << "FAIL: Unconditional jump" << std::endl;
        return 1;
    }
    std::cout << "PASS: Unconditional jump" << std::endl;

    std::cout << "All tests passed." << std::endl;
    return 0;
}
