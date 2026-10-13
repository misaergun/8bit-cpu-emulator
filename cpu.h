
#ifndef CPU_H
#define CPU_H

#include <array>
#include <cstdint>
#include <stdexcept>

class CPU {
public:
    static constexpr uint16_t MEMORY_SIZE = 256;

    static constexpr uint8_t OP_LOAD = 1;
    static constexpr uint8_t OP_ADD = 2;
    static constexpr uint8_t OP_HALT = 255;

    uint8_t register_a = 0;
    uint16_t program_counter = 0;

    std::array<uint8_t, MEMORY_SIZE> memory{};

    bool halted = false;

    void add(uint8_t value) {
        register_a = static_cast<uint8_t>(
            register_a + value
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

            case OP_ADD: {
                uint8_t value = fetch();
                add(value);
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

    void run() {
        while (!halted) {
            step();
        }
    }
};

#endif
