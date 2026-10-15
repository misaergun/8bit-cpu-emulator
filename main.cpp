#include <iostream>
#include <vector>
#include <cstdint>
#include <limits>
#include <iomanip>
#include "cpu.h"
#include "program_loader.h"
#include "instruction_names.h"

std::vector<uint8_t> comparison_program(uint8_t first, uint8_t second)
{
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
        CPU::OP_HALT};
}

std::vector<uint8_t> addition_program(uint8_t first, uint8_t second)
{
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_ADD_B,
        CPU::OP_HALT};
}

std::vector<uint8_t> subtraction_program(uint8_t first, uint8_t second)
{
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_SUB_B,
        CPU::OP_HALT};
}

std::vector<uint8_t> and_program(uint8_t first, uint8_t second)
{
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_AND,
        CPU::OP_HALT};
}

std::vector<uint8_t> or_program(uint8_t first, uint8_t second)
{
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_OR,
        CPU::OP_HALT};
}

std::vector<uint8_t> xor_program(uint8_t first, uint8_t second)
{
    return {
        CPU::OP_LOAD, first,
        CPU::OP_LOAD_B, second,
        CPU::OP_XOR,
        CPU::OP_HALT};
}
std::vector<uint8_t> not_program(uint8_t value)
{
    return {
        CPU::OP_LOAD, value,
        CPU::OP_NOT,
        CPU::OP_HALT};
}

std::vector<uint8_t> shl_program(uint8_t value)
{
    return {
        CPU::OP_LOAD, value,
        CPU::OP_SHL,
        CPU::OP_HALT};
}

std::vector<uint8_t> shr_program(uint8_t value)
{
    return {
        CPU::OP_LOAD, value,
        CPU::OP_SHR,
        CPU::OP_HALT};
}

void print_cpu_state(const CPU &cpu)
{
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

void inspect_memory(const CPU &cpu, uint16_t address)
{
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

void dump_memory(const CPU &cpu, uint16_t start, uint16_t count)
{
    if (start >= CPU::MEMORY_SIZE ||
        count == 0 ||
        count > CPU::MEMORY_SIZE - start)
    {
        std::cerr << "Invalid memory range.\n";
        return;
    }

    std::cout << "\nMemory Dump\n";
    std::cout << "Address   Hex   Decimal\n";
    std::cout << "-----------------------\n";

    for (uint16_t i = 0; i < count; ++i)
    {
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

bool execute_program(CPU &cpu)
{
    char mode;

    while (true)
    {
        std::cout << "\nChoose execution mode:\n";
        std::cout << "s - Step mode\n";
        std::cout << "r - Run mode\n";
        std::cout << "Your choice: ";

        if (!(std::cin >> mode))
        {
            std::cerr << "Invalid input. Choose 's' or 'r'.\n";

            if (std::cin.eof())
            {
                return false;
            }

            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (mode == 's' || mode == 'S' ||
            mode == 'r' || mode == 'R')
        {
            break;
        }

        std::cerr << "Invalid mode. Choose 's' or 'r'.\n";
    }

    if (mode == 's' || mode == 'S')
    {
        while (!cpu.halted)
        {
            uint16_t instruction_address = cpu.program_counter;
            uint8_t instruction = cpu.read_memory(instruction_address);

            std::cout << "\nExecuting: "
                      << instruction_name(instruction)
                      << " (address " << instruction_address << ")\n";

            cpu.step();
            print_cpu_state(cpu);

            if (!cpu.halted)
            {
                std::cout << "Press Enter for next instruction...";

                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(), '\n');
                std::cin.get();

                if (!std::cin)
                {
                    return false;
                }
            }
        }
    }
    else
    {
        while (!cpu.halted)
        {
            uint16_t instruction_address = cpu.program_counter;
            uint8_t instruction = cpu.read_memory(instruction_address);

            std::cout << "Executing: "
                      << instruction_name(instruction)
                      << " (address " << instruction_address << ")\n";

            cpu.step();
            print_cpu_state(cpu);
        }
    }

    return true;
}

void inspect_memory_menu(const CPU &cpu)
{
    char inspect_choice;

    // Ask whether to inspect memory
    while (true)
    {
        std::cout << "\nInspect memory? (y/n): ";

        if (!(std::cin >> inspect_choice))
        {
            if (std::cin.eof())
            {
                return;
            }

            std::cerr << "Invalid input. Enter 'y' or 'n'.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (inspect_choice == 'y' || inspect_choice == 'Y' ||
            inspect_choice == 'n' || inspect_choice == 'N')
        {
            break;
        }

        std::cerr << "Invalid input. Enter 'y' or 'n'.\n";
    }

    if (inspect_choice == 'n' || inspect_choice == 'N')
    {
        return;
    }

    int address;

    // Ask for a valid memory address
    while (true)
    {
        std::cout << "Enter memory address (0-255): ";

        if (!(std::cin >> address))
        {
            std::cerr << "Invalid address. Enter a value from 0 to 255.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (address < 0 || address >= CPU::MEMORY_SIZE)
        {
            std::cerr << "Invalid address. Enter a value from 0 to 255.\n";
            continue;
        }

        break;
    }

    inspect_memory(cpu, static_cast<uint16_t>(address));

    char dump_choice;

    // Ask whether to dump a memory range
    while (true)
    {
        std::cout << "\nDump a memory range? (y/n): ";

        if (!(std::cin >> dump_choice))
        {
            if (std::cin.eof())
            {
                return;
            }

            std::cerr << "Invalid input. Enter 'y' or 'n'.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (dump_choice == 'y' || dump_choice == 'Y' ||
            dump_choice == 'n' || dump_choice == 'N')
        {
            break;
        }

        std::cerr << "Invalid input. Enter 'y' or 'n'.\n";
    }

    if (dump_choice == 'n' || dump_choice == 'N')
    {
        return;
    }

    int start;
    int count;

    // Ask for a valid starting address
    while (true)
    {
        std::cout << "Start address (0-255): ";

        if (!(std::cin >> start))
        {
            std::cerr << "Invalid start address. Enter a value from 0 to 255.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (start < 0 || start >= CPU::MEMORY_SIZE)
        {
            std::cerr << "Invalid start address. Enter a value from 0 to 255.\n";
            continue;
        }

        break;
    }

    // Ask for a valid byte count
    while (true)
    {
        std::cout << "Number of bytes (1-256): ";

        if (!(std::cin >> count))
        {
            std::cerr << "Invalid byte count. Enter a positive integer.\n";
            std::cin.clear();
            std::cin.ignore(
                std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (count < 1 || count > CPU::MEMORY_SIZE - start)
        {
            std::cerr << "Invalid byte count. The range must stay within memory.\n";
            continue;
        }

        break;
    }

    dump_memory(
        cpu,
        static_cast<uint16_t>(start),
        static_cast<uint16_t>(count));
}

int main()
{
    CPU cpu;

    while (true)
    {
        int program_choice;
        std::vector<uint8_t> program;

        // Program selection
        while (true)
        {
            std::cout << "\n=== 8-Bit CPU Emulator ===\n";
            std::cout << "Choose a program:\n";
            std::cout << "1 - Comparison\n";
            std::cout << "2 - Addition\n";
            std::cout << "3 - Subtraction\n";
            std::cout << "4 - Bitwise AND\n";
            std::cout << "5 - Bitwise OR\n";
            std::cout << "6 - Bitwise XOR\n";
            std::cout << "7 - Bitwise NOT\n";
            std::cout << "8 - Shift Left (SHL)\n";
            std::cout << "9 - Shift Right (SHR)\n";
            std::cout << "Your choice: ";

            if (!(std::cin >> program_choice))
            {
                std::cerr << "Invalid input. Enter 1, 2, or 3.\n";
                std::cin.clear();
                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }

            if (program_choice < 1 || program_choice > 9)
            {
                std::cerr << "Invalid program choice. Enter 1, 2, or 3.\n";
                continue;
            }

            break;
        }

        int first;
        int second;

        // First number
        while (true)
        {
            std::cout << "Enter the first number (0-255): ";

            if (!(std::cin >> first))
            {
                std::cerr << "Invalid number. Enter a value from 0 to 255.\n";
                std::cin.clear();
                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }

            if (first < 0 || first > 255)
            {
                std::cerr << "Invalid number. Enter a value from 0 to 255.\n";
                continue;
            }

            break;
        }

        // Second number
        while (true)
        {
            std::cout << "Enter the second number (0-255): ";

            if (!(std::cin >> second))
            {
                std::cerr << "Invalid number. Enter a value from 0 to 255.\n";
                std::cin.clear();
                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }

            if (second < 0 || second > 255)
            {
                std::cerr << "Invalid number. Enter a value from 0 to 255.\n";
                continue;
            }

            break;
        }

        uint8_t a = static_cast<uint8_t>(first);
        uint8_t b = static_cast<uint8_t>(second);

        // Select program
        switch (program_choice)
        {
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
        case 4:
            program = and_program(a, b);
            std::cout << "\nRunning Bitwise AND program...\n";
            break;

        case 5:
            program = or_program(a, b);
            std::cout << "\nRunning Bitwise OR program...\n";
            break;

        case 6:
            program = xor_program(a, b);
            std::cout << "\nRunning Bitwise XOR program...\n";
            break;
        case 7:
            program = not_program(a);
            std::cout << "\nRunning Bitwise NOT program...\n";
            break;

        case 8:
            program = shl_program(a);
            std::cout << "\nRunning Shift Left (SHL) program...\n";
            break;

        case 9:
            program = shr_program(a);
            std::cout << "\nRunning Shift Right (SHR) program...\n";
            break;
        }

        // Reset CPU and load program
        cpu.reset();
        load_program(cpu, program);

        if (!execute_program(cpu))
        {
            return 1;
        }

        // Optional memory inspection
        inspect_memory_menu(cpu);

        // Display final result
        std::cout << "\nFinal Register A: "
                  << static_cast<int>(cpu.register_a)
                  << '\n';

        if (program_choice == 1)
        {
            std::cout << "Comparison result: ";

            switch (cpu.register_a)
            {
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

        // Ask whether to run another program

        char choice;

        while (true)
        {
            std::cout << "\nRun another program? (y/n): ";

            if (!(std::cin >> choice))
            {
                if (std::cin.eof())
                {
                    std::cout << "\nExiting CPU emulator.\n";
                    return 0;
                }

                std::cerr << "Invalid input. Enter 'y' or 'n'.\n";

                std::cin.clear();
                std::cin.ignore(
                    std::numeric_limits<std::streamsize>::max(), '\n');

                continue;
            }

            if (choice == 'y' || choice == 'Y' ||
                choice == 'n' || choice == 'N')
            {
                break;
            }

            std::cerr << "Invalid input. Enter 'y' or 'n'.\n";
        }

        if (choice == 'n' || choice == 'N')
        {
            std::cout << "Exiting CPU emulator.\n";
            break;
        }

        cpu.reset();
        std::cout << "\nCPU reset. Select the next program.\n";
    }

    return 0;
}
