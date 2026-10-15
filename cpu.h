#ifndef CPU_H
#define CPU_H
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class CPU
{
public:
    static constexpr uint16_t MEMORY_SIZE = 256;
    static constexpr uint16_t STACK_START = MEMORY_SIZE;
    static constexpr uint16_t STACK_LIMIT = 192;
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
    static constexpr uint8_t OP_JE = 15;
    static constexpr uint8_t OP_JL = 16;
    static constexpr uint8_t OP_JG = 17;
    static constexpr uint8_t OP_AND = 18;
    static constexpr uint8_t OP_OR = 19;
    static constexpr uint8_t OP_XOR = 20;
    static constexpr uint8_t OP_NOT = 21;
    static constexpr uint8_t OP_SHL = 22;
    static constexpr uint8_t OP_SHR = 23;
    static constexpr uint8_t OP_PUSH = 24;
    static constexpr uint8_t OP_POP = 25;
    static constexpr uint8_t OP_CALL = 26;
    static constexpr uint8_t OP_RET = 27;
    static constexpr uint8_t OP_HALT = 255;

    uint8_t register_a = 0;
    uint8_t register_b = 0;
    uint16_t program_counter = 0;
    uint16_t stack_pointer = STACK_START;
    uint16_t stack_size = 0;
    std::vector<uint16_t> return_addresses;
    std::array<uint8_t, MEMORY_SIZE> memory{};

    bool halted = false;
    bool zero_flag = false;
    bool less_flag = false;
    bool greater_flag = false;

    void add(uint8_t value)
    {
        register_a = static_cast<uint8_t>(
            register_a + value);
    }

    void subtract(uint8_t value)
    {
        register_a = static_cast<uint8_t>(
            register_a - value);
    }

    void write_memory(uint16_t address, uint8_t value)
    {
        if (address >= MEMORY_SIZE)
        {
            throw std::out_of_range(
                "Memory write address out of range");
        }
        memory[address] = value;
    }

    void write_stack_memory(uint16_t address, uint8_t value)
    {

        if (address < STACK_LIMIT || address >= MEMORY_SIZE)
        {

            throw std::out_of_range(

                "Stack write address out of range"

            );
        }

        memory[address] = value;
    }

    uint8_t read_memory(uint16_t address) const
    {

        if (address >= MEMORY_SIZE)
        {

            throw std::out_of_range(

                "Memory read address out of range"

            );
        }

        return memory[address];
    }

    uint8_t fetch()
    {

        uint8_t instruction = read_memory(program_counter);

        ++program_counter;

        return instruction;
    }

    void step()
    {

        if (halted)
        {

            return;
        }

        uint8_t instruction = fetch();

        switch (instruction)
        {

        case OP_LOAD:
        {

            uint8_t value = fetch();

            register_a = value;

            break;
        }

        case OP_LOAD_B:
        {
            uint8_t value = fetch();
            register_b = value;
            break;
        }

        case OP_STORE_B:
        {
            uint8_t address = fetch();
            if (address >= STACK_LIMIT)
            {
                throw std::runtime_error(
                    "Cannot write to stack memory using STORE_B");
            }

            write_memory(address, register_b);
            break;
        }

        case OP_ADD:
        {
            uint8_t value = fetch();
            add(value);
            break;
        }

        case OP_ADD_B:
        {
            add(register_b);
            break;
        }

        case OP_SUB:
        {
            uint8_t value = fetch();
            subtract(value);
            break;
        }

        case OP_SUB_B:
        {
            subtract(register_b);
            break;
        }

        case OP_AND:
        {
            register_a = register_a & register_b;
            zero_flag = (register_a == 0);
            break;
        }

        case OP_OR:
        {
            register_a = register_a | register_b;
            zero_flag = (register_a == 0);
            break;
        }

        case OP_XOR:
        {
            register_a = register_a ^ register_b;
            zero_flag = (register_a == 0);
            break;
        }

        case OP_NOT:
        {
            register_a = static_cast<uint8_t>(~register_a);
            zero_flag = (register_a == 0);
            break;
        }

        case OP_SHL:
        {
            register_a = static_cast<uint8_t>(register_a << 1);
            zero_flag = (register_a == 0);
            break;
        }

        case OP_SHR:
        {
            register_a = static_cast<uint8_t>(register_a >> 1);
            zero_flag = (register_a == 0);
            break;
        }

        case OP_PUSH:
        {
            if (stack_pointer <= STACK_LIMIT ||
            stack_size >= MEMORY_SIZE - STACK_LIMIT) {
            throw std::runtime_error("Stack overflow");
        }

            --stack_pointer;
            write_stack_memory(stack_pointer, register_a);
            ++stack_size;
            break;
        }

        case OP_POP:
        {
            if (stack_size == 0 ||
                stack_pointer >= MEMORY_SIZE ||
                (!return_addresses.empty() &&
                stack_pointer >= return_addresses.back() &&
                stack_pointer < return_addresses.back() + 2)) {
                throw std::runtime_error("Stack underflow");
            }

            register_a = read_memory(stack_pointer);
            ++stack_pointer;
            --stack_size;
            zero_flag = (register_a == 0);

            break;
        }

        case OP_CALL:
        {
            uint8_t address = fetch();
            if (stack_pointer < STACK_LIMIT + 2 ||
            stack_size > (MEMORY_SIZE - STACK_LIMIT) - 2) {
            throw std::runtime_error("Stack overflow");
        }

            // Save the return address on the stack.
                stack_pointer -= 2;
                stack_size += 2;

                return_addresses.push_back(stack_pointer);

            write_stack_memory(
                stack_pointer,
                static_cast<uint8_t>(program_counter & 0xFF)
            );

            write_stack_memory(
                stack_pointer + 1,
                static_cast<uint8_t>((program_counter >> 8) & 0xFF)
            );

            program_counter = address;
            break;
        }

        case OP_RET:
        {

            if (stack_size < 2 || stack_pointer > MEMORY_SIZE - 2) {
                throw std::runtime_error("Stack underflow");
        }
            if (return_addresses.empty() ||
                return_addresses.back() != stack_pointer) {
                throw std::runtime_error("Stack underflow");
            }

            uint16_t low_byte = read_memory(stack_pointer);
            uint16_t high_byte = read_memory(stack_pointer + 1);

            program_counter = static_cast<uint16_t>(
                low_byte | (high_byte << 8)
            );

            return_addresses.pop_back();
            stack_pointer += 2;
            stack_size -= 2;

            break;
        }

        case OP_JMP:
        {
            uint8_t address = fetch();
            program_counter = address;
            break;
        }

        case OP_JZ:
        {
            uint8_t address = fetch();
            if (register_a == 0)
            {
                program_counter = address;
            }
            break;
        }

        case OP_LOAD_MEM:
        {
            uint8_t address = fetch();
            register_a = read_memory(address);
            break;
        }

        case OP_STORE:
        {
            uint8_t address = fetch();
            if (address >= STACK_LIMIT)
            {
                throw std::runtime_error(
                    "Cannot write to stack memory using STORE");
            }
            write_memory(address, register_a);
            break;
        }

        case OP_MOV_A_B:
        {
            register_a = register_b;
            break;
        }

        case OP_MOV_B_A:
        {
            register_b = register_a;
            break;
        }

        case OP_CMP:
        {
            zero_flag = (register_a == register_b);
            less_flag = (register_a < register_b);
            greater_flag = (register_a > register_b);
            break;
        }

        case OP_JE:
        {
            uint8_t address = fetch();
            if (zero_flag)
            {
                program_counter = address;
            }
            break;
        }

        case OP_JL:
        {
            uint8_t address = fetch();
            if (less_flag)
            {
                program_counter = address;
            }
            break;
        }

        case OP_JG:
        {
            uint8_t address = fetch();
            if (greater_flag)
            {
                program_counter = address;
            }
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

    void reset()
    {
        register_a = 0;
        register_b = 0;
        program_counter = 0;
        stack_pointer = STACK_START;
        stack_size = 0;
        return_addresses.clear();
        halted = false;
        zero_flag = false;
        less_flag = false;
        greater_flag = false;
        memory.fill(0);
    }

    void run()
    {
        constexpr uint32_t MAX_STEPS = 1000;
        uint32_t steps = 0;
        while (!halted)
        {
            if (steps >= MAX_STEPS)
            {
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
