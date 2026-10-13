
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

void print_cpu_state(const CPU& cpu) {
    std::cout << "Register A: "
              << static_cast<int>(cpu.register_a) << '\n';

    std::cout << "Register B: "
              << static_cast<int>(cpu.register_b) << '\n';

    std::cout << "Program Counter: "
              << cpu.program_counter << '\n';

    std::cout << "Zero flag: "
              << (cpu.zero_flag ? "true" : "false") << '\n';

    std::cout << "Less flag: "
              << (cpu.less_flag ? "true" : "false") << '\n';

    std::cout << "Greater flag: "
              << (cpu.greater_flag ? "true" : "false") << '\n';

    std::cout << "CPU halted: "
              << (cpu.halted ? "yes" : "no") << '\n';

    std::cout << "------------------------\n";
}

int main() {
    CPU cpu;

    const std::vector<uint8_t> program = {
        CPU::OP_LOAD, 8,
        CPU::OP_LOAD_B, 5,
        CPU::OP_CMP,
        CPU::OP_JL, 12,
        CPU::OP_JG, 16,
        CPU::OP_JE, 20,
        CPU::OP_HALT,
        CPU::OP_LOAD, 1,
        CPU::OP_HALT,
        0,
        CPU::OP_LOAD, 2,
        CPU::OP_HALT,
        0,
        CPU::OP_LOAD, 3,
        CPU::OP_HALT
    };

    load_program(cpu, program);

    while (!cpu.halted) {
        cpu.step();
        print_cpu_state(cpu);
    }

    std::cout << "Final comparison result: "
              << static_cast<int>(cpu.register_a)
              << '\n';

    return 0;
}
