// WebAssembly interface for the browser.
//
// Build: ./scripts/build_wasm.sh (outputs web/generated/cpu.js and cpu.wasm)
//
// Every cpu_* function except cpu_memory_byte returns a JSON string in the
// same format as the HTTP API in web_server.cpp:
//   {"ok":true,"state":{...}}  or  {"ok":false,"error":"...","state":{...}}
// The returned pointer stays valid until the next call; JavaScript copies it
// immediately with UTF8ToString.

#include <cmath>
#include <exception>
#include <string>

#include "assembler.h"
#include "emulator_session.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

namespace
{

EmulatorSession session;
std::string response;

// Program shown in the UI for code loaded from the assembly editor.
const ProgramInfo custom_program{"custom", "Custom Program", false};

const char *reply(std::string text)
{
    response = std::move(text);
    return response.c_str();
}

// Accepts whole numbers from 0 to 255. JavaScript numbers arrive as doubles,
// so fractions and NaN are rejected here instead of being truncated.
bool read_byte(double value, const char *name, uint8_t &out, std::string &error)
{
    if (std::isnan(value) || value != std::floor(value) ||
        value < 0 || value > 255)
    {
        error = std::string("Input ") + name +
                " must be a whole number from 0 to 255";
        return false;
    }
    out = static_cast<uint8_t>(value);
    return true;
}

void write_messages(std::ostream &out, const std::vector<AssemblyMessage> &list)
{
    out << '[';
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (i > 0)
            out << ',';
        out << "{\"line\":" << list[i].line
            << ",\"message\":" << json_escape(list[i].text) << '}';
    }
    out << ']';
}

} // namespace

extern "C"
{

WASM_EXPORT const char *cpu_programs()
{
    return reply(programs_envelope());
}

WASM_EXPORT const char *cpu_state()
{
    return reply(state_envelope(session));
}

// Resets the CPU and loads a program. Pass NaN as b for programs that only
// use input A (NOT, SHL, SHR); b is ignored for those programs.
WASM_EXPORT const char *cpu_load(const char *program_id, double a, double b)
{
    try
    {
        const ProgramInfo *program =
            program_id == nullptr ? nullptr : find_program(program_id);
        if (program == nullptr)
        {
            return reply(error_envelope(
                std::string("Unknown program: ") +
                (program_id == nullptr ? "" : program_id)));
        }

        uint8_t first = 0;
        uint8_t second = 0;
        std::string error;
        if (std::isnan(a))
        {
            return reply(error_envelope("Input a is required"));
        }
        if (!read_byte(a, "a", first, error))
        {
            return reply(error_envelope(error));
        }
        if (program->needs_second_input)
        {
            if (std::isnan(b))
            {
                return reply(error_envelope("Input b is required"));
            }
            if (!read_byte(b, "b", second, error))
            {
                return reply(error_envelope(error));
            }
        }

        session.load(*program, first, second);
        return reply(state_envelope(session));
    }
    catch (const std::exception &e)
    {
        return reply(error_envelope(
            std::string("Could not load program: ") + e.what()));
    }
}

// Assembles source code and, if it has no errors, resets the CPU and loads
// the machine code. On errors the loaded program is left unchanged.
// Success: {"ok":true,"state":{...},"assembly":{"size":N,"warnings":[...],
//           "source_map":[{"line":L,"address":A,"size":S,"instruction":true}]}}
// Failure: {"ok":false,"error":"...","assembly":{"errors":[{"line":L,
//           "message":"..."}],"warnings":[...]}}
WASM_EXPORT const char *cpu_load_assembly(const char *source)
{
    try
    {
        AssemblyResult result = assemble(source == nullptr ? "" : source);
        std::ostringstream out;

        if (!result.ok)
        {
            size_t count = result.errors.size();
            out << "{\"ok\":false,\"error\":"
                << json_escape("Assembly failed with " + std::to_string(count) +
                               (count == 1 ? " error." : " errors."))
                << ",\"assembly\":{\"errors\":";
            write_messages(out, result.errors);
            out << ",\"warnings\":";
            write_messages(out, result.warnings);
            out << "}}";
            return reply(out.str());
        }

        session.load_bytes(custom_program, result.bytes, 0, 0);

        out << "{\"ok\":true,\"state\":";
        session.write_json(out);
        out << ",\"assembly\":{\"size\":" << result.bytes.size()
            << ",\"warnings\":";
        write_messages(out, result.warnings);
        out << ",\"source_map\":[";
        for (size_t i = 0; i < result.source_map.size(); ++i)
        {
            const SourceMapEntry &entry = result.source_map[i];
            if (i > 0)
                out << ',';
            out << "{\"line\":" << entry.line
                << ",\"address\":" << entry.address
                << ",\"size\":" << entry.size
                << ",\"instruction\":" << json_bool(entry.is_instruction) << '}';
        }
        out << "]}}";
        return reply(out.str());
    }
    catch (const std::exception &e)
    {
        return reply(error_envelope(std::string("Could not load program: ") +
                                    e.what()));
    }
}

// Executes exactly one instruction.
WASM_EXPORT const char *cpu_step()
{
    try
    {
        if (const char *reason = execution_blocker(session))
            return reply(error_envelope(reason, &session));
        if (!session.execute_one())
            return reply(error_envelope(session.error(), &session));
        return reply(state_envelope(session));
    }
    catch (const std::exception &e)
    {
        return reply(error_envelope(std::string("Internal error: ") + e.what()));
    }
}

// Runs until HALT, a CPU fault, or MAX_RUN_STEPS instructions.
WASM_EXPORT const char *cpu_run()
{
    try
    {
        if (const char *reason = execution_blocker(session))
            return reply(error_envelope(reason, &session));
        if (!session.run_to_halt())
            return reply(error_envelope(session.error(), &session));
        return reply(state_envelope(session));
    }
    catch (const std::exception &e)
    {
        return reply(error_envelope(std::string("Internal error: ") + e.what()));
    }
}

// Restarts the loaded program from its initial state.
WASM_EXPORT const char *cpu_reset()
{
    try
    {
        session.reset();
        return reply(state_envelope(session));
    }
    catch (const std::exception &e)
    {
        return reply(error_envelope(std::string("Internal error: ") + e.what()));
    }
}

// Returns one memory byte (0-255), or -1 for an address outside memory.
WASM_EXPORT int cpu_memory_byte(int address)
{
    if (address < 0 || address >= CPU::MEMORY_SIZE)
    {
        return -1;
    }
    return session.memory_at(static_cast<uint16_t>(address));
}

} // extern "C"
