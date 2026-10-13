
#ifndef CPU_H
#define CPU_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

class CPU {
public:
    static constexpr uint16_t MEMORY_SIZE = 256;

    static constexpr uint8_t OP_LOAD = 1;
    static constexpr uint8_t OP_ADD = 2;
    static constexpr uint8_t OP_SUB = 3;
    static constexpr uint8_t OP_JMP = 4;
    static constexpr uint8_t OP_JZ = 5;
    static constexpr uint8_t OP_LOAD_MEM = 6;
    static constexpr uint8_t OP_STORE = 7;
    static constexpr uint8_t OP_LOAD_B = 8;
    static constexpr uint8_t OP_STORE_B = 9;
    static constexpr uint8_t OP_MOV_A_B = 10;
    static constexpr uint8_t OP_MOV_B_A = 11;
    static constexpr uint8_t OP_ADD_B = 12;
    static constexpr uint8_t OP_SUB_B = 13;
    static constexpr uint8_t OP_CMP = 14;
    static constexpr uint8_t OP_HALT = 255;

    uint8_t register_a = 0;
    uint8_t register_b = 0;
    uint16_t program_counter = 0;

    std::array<uint8_t, MEMORY_SIZE> memory{};

    bool halted = false;
    bool zero_flag = false;

    void add(uint8_t value) {
        register_a = static_cast<uint8_t>(
            register_a + value
        );
    }

    void subtract(uint8_t value) {
        register_a = static_cast<uint8_t>(
            register_a - value
        );
    }

    void write_memory(uint16_t address, uint8_t value) {
        if (address >= MEMORY_SIZE) {
            throw std::out_of_range(
                "Memory write address out of range"
            );
        }

        memory[address] = value;
    }

    uint8_t read_memory(uint16_t address) const {
        if (address >= MEMORY_SIZE) {
            throw std::out_of_range(
                "Memory read address out of range"
            );
        }

        return memory[address];
    }

    uint8_t fetch() {
        uint8_t instruction = read_memory(program_counter);
        ++program_counter;
        return instruction;
    }

    void step() {
        if (halted) {
            return;
        }

        uint8_t instruction = fetch();

        switch (instruction) {
            case OP_LOAD: {
                uint8_t value = fetch();
                register_a = value;
                break;
            }

            case OP_LOAD_B: {
                uint8_t value = fetch();
                register_b = value;
                break;
            }

            case OP_STORE_B: {
                uint8_t address = fetch();
                write_memory(address, register_b);
                break;
            }

            case OP_ADD: {
                uint8_t value = fetch();
                add(value);
                break;
            }

            case OP_ADD_B: {
                add(register_b);
                break;
            }


            case OP_SUB: {
                uint8_t value = fetch();
                subtract(value);
                break;
            }

            case OP_SUB_B: {
                subtract(register_b);
                break;
            }


            case OP_JMP: {
                uint8_t address = fetch();
                program_counter = address;
                break;
            }

            case OP_JZ: {
                uint8_t address = fetch();

                if (register_a == 0) {
                    program_counter = address;
                }

                break;
            }

            case OP_LOAD_MEM: {
                uint8_t address = fetch();
                register_a = read_memory(address);
                break;
            }

            case OP_STORE: {
                uint8_t address = fetch();
                write_memory(address, register_a);
                break;
            }

            case OP_MOV_A_B: {
                register_a = register_b;
                break;
            }

            case OP_MOV_B_A: {
                register_b = register_a;
                break;
            }

            case OP_CMP: {
                zero_flag = (register_a == register_b);
                break;
            }

            case OP_HALT:
                halted = true;
                break;

            default:
                throw std::runtime_error(
                    "Unknown instruction: " +
                    std::to_string(instruction)
                );
        }
    }


    void reset() {
        register_a = 0;
        register_b = 0;
        program_counter = 0;
        halted = false;
        zero_flag = false;
        memory.fill(0);
    }


    
void run() {
    constexpr uint32_t MAX_STEPS = 1000;
    uint32_t steps = 0;

    while (!halted) {
        if (steps >= MAX_STEPS) {
            throw std::runtime_error(
                "Maximum instruction limit reached"
            );
        }

        step();
        ++steps;
    }
}

};

#endif
