
#include <iostream>
#include <vector>
#include <limits>
#include <iomanip>
#include "cpu.h"

void load_program(CPU& cpu, const std::vector<uint8_t>& program) {
    for (size_t i = 0; i < program.size(); ++i) {
        cpu.write_memory(
            static_cast<uint16_t>(i),
            program[i]
        );
    }
}

std::vector<uint8_t> comparison_program(uint8_t first, uint8_t second) {
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
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
}



std::vector<uint8_t> addition_program(uint8_t first, uint8_t second) {
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_ADD_B,
        CPU::OP_HALT
    };
}

std::vector<uint8_t> subtraction_program(uint8_t first, uint8_t second) {
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_SUB_B,
        CPU::OP_HALT
    };
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
              << std::hex << std::uppercase
              << static_cast<int>(value)
              << std::dec
              << '\n';
}

void dump_memory(const CPU& cpu, uint16_t start, uint16_t count) {
    if (start >= CPU::MEMORY_SIZE ||
        count == 0 ||
        count > CPU::MEMORY_SIZE - start) {
        std::cerr << "Invalid memory range.\n";
        return;
    }

    std::cout << "\nMemory Dump\n";
    std::cout << "Address   Hex   Decimal\n";
    std::cout << "-----------------------\n";

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t address = start + i;
        uint8_t value = cpu.read_memory(address);

        std::cout << "0x"
                  << std::hex << std::uppercase
                  << std::setw(2) << std::setfill('0')
                  << address
                  << "      "
                  << std::setw(2)
                  << static_cast<int>(value)
                  << "    "
                  << std::dec
                  << std::setfill(' ')
                  << std::setw(3)
                  << static_cast<int>(value)
                  << '\n';
    }

    std::cout << std::dec << std::setfill(' ');
}

bool execute_program(CPU& cpu) {
    char mode;

    std::cout << "\nChoose execution mode:\n";
    std::cout << "s - Step mode\n";
    std::cout << "r - Run mode\n";
    std::cout << "Your choice: ";

    if (!(std::cin >> mode)) {
        std::cerr << "Invalid input.\n";
        return false;
    }

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

                if (!std::cin) {
                    return false;
                }
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
        return false;
    }

    return true;
}

void inspect_memory_menu(const CPU& cpu) {
    char inspect_choice;

    std::cout << "\nInspect memory? (y/n): ";
    if (!(std::cin >> inspect_choice)) {
        return;
    }

    if (inspect_choice != 'y' && inspect_choice != 'Y') {
        return;
    }

    uint16_t address;

    std::cout << "Enter memory address (0-255): ";
    if (!(std::cin >> address) || address >= CPU::MEMORY_SIZE) {
        std::cerr << "Invalid memory address.\n";
        std::cin.clear();
        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(), '\n'
        );
        return;
    }

    inspect_memory(cpu, address);

    char dump_choice;

    std::cout << "\nDump a memory range? (y/n): ";
    if (!(std::cin >> dump_choice)) {
        return;
    }

    if (dump_choice == 'y' || dump_choice == 'Y') {
        uint16_t start;
        uint16_t count;

        std::cout << "Start address (0-255): ";
        if (!(std::cin >> start) || start >= CPU::MEMORY_SIZE) {
            std::cerr << "Invalid start address.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n'
            );
            return;
        }

        std::cout << "Number of bytes: ";
        if (!(std::cin >> count)) {
            std::cerr << "Invalid byte count.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n'
            );
            return;
        }

        dump_memory(cpu, start, count);
    }
}


int main() {
    CPU cpu;

    while (true) {
        int program_choice;
        std::vector<uint8_t> program;

        std::cout << "\n=== 8-Bit CPU Emulator ===\n";
        std::cout << "Choose a program:\n";
        std::cout << "1 - Comparison\n";
        std::cout << "2 - Addition\n";
        std::cout << "3 - Subtraction\n";
        std::cout << "Your choice: ";

        if (!(std::cin >> program_choice)) {
            std::cerr << "Invalid input.\n";
            return 1;
        }

        if (program_choice < 1 || program_choice > 3) {
            std::cerr << "Invalid program choice.\n";
            continue;
        }

        uint16_t first;
        uint16_t second;

        std::cout << "Enter the first number (0-255): ";
        if (!(std::cin >> first) || first > 255) {
            std::cerr << "Invalid number. Enter a value from 0 to 255.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n'
            );
            continue;
        }

        std::cout << "Enter the second number (0-255): ";
        if (!(std::cin >> second) || second > 255) {
            std::cerr << "Invalid number. Enter a value from 0 to 255.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n'
            );
            continue;
        }

        uint8_t a = static_cast<uint8_t>(first);
        uint8_t b = static_cast<uint8_t>(second);

        switch (program_choice) {
            case 1:
                program = comparison_program(a, b);
                std::cout << "Comparison program selected.\n";
                break;

            case 2:
                program = addition_program(a, b);
                std::cout << "Addition program selected.\n";
                break;

            case 3:
                program = subtraction_program(a, b);
                std::cout << "Subtraction program selected.\n";
                break;
        }

        cpu.reset();
        load_program(cpu, program);

        if (!execute_program(cpu)) {
            return 1;
        }

        inspect_memory_menu(cpu);

        
        std::cout << "\nFinal Register A: "
                << static_cast<int>(cpu.register_a)
                << '\n';

        if (program_choice == 1) {
            std::cout << "Comparison result: ";

            switch (cpu.register_a) {
                case 1:
                    std::cout << "A < B\n";
                    break;

                case 2:
                    std::cout << "A > B\n";
                    break;

                case 3:
                    std::cout << "A == B\n";
                    break;

                default:
                    std::cout << "Unknown comparison result\n";
                    break;
            }
        }


        char choice;

        std::cout << "\nRun another program? (y/n): ";
        if (!(std::cin >> choice)) {
            break;
        }

        if (choice != 'y' && choice != 'Y') {
            std::cout << "Exiting CPU emulator.\n";
            break;
        }

        cpu.reset();
        std::cout << "\nCPU reset. Select the next program.\n";
    }

    return 0;
}

