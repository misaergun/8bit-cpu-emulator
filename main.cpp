
#include <iostream>
#include <vector>
#include "cpu.h"

void load_program(CPU& cpu, const std::vector<uint8_t>& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        cpu.write_memory(
            static_cast<uint16_t>(i),
            program[i]
        );
    }
}

int main() {
    CPU cpu;

    // Compare A = 8 and B = 5.
    // Result: 1 = A < B, 2 = A > B, 3 = A == B.
    const std::vector<uint8_t> program = {
        CPU::OP_LOAD, 8,       // Address 0-1
        CPU::OP_LOAD_B, 5,     // Address 2-3
        CPU::OP_CMP,           // Address 4
        CPU::OP_JL, 12,        // Address 5-6
        CPU::OP_JG, 16,        // Address 7-8
        CPU::OP_JE, 20,        // Address 9-10
        CPU::OP_HALT,          // Address 11 (fallback)
        CPU::OP_LOAD, 1,       // Address 12-13: A < B
        CPU::OP_HALT,          // Address 14
        0,                     // Address 15: unused
        CPU::OP_LOAD, 2,       // Address 16-17: A > B
        CPU::OP_HALT,          // Address 18
        0,                     // Address 19: unused
        CPU::OP_LOAD, 3,       // Address 20-21: A == B
        CPU::OP_HALT           // Address 22
    };

    load_program(cpu, program);
    cpu.run();

    std::cout << "Comparison result: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no")
              << std::endl;

    return 0;
}
