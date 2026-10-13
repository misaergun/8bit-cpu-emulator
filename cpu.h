
#ifndef CPU_H
#define CPU_H

#include <cstdint>

class CPU {
public:
    uint8_t register_a = 0;

    void add(uint8_t value) {
        register_a = static_cast<uint8_t>(
            register_a + value
        );
    }
};

#endif
