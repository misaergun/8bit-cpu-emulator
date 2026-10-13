
#include <iostream>
#include <vector>
#include <limits>
#include "cpu.h"

void load_program(CPU& cpu, const std::vector<uint8_t>& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        cpu.write_memory(
            static_cast<uint16_t>(i),
            program[i]
        );
    }
}

const char* instruction_name(uint8_t instruction) {
    switch (instruction) {
        case CPU::OP_LOAD: return "LOAD";
        case CPU::OP_ADD: return "ADD";
        case CPU::OP_SUB: return "SUB";
        case CPU::OP_JMP: return "JMP";
        case CPU::OP_JZ: return "JZ";
        case CPU::OP_LOAD_MEM: return "LOAD_MEM";
        case CPU::OP_STORE: return "STORE";
        case CPU::OP_LOAD_B: return "LOAD_B";
        case CPU::OP_STORE_B: return "STORE_B";
        case CPU::OP_MOV_A_B: return "MOV_A_B";
        case CPU::OP_MOV_B_A: return "MOV_B_A";
        case CPU::OP_ADD_B: return "ADD_B";
        case CPU::OP_SUB_B: return "SUB_B";
        case CPU::OP_CMP: return "CMP";
        case CPU::OP_JE: return "JE";
        case CPU::OP_JL: return "JL";
        case CPU::OP_JG: return "JG";
        case CPU::OP_HALT: return "HALT";
        default: return "UNKNOWN";
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

void inspect_memory(const CPU& cpu, uint16_t address) {
    uint8_t value = cpu.read_memory(address);

    std::cout << "Memory address: " << address << '\n';

    std::cout << "Value (decimal): "
              << static_cast<int>(value) << '\n';

    std::cout << "Value (hex): 0x"
              << std::hex
              << static_cast<int>(value)
              << std::dec
              << '\n';
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

    char mode;
    std::cout << "Choose execution mode:\n";
    std::cout << "s - Step mode\n";
    std::cout << "r - Run mode\n";
    std::cout << "Your choice: ";
    std::cin >> mode;

    if (mode == 's' || mode == 'S') {
        while (!cpu.halted) {
            uint16_t instruction_address = cpu.program_counter;
            uint8_t instruction = cpu.read_memory(instruction_address);

            std::cout << "\nExecuting: "
                      << instruction_name(instruction)
                      << " (address " << instruction_address << ")\n";

            cpu.step();
            print_cpu_state(cpu);

            if (!cpu.halted) {
                std::cout << "Press Enter for next instruction...";
                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(), '\n'
                );
                std::cin.get();
            }
        }
    } else if (mode == 'r' || mode == 'R') {
        while (!cpu.halted) {
            uint16_t instruction_address = cpu.program_counter;
            uint8_t instruction = cpu.read_memory(instruction_address);

            std::cout << "Executing: "
                      << instruction_name(instruction)
                      << " (address " << instruction_address << ")\n";

            cpu.step();
            print_cpu_state(cpu);
        }
    } else {
        std::cerr << "Invalid mode. Choose 's' or 'r'.\n";
        return 1;
    }


    char inspect_choice;

    std::cout << "\nInspect memory? (y/n): ";
    std::cin >> inspect_choice;

    if (inspect_choice == 'y' || inspect_choice == 'Y') {
        uint16_t address;

        std::cout << "Enter memory address (0-255): ";
        std::cin >> address;

        if (address >= CPU::MEMORY_SIZE) {
            std::cerr << "Invalid memory address.\n";
            return 1;
        }

        inspect_memory(cpu, address);
    }


    std::cout << "Final comparison result: "
              << static_cast<int>(cpu.register_a)
              << '\n';

    return 0;




    std::cout << "Final comparison result: "
              << static_cast<int>(cpu.register_a)
              << '\n';

    return 0;
}
