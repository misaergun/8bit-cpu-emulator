
#ifndef INSTRUCTION_NAMES_H
#define INSTRUCTION_NAMES_H

#include <cstdint>
#include "cpu.h"

inline const char *instruction_name(uint8_t instruction)
{
    switch (instruction)
    {
    case CPU::OP_LOAD:
        return "LOAD";
    case CPU::OP_ADD:
        return "ADD";
    case CPU::OP_SUB:
        return "SUB";
    case CPU::OP_JMP:
        return "JMP";
    case CPU::OP_JZ:
        return "JZ";
    case CPU::OP_LOAD_MEM:
        return "LOAD_MEM";
    case CPU::OP_STORE:
        return "STORE";
    case CPU::OP_LOAD_B:
        return "LOAD_B";
    case CPU::OP_STORE_B:
        return "STORE_B";
    case CPU::OP_MOV_A_B:
        return "MOV_A_B";
    case CPU::OP_MOV_B_A:
        return "MOV_B_A";
    case CPU::OP_ADD_B:
        return "ADD_B";
    case CPU::OP_SUB_B:
        return "SUB_B";
    case CPU::OP_CMP:
        return "CMP";
    case CPU::OP_JE:
        return "JE";
    case CPU::OP_JL:
        return "JL";
    case CPU::OP_JG:
        return "JG";
    case CPU::OP_NOT:
        return "NOT";
    case CPU::OP_SHL:
        return "SHL";
    case CPU::OP_SHR:
        return "SHR";
    case CPU::OP_PUSH:
        return "PUSH";
    case CPU::OP_POP:
        return "POP";
    case CPU::OP_CALL:
        return "CALL";
    case CPU::OP_RET:
        return "RET";
    case CPU::OP_HALT:
        return "HALT";
    case CPU::OP_AND:
        return "AND";
    case CPU::OP_OR:
        return "OR";
    case CPU::OP_XOR:
        return "XOR";
    default:
        return "UNKNOWN";
    }
}

#endif
