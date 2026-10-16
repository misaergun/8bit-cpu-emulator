
#ifndef PROGRAM_LOADER_H
#define PROGRAM_LOADER_H

#include <cstdint>
#include <stdexcept>
#include <vector>
#include "cpu.h"

inline void load_program(CPU &cpu, const std::vector<uint8_t> &program)
{
    if (program.size() > CPU::STACK_LIMIT)
    {
        throw std::out_of_range(
            "Program exceeds available memory reserved for code and data");
    }

    for (size_t i = 0; i < program.size(); ++i)
    {
        cpu.write_memory(
            static_cast<uint16_t>(i),
            program[i]);
    }
}

#endif
