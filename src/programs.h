#ifndef PROGRAMS_H
#define PROGRAMS_H

// Program catalog used by the web server.
// The byte sequences match the programs built in main.cpp.

#include <cstdint>
#include <string>
#include <vector>
#include "cpu.h"

struct ProgramInfo
{
    const char *id;
    const char *name;
    bool needs_second_input;
};

inline const std::vector<ProgramInfo> &program_catalog()
{
    static const std::vector<ProgramInfo> catalog = {
        {"comparison", "Comparison", true},
        {"addition", "Addition", true},
        {"subtraction", "Subtraction", true},
        {"and", "Bitwise AND", true},
        {"or", "Bitwise OR", true},
        {"xor", "Bitwise XOR", true},
        {"not", "Bitwise NOT", false},
        {"shl", "Shift Left (SHL)", false},
        {"shr", "Shift Right (SHR)", false},
    };
    return catalog;
}

inline const ProgramInfo *find_program(const std::string &id)
{
    for (const ProgramInfo &info : program_catalog())
    {
        if (id == info.id)
        {
            return &info;
        }
    }
    return nullptr;
}

// Returns the program bytes, or an empty vector for an unknown id.
inline std::vector<uint8_t> build_program(
    const std::string &id, uint8_t first, uint8_t second)
{
    if (id == "comparison")
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

    uint8_t binary_opcode = 0;
    if (id == "addition")
        binary_opcode = CPU::OP_ADD_B;
    else if (id == "subtraction")
        binary_opcode = CPU::OP_SUB_B;
    else if (id == "and")
        binary_opcode = CPU::OP_AND;
    else if (id == "or")
        binary_opcode = CPU::OP_OR;
    else if (id == "xor")
        binary_opcode = CPU::OP_XOR;

    if (binary_opcode != 0)
    {
        return {
            CPU::OP_LOAD, first,
            CPU::OP_LOAD_B, second,
            binary_opcode,
            CPU::OP_HALT};
    }

    uint8_t unary_opcode = 0;
    if (id == "not")
        unary_opcode = CPU::OP_NOT;
    else if (id == "shl")
        unary_opcode = CPU::OP_SHL;
    else if (id == "shr")
        unary_opcode = CPU::OP_SHR;

    if (unary_opcode != 0)
    {
        return {
            CPU::OP_LOAD, first,
            unary_opcode,
            CPU::OP_HALT};
    }

    return {};
}

// Same wording as the terminal application's comparison output.
inline const char *comparison_result_text(uint8_t register_a)
{
    switch (register_a)
    {
    case 1:
        return "A < B";
    case 2:
        return "A > B";
    case 3:
        return "A == B";
    default:
        return "Unknown comparison result";
    }
}

#endif
