// Unit tests for the web server's CPU session logic.
// Build (from the project root):
// clang++ -std=c++17 -Wall -Wextra tests/test_web_session.cpp -o build/test_web_session

#define main web_server_main
#include "../src/web_server.cpp"
#undef main

#include <iostream>
#include <sstream>

namespace
{

int failures = 0;

void check(bool condition, const std::string &name)
{
    if (condition)
    {
        std::cout << "PASS: " << name << '\n';
    }
    else
    {
        std::cout << "FAIL: " << name << '\n';
        ++failures;
    }
}

bool contains(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

std::string state_of(const EmulatorSession &s)
{
    std::ostringstream out;
    s.write_json(out);
    return out.str();
}

const ProgramInfo test_program{"test", "Test", false};

} // namespace

int main()
{
    {
        EmulatorSession s;
        check(s.status() == "IDLE", "New session is IDLE");
        check(contains(state_of(s), "\"program\":null"),
              "IDLE state has no program");
    }

    {
        // JMP 0 loops forever; run must stop at the instruction limit.
        EmulatorSession s;
        s.load_bytes(test_program, {CPU::OP_JMP, 0}, 0, 0);
        check(!s.run_to_halt(), "Infinite loop is stopped");
        check(s.status() == "ERROR", "Infinite loop sets ERROR status");
        check(contains(s.error(), "Instruction limit reached"),
              "Infinite loop error message");
        check(contains(state_of(s), "\"steps\":1000"),
              "Infinite loop executed exactly 1000 instructions");

        s.reset();
        check(s.status() == "READY", "Reset after error returns to READY");
        check(contains(state_of(s), "\"trace\":[]"), "Reset clears trace");
        check(contains(state_of(s), "\"memory\":[4,0,0"),
              "Reset reloads program bytes");
    }

    {
        // Opcode 99 is not a valid instruction.
        EmulatorSession s;
        s.load_bytes(test_program, {99}, 0, 0);
        check(!s.execute_one(), "Unknown instruction is reported");
        check(s.status() == "ERROR", "Unknown instruction sets ERROR");
        check(contains(s.error(), "Unknown instruction: 99"),
              "Unknown instruction message comes from CPU");
    }

    {
        EmulatorSession s;
        s.load(*find_program("comparison"), 3, 9);
        check(s.status() == "READY", "Loaded program is READY");
        check(s.execute_one(), "First step succeeds");
        check(s.status() == "RUNNING", "Status is RUNNING after a step");
        check(s.run_to_halt(), "Comparison runs to HALT");
        check(s.status() == "HALTED", "Status is HALTED");

        std::string json = state_of(s);
        check(contains(json, "\"registers\":{\"a\":1,"),
              "Comparison 3 vs 9 leaves A = 1");
        check(contains(json, "\"text\":\"A < B\""),
              "Comparison result text matches terminal app");
    }

    {
        EmulatorSession s;
        s.load(*find_program("not"), 12, 0);
        std::string json = state_of(s);
        check(contains(json, "\"b\":null"), "Unary program reports no B input");
        check(contains(json, "\"code_size\":4"), "NOT program is 4 bytes");
    }

    if (failures > 0)
    {
        std::cout << failures << " session test(s) failed.\n";
        return 1;
    }
    std::cout << "All web session tests passed.\n";
    return 0;
}
