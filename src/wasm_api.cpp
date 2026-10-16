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
