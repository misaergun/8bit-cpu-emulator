
#ifndef CPU_H
#define CPU_H

#include <array>
#include <cstdint>

class CPU {
public:
    static constexpr uint16_t MEMORY_SIZE = 256;

    uint8_t register_a = 0;
    uint8_t program_counter = 0;

    std::array<uint8_t, MEMORY_SIZE> memory{};

    void add(uint8_t value) {
        register_a = static_cast<uint8_t>(
            register_a + value
        );
    }

    void write_memory(uint8_t address, uint8_t value) {
        memory[address] = value;
    }

    uint8_t read_memory(uint8_t address) const {
        return memory[address];
    }

    void advance_program_counter() {
        program_counter =
            static_cast<uint8_t>(program_counter + 1);
    }
};

#endif
