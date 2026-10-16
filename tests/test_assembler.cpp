// Unit tests for the assembler (src/assembler.h). Assembled programs are run
// on the real CPU through EmulatorSession.
// Build (from the project root):
// clang++ -std=c++17 -Wall -Wextra tests/test_assembler.cpp -o build/test_assembler
// The same file is also built with em++ and run in Node by run_all_tests.sh.

#include "../src/assembler.h"
#include "../src/emulator_session.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

const ProgramInfo custom{"custom", "Custom Program", false};

// Assembles and runs a program; returns the session for inspection.
EmulatorSession run(const std::string &source, bool &ok)
{
    EmulatorSession session;
    AssemblyResult result = assemble(source);
    ok = result.ok;
    if (result.ok)
    {
        session.load_bytes(custom, result.bytes, 0, 0);
        ok = session.run_to_halt();
    }
    return session;
}

std::string state_of(const EmulatorSession &s)
{
    std::ostringstream out;
    s.write_json(out);
    return out.str();
}

bool contains(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

// True if assembly fails with an error on `line` containing `text`.
bool fails_with(const std::string &source, int line, const std::string &text)
{
    AssemblyResult result = assemble(source);
    if (result.ok || !result.bytes.empty())
        return false;
    for (const AssemblyMessage &e : result.errors)
    {
        if (e.line == line && contains(e.text, text))
            return true;
    }
    for (const AssemblyMessage &e : result.errors)
        std::cout << "  (got line " << e.line << ": " << e.text << ")\n";
    return false;
}

std::string repeat(const std::string &line, int count)
{
    std::string out;
    for (int i = 0; i < count; ++i)
        out += line + "\n";
    return out;
}

} // namespace

int main()
{
    // --- Instruction table matches the CPU -------------------------------
    {
        check(assembler_detail::mnemonic_table().size() == 28,
              "Assembler knows all 28 CPU instructions");
        bool consistent = true;
        for (int op = 0; op < 256; ++op)
        {
            bool has = operand_kind(static_cast<uint8_t>(op)) != OperandKind::None;
            if (has != instruction_has_operand(static_cast<uint8_t>(op)))
                consistent = false;
        }
        check(consistent, "Operand table matches instruction_has_operand()");
    }

    // --- Encoding and execution ------------------------------------------
    {
        AssemblyResult r = assemble("LOAD 42\nHALT\n");
        check(r.ok && r.bytes == std::vector<uint8_t>{1, 42, 255},
              "LOAD 42 / HALT encodes to 01 2A FF");
        bool ok;
        EmulatorSession s = run("LOAD 42\nHALT", ok);
        check(ok && contains(state_of(s), "\"registers\":{\"a\":42,"),
              "LOAD 42 / HALT runs and leaves A = 42");
    }

    {
        bool ok;
        EmulatorSession s = run("LOAD 10\nLOAD_B 32\nADD_B\nHALT", ok);
        check(ok && contains(state_of(s), "\"registers\":{\"a\":42,\"b\":32,"),
              "ADD_B adds both registers (10 + 32 = 42)");
        s = run("LOAD 5\nLOAD_B 7\nSUB_B\nHALT", ok);
        check(ok && contains(state_of(s), "\"registers\":{\"a\":254,"),
              "SUB_B wraps around (5 - 7 = 254)");
        s = run("LOAD 9\nMOV_B_A\nLOAD 0\nMOV_A_B\nHALT", ok);
        check(ok && contains(state_of(s), "\"registers\":{\"a\":9,\"b\":9,"),
              "MOV_B_A copies A to B and MOV_A_B copies B to A");
    }

    {
        // The built-in comparison program written in assembly must produce
        // exactly the same bytes as programs.h.
        const std::string source =
            "; Comparison, same layout as the built-in program\n"
            "        LOAD 7\n"
            "        LOAD_B 3\n"
            "        CMP\n"
            "        JL less\n"
            "        JG greater\n"
            "        JE equal\n"
            "        HALT\n"
            "less:   LOAD 1\n"
            "        HALT\n"
            "        .byte 0\n"
            "greater: LOAD 2\n"
            "        HALT\n"
            "        .byte 0\n"
            "equal:  LOAD 3\n"
            "        HALT\n";
        AssemblyResult r = assemble(source);
        check(r.ok && r.bytes == build_program("comparison", 7, 3),
              "Assembled comparison program == built-in comparison bytes");
        bool ok;
        EmulatorSession s = run(source, ok);
        std::string json = state_of(s);
        check(ok && contains(json, "\"registers\":{\"a\":2,") &&
                  contains(json, "\"greater\":true"),
              "Assembled comparison 7 vs 3 takes the JG branch");
    }

    {
        const std::string source =
            "        LOAD 5        ; counter\n"
            "loop:   STORE 0xB0    ; memory[0xB0] = A\n"
            "        SUB 1\n"
            "        JZ done       ; JZ jumps when A == 0\n"
            "        JMP loop\n"
            "done:   HALT\n";
        bool ok;
        EmulatorSession s = run(source, ok);
        std::string json = state_of(s);
        // 1 LOAD + 5 x (STORE, SUB, JZ) + 4 x JMP + HALT = 21 steps
        check(ok && contains(json, "\"steps\":21") &&
                  contains(json, "\"registers\":{\"a\":0,") &&
                  s.memory_at(0xB0) == 1,
              "Loop with labels and JZ runs 21 instructions, memory[0xB0] = 1");
    }

    {
        const std::string source =
            "        LOAD 21\n"
            "        CALL double\n"
            "        STORE result\n"
            "        PUSH\n"
            "        LOAD 0\n"
            "        POP\n"
            "        HALT\n"
            "double: MOV_B_A\n"
            "        ADD_B\n"
            "        RET\n"
            "result: .byte 0\n";
        bool ok;
        EmulatorSession s = run(source, ok);
        AssemblyResult r = assemble(source);
        check(ok && contains(state_of(s), "\"registers\":{\"a\":42,") &&
                  s.memory_at(static_cast<uint16_t>(r.bytes.size() - 1)) == 42,
              "CALL/RET, PUSH/POP and a data label work (21 * 2 = 42)");
    }

    // --- Syntax ----------------------------------------------------------
    {
        const std::string source =
            "\n"
            "   ; full-line comment\n"
            "start:\n"
            "\tload 0x0A   ; lower case, hex\n"
            "other: Load_B 0b11 \n"
            "  add_b\n"
            "  JmP end\n"
            "end: halt\n";
        AssemblyResult r = assemble(source);
        check(r.ok && r.bytes == std::vector<uint8_t>{1, 10, 8, 3, 12, 4, 7, 255},
              "Comments, blank lines, tabs, mixed case, hex and binary");
        check(r.source_map.size() == 5 && r.source_map[0].line == 4 &&
                  r.source_map[0].address == 0 && r.source_map[0].size == 2 &&
                  r.source_map[4].line == 8 && r.source_map[4].address == 7,
              "Source map links addresses to source lines");
        check(assemble("A: B: LOAD 1\nJMP b\nHALT").ok,
              "Several labels on one line, labels are case-insensitive");
        check(assemble(".byte 1, 2 3,4\nHALT").bytes ==
                  std::vector<uint8_t>{1, 2, 3, 4, 255},
              ".byte accepts commas and spaces");
        check(assemble("LOAD 255\nLOAD 0\nHALT").ok, "0 and 255 are valid values");
        check(assemble("LOAD_MEM 0xFF\nHALT").ok,
              "LOAD_MEM can read the stack region");
        check(assemble("STORE 0xBF\nHALT").ok, "STORE to 0xBF is allowed");
    }

    {
        AssemblyResult r = assemble("LOAD 1");
        check(r.ok && r.warnings.size() == 1 && contains(r.warnings[0].text, "HALT"),
              "Missing HALT gives a warning, not an error");
        check(assemble("LOAD 1\nHALT").warnings.empty(), "No warning with HALT");
    }

    // --- Errors ----------------------------------------------------------
    check(fails_with("LOAD 1\nLAOD 2\nHALT", 2, "Unknown instruction 'LAOD'"),
          "Unknown mnemonic reported with its line number");
    check(fails_with("HALT\nLOAD", 2, "LOAD needs an operand"), "Missing operand");
    check(fails_with("LOAD 1 2\nHALT", 1, "exactly one operand"), "Extra operand");
    check(fails_with("HALT 5", 1, "does not take an operand"),
          "Operand on an instruction without operands");
    check(fails_with("LOAD 1, 2\nHALT", 1, "commas are not used"), "Comma in operands");
    check(fails_with("LOAD 256\nHALT", 1, "out of range"), "Decimal value too large");
    check(fails_with("LOAD 0x100\nHALT", 1, "out of range"), "Hex value too large");
    check(fails_with("LOAD -1\nHALT", 1, "expects a number"), "Negative value");
    check(fails_with("LOAD 12abc\nHALT", 1, "expects a number"), "Malformed number");
    check(fails_with("LOAD 0x\nHALT", 1, "expects a number"), "Empty hex number");
    check(fails_with("LOAD 0b102\nHALT", 1, "expects a number"), "Bad binary digit");
    check(fails_with("x: LOAD x\nHALT", 1, "expects a number"),
          "Labels are not allowed as immediate values");
    check(fails_with("JMP nowhere\nHALT", 1, "Undefined label 'nowhere'"),
          "Undefined label");
    check(fails_with("JMP @@\nHALT", 1, "Invalid operand"), "Invalid operand text");
    check(fails_with("a: HALT\nA: HALT", 2, "Duplicate label 'A' (first defined on line 1)"),
          "Duplicate label (case-insensitive)");
    check(fails_with("load: HALT", 1, "same name as an instruction"),
          "Label named like an instruction");
    check(fails_with("1abc: HALT", 1, "Invalid label name"), "Invalid label name");
    check(fails_with("JMP 0x50\nHALT", 1, "outside the program"),
          "Numeric jump target outside the program");
    check(fails_with("LOAD 1\nJMP 1\nHALT", 2, "middle of an instruction"),
          "Jump into the middle of an instruction");
    check(fails_with("JMP end\nHALT\nend:", 1, "outside the program"),
          "Label after the last instruction is not a valid jump target");
    check(fails_with("CALL 0xC0\nHALT", 1, "outside the program"),
          "CALL into the stack region");
    check(fails_with("STORE 0xC0\nHALT", 1, "stack region"),
          "STORE into the stack region");
    check(fails_with("STORE_B 255\nHALT", 1, "stack region"),
          "STORE_B into the stack region");
    check(fails_with(".byte\nHALT", 1, "at least one value"), "Empty .byte");
    check(fails_with(".byte 1,,2\nHALT", 1, "Misplaced comma"), "Double comma in .byte");
    check(fails_with(".byte 1,\nHALT", 1, "Misplaced comma"), "Trailing comma in .byte");
    check(fails_with(".byte 300\nHALT", 1, "out of range"), ".byte value too large");
    check(fails_with("", 0, "Program is empty"), "Empty source");
    check(fails_with("; only a comment\n\nlabel:\n", 0, "Program is empty"),
          "Comments and labels only");
    check(fails_with(std::string(70000, ' '), 0, "too long"), "Source too long");

    {
        // 95 x LOAD (190 bytes) + HALT = 191 bytes; .byte 0 fills to 192.
        check(assemble(repeat("LOAD 1", 95) + "HALT\n.byte 0").ok,
              "A program of exactly 192 bytes fits");
        check(fails_with(repeat("LOAD 1", 96) + "HALT", 0, "193 bytes"),
              "A 193-byte program is rejected (stack region is reserved)");
        check(fails_with(repeat("LOAD 1", 200), 0, "reserved for the stack"),
              "A much larger program is rejected");
    }

    {
        AssemblyResult r = assemble("LAOD 1\nJMP x\nLOAD 999\nHALT 1");
        check(!r.ok && r.errors.size() == 4 && r.bytes.empty() &&
                  r.errors[0].line == 1 && r.errors[1].line == 2 &&
                  r.errors[2].line == 3 && r.errors[3].line == 4,
              "All errors are reported together, sorted by line, no bytes produced");
    }

    // --- Runtime behavior is still the CPU's ------------------------------
    {
        EmulatorSession s;
        s.load_bytes(custom, assemble("loop: JMP loop").bytes, 0, 0);
        check(!s.run_to_halt() && contains(s.error(), "Instruction limit"),
              "An infinite loop still stops at the instruction limit");
        s.load_bytes(custom, assemble("POP\nHALT").bytes, 0, 0);
        check(!s.execute_one() && contains(s.error(), "Stack underflow"),
              "CPU errors (stack underflow) are still reported by the CPU");
    }

    if (failures > 0)
    {
        std::cout << failures << " assembler test(s) failed.\n";
        return 1;
    }
    std::cout << "All assembler tests passed.\n";
    return 0;
}
