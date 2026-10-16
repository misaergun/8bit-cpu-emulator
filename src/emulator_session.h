#ifndef EMULATOR_SESSION_H
#define EMULATOR_SESSION_H

// CPU session shared by the HTTP server (web_server.cpp) and the
// WebAssembly build (wasm_api.cpp). It wraps the CPU from cpu.h, records an
// instruction trace, and reports the state as JSON for the web interface.
// It has no platform dependencies.

#include <cstdint>
#include <cstdio>
#include <deque>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpu.h"
#include "instruction_names.h"
#include "program_loader.h"
#include "programs.h"

constexpr uint32_t MAX_RUN_STEPS = 1000;
constexpr size_t MAX_TRACE_ENTRIES = 2000;

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------


inline std::string hex_byte(unsigned value)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02X", value & 0xFF);
    return buffer;
}

inline std::string json_escape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 2);
    out += '"';
    for (unsigned char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20)
            {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04X", c);
                out += buffer;
            }
            else
            {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
    return out;
}

inline const char *json_bool(bool value)
{
    return value ? "true" : "false";
}


struct CpuSnapshot
{
    uint8_t a = 0;
    uint8_t b = 0;
    uint16_t pc = 0;
    uint16_t sp = 0;
    bool zero = false;
    bool less = false;
    bool greater = false;
};

struct TraceEntry
{
    uint64_t index = 0;
    uint16_t address = 0;
    uint8_t opcode = 0;
    bool has_operand = false;
    uint8_t operand = 0;
    CpuSnapshot before;
    CpuSnapshot after;
};

inline bool instruction_has_operand(uint8_t opcode)
{
    switch (opcode)
    {
    case CPU::OP_LOAD:
    case CPU::OP_ADD:
    case CPU::OP_SUB:
    case CPU::OP_JMP:
    case CPU::OP_JZ:
    case CPU::OP_LOAD_MEM:
    case CPU::OP_STORE:
    case CPU::OP_LOAD_B:
    case CPU::OP_STORE_B:
    case CPU::OP_JE:
    case CPU::OP_JL:
    case CPU::OP_JG:
    case CPU::OP_CALL:
        return true;
    default:
        return false;
    }
}

inline void write_snapshot_json(std::ostream &out, const CpuSnapshot &s)
{
    out << "{\"a\":" << static_cast<int>(s.a)
        << ",\"b\":" << static_cast<int>(s.b)
        << ",\"pc\":" << s.pc
        << ",\"sp\":" << s.sp
        << ",\"zero\":" << json_bool(s.zero)
        << ",\"less\":" << json_bool(s.less)
        << ",\"greater\":" << json_bool(s.greater) << '}';
}

class EmulatorSession
{
public:
    void load(const ProgramInfo &program, uint8_t a, uint8_t b)
    {
        load_bytes(program, build_program(program.id, a, b), a, b);
    }

    // Loads raw program bytes. Used by load() and by the session unit tests.
    void load_bytes(const ProgramInfo &program,
                    const std::vector<uint8_t> &bytes, uint8_t a, uint8_t b)
    {
        if (bytes.empty())
        {
            throw std::invalid_argument("Program has no code");
        }

        cpu_.reset();
        load_program(cpu_, bytes);

        program_ = &program;
        input_a_ = a;
        input_b_ = b;
        program_bytes_ = bytes;
        program_size_ = bytes.size();
        steps_ = 0;
        trace_.clear();
        error_.clear();
    }

    // Restarts the loaded program, or clears the CPU if none is loaded.
    void reset()
    {
        if (program_ != nullptr)
        {
            std::vector<uint8_t> bytes = program_bytes_;
            load_bytes(*program_, bytes, input_a_, input_b_);
            return;
        }

        cpu_.reset();
        program_bytes_.clear();
        program_size_ = 0;
        steps_ = 0;
        trace_.clear();
        error_.clear();
    }

    bool has_program() const { return program_ != nullptr; }
    bool has_error() const { return !error_.empty(); }
    bool halted() const { return cpu_.halted; }
    uint8_t memory_at(uint16_t address) const { return cpu_.read_memory(address); }
    const std::string &error() const { return error_; }

    std::string status() const
    {
        if (program_ == nullptr)
            return "IDLE";
        if (!error_.empty())
            return "ERROR";
        if (cpu_.halted)
            return "HALTED";
        if (steps_ == 0)
            return "READY";
        return "RUNNING";
    }

    // Executes one instruction. Returns false if the CPU raised a fault.
    bool execute_one()
    {
        TraceEntry entry;
        entry.before = snapshot();
        entry.address = cpu_.program_counter;

        if (entry.address < CPU::MEMORY_SIZE)
        {
            entry.opcode = cpu_.memory[entry.address];
        }
        if (instruction_has_operand(entry.opcode) &&
            entry.address + 1 < CPU::STACK_LIMIT)
        {
            entry.has_operand = true;
            entry.operand = cpu_.memory[entry.address + 1];
        }

        try
        {
            cpu_.step();
        }
        catch (const std::exception &e)
        {
            error_ = "CPU fault at address 0x" + hex_byte(entry.address) +
                     ": " + e.what();
            return false;
        }

        entry.after = snapshot();
        entry.index = ++steps_;
        trace_.push_back(entry);
        if (trace_.size() > MAX_TRACE_ENTRIES)
        {
            trace_.pop_front();
        }
        return true;
    }

    // Runs until HALT. Returns false on a fault or when the step limit is hit.
    bool run_to_halt()
    {
        uint32_t executed = 0;
        while (!cpu_.halted)
        {
            if (executed >= MAX_RUN_STEPS)
            {
                error_ = "Instruction limit reached (" +
                         std::to_string(MAX_RUN_STEPS) +
                         " instructions) without HALT. Execution stopped.";
                return false;
            }
            if (!execute_one())
            {
                return false;
            }
            ++executed;
        }
        return true;
    }

    void write_json(std::ostream &out) const
    {
        out << '{';

        out << "\"program\":";
        if (program_ != nullptr)
        {
            out << "{\"id\":" << json_escape(program_->id)
                << ",\"name\":" << json_escape(program_->name)
                << ",\"needs_b\":" << json_bool(program_->needs_second_input)
                << ",\"a\":" << static_cast<int>(input_a_)
                << ",\"b\":";
            if (program_->needs_second_input)
                out << static_cast<int>(input_b_);
            else
                out << "null";
            out << ",\"size\":" << program_size_ << '}';
        }
        else
        {
            out << "null";
        }

        out << ",\"status\":" << json_escape(status());
        out << ",\"error\":";
        if (error_.empty())
            out << "null";
        else
            out << json_escape(error_);

        out << ",\"registers\":{\"a\":" << static_cast<int>(cpu_.register_a)
            << ",\"b\":" << static_cast<int>(cpu_.register_b)
            << ",\"pc\":" << cpu_.program_counter
            << ",\"sp\":" << cpu_.stack_pointer
            << ",\"stack_size\":" << cpu_.stack_size << '}';

        out << ",\"flags\":{\"zero\":" << json_bool(cpu_.zero_flag)
            << ",\"less\":" << json_bool(cpu_.less_flag)
            << ",\"greater\":" << json_bool(cpu_.greater_flag) << '}';

        out << ",\"halted\":" << json_bool(cpu_.halted);

        out << ",\"layout\":{\"memory_size\":" << CPU::MEMORY_SIZE
            << ",\"code_size\":" << program_size_
            << ",\"stack_limit\":" << CPU::STACK_LIMIT << '}';

        out << ",\"memory\":[";
        for (size_t i = 0; i < cpu_.memory.size(); ++i)
        {
            if (i > 0)
                out << ',';
            out << static_cast<int>(cpu_.memory[i]);
        }
        out << ']';

        out << ",\"next_instruction\":";
        if (program_ != nullptr && !cpu_.halted && error_.empty() &&
            cpu_.program_counter < CPU::STACK_LIMIT)
        {
            uint8_t opcode = cpu_.memory[cpu_.program_counter];
            out << "{\"address\":" << cpu_.program_counter
                << ",\"opcode\":" << static_cast<int>(opcode)
                << ",\"name\":" << json_escape(instruction_name(opcode))
                << ",\"operand\":";
            if (instruction_has_operand(opcode) &&
                cpu_.program_counter + 1 < CPU::STACK_LIMIT)
                out << static_cast<int>(cpu_.memory[cpu_.program_counter + 1]);
            else
                out << "null";
            out << '}';
        }
        else
        {
            out << "null";
        }

        out << ",\"result\":";
        if (program_ != nullptr && cpu_.halted && error_.empty())
        {
            std::string text;
            if (std::string(program_->id) == "comparison")
            {
                text = comparison_result_text(cpu_.register_a);
            }
            else
            {
                text = "Register A = " +
                       std::to_string(static_cast<int>(cpu_.register_a)) +
                       " (0x" + hex_byte(cpu_.register_a) + ")";
            }
            out << "{\"register_a\":" << static_cast<int>(cpu_.register_a)
                << ",\"text\":" << json_escape(text) << '}';
        }
        else
        {
            out << "null";
        }

        out << ",\"steps\":" << steps_;
        out << ",\"max_run_steps\":" << MAX_RUN_STEPS;

        out << ",\"trace\":[";
        bool first = true;
        for (const TraceEntry &entry : trace_)
        {
            if (!first)
                out << ',';
            first = false;

            out << "{\"index\":" << entry.index
                << ",\"address\":" << entry.address
                << ",\"opcode\":" << static_cast<int>(entry.opcode)
                << ",\"name\":" << json_escape(instruction_name(entry.opcode))
                << ",\"operand\":";
            if (entry.has_operand)
                out << static_cast<int>(entry.operand);
            else
                out << "null";
            out << ",\"before\":";
            write_snapshot_json(out, entry.before);
            out << ",\"after\":";
            write_snapshot_json(out, entry.after);
            out << '}';
        }
        out << "]}";
    }

private:
    CPU cpu_;
    const ProgramInfo *program_ = nullptr;
    uint8_t input_a_ = 0;
    uint8_t input_b_ = 0;
    std::vector<uint8_t> program_bytes_;
    size_t program_size_ = 0;
    uint64_t steps_ = 0;
    std::deque<TraceEntry> trace_;
    std::string error_;

    CpuSnapshot snapshot() const
    {
        CpuSnapshot s;
        s.a = cpu_.register_a;
        s.b = cpu_.register_b;
        s.pc = cpu_.program_counter;
        s.sp = cpu_.stack_pointer;
        s.zero = cpu_.zero_flag;
        s.less = cpu_.less_flag;
        s.greater = cpu_.greater_flag;
        return s;
    }
};

// ---------------------------------------------------------------------------
// Responses shared by the HTTP API and the WebAssembly API
// ---------------------------------------------------------------------------

// Returns why the session cannot execute an instruction, or nullptr if it can.
inline const char *execution_blocker(const EmulatorSession &session)
{
    if (!session.has_program())
        return "No program loaded. Select a program first.";
    if (session.has_error())
        return "CPU stopped with an error. Reset to try again.";
    if (session.halted())
        return "CPU is halted. Reset to run the program again.";
    return nullptr;
}

// {"ok":true,"state":{...}}
inline std::string state_envelope(const EmulatorSession &session)
{
    std::ostringstream out;
    out << "{\"ok\":true,\"state\":";
    session.write_json(out);
    out << '}';
    return out.str();
}

// {"ok":false,"error":"..."} with the current state when one is given.
inline std::string error_envelope(const std::string &message,
                                  const EmulatorSession *session = nullptr)
{
    std::ostringstream out;
    out << "{\"ok\":false,\"error\":" << json_escape(message);
    if (session != nullptr)
    {
        out << ",\"state\":";
        session->write_json(out);
    }
    out << '}';
    return out.str();
}

// {"ok":true,"programs":[{"id":...,"name":...,"needs_b":...}, ...]}
inline std::string programs_envelope()
{
    std::ostringstream out;
    out << "{\"ok\":true,\"programs\":[";
    bool first = true;
    for (const ProgramInfo &info : program_catalog())
    {
        if (!first)
            out << ',';
        first = false;
        out << "{\"id\":" << json_escape(info.id)
            << ",\"name\":" << json_escape(info.name)
            << ",\"needs_b\":" << json_bool(info.needs_second_input) << '}';
    }
    out << "]}";
    return out.str();
}

#endif
