#ifndef ASSEMBLER_H
#define ASSEMBLER_H

// Two-pass assembler for the CPU in cpu.h.
//
// Syntax (one statement per line):
//   label:                 defines a label at the current address
//   MNEMONIC [operand]     an instruction, e.g. LOAD 5, JMP loop, ADD_B
//   .byte value, value...  raw data bytes
//   ; comment              everything after ';' is ignored
//
// Mnemonics, labels and directives are case-insensitive. Numbers can be
// decimal (42), hexadecimal (0x2A) or binary (0b101010), from 0 to 255.
// Labels can be used as operands of jumps, CALL, LOAD_MEM, STORE and STORE_B.
//
// Opcodes come from the CPU class and mnemonics from instruction_name(), so
// the assembler always matches the CPU.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "cpu.h"
#include "instruction_names.h"

enum class OperandKind
{
    None,         // no operand (ADD_B, CMP, HALT, ...)
    Immediate,    // value 0-255 (LOAD, LOAD_B, ADD, SUB)
    JumpTarget,   // code address (JMP, JZ, JE, JL, JG, CALL)
    ReadAddress,  // any memory address 0-255 (LOAD_MEM)
    WriteAddress, // address below the stack region (STORE, STORE_B)
};

// The operand each instruction reads from memory after its opcode, matching
// the fetch() calls in CPU::step().
inline OperandKind operand_kind(uint8_t opcode)
{
    switch (opcode)
    {
    case CPU::OP_LOAD:
    case CPU::OP_LOAD_B:
    case CPU::OP_ADD:
    case CPU::OP_SUB:
        return OperandKind::Immediate;
    case CPU::OP_JMP:
    case CPU::OP_JZ:
    case CPU::OP_JE:
    case CPU::OP_JL:
    case CPU::OP_JG:
    case CPU::OP_CALL:
        return OperandKind::JumpTarget;
    case CPU::OP_LOAD_MEM:
        return OperandKind::ReadAddress;
    case CPU::OP_STORE:
    case CPU::OP_STORE_B:
        return OperandKind::WriteAddress;
    default:
        return OperandKind::None;
    }
}

struct AssemblyMessage
{
    int line = 0; // 1-based source line
    std::string text;
};

// Where each instruction or data statement was placed in memory.
struct SourceMapEntry
{
    int line = 0;
    uint16_t address = 0;
    uint16_t size = 0;
    bool is_instruction = false;
};

struct AssemblyResult
{
    bool ok = false;
    std::vector<uint8_t> bytes;
    std::vector<AssemblyMessage> errors;
    std::vector<AssemblyMessage> warnings;
    std::vector<SourceMapEntry> source_map;
};

namespace assembler_detail
{

constexpr size_t MAX_SOURCE_BYTES = 64 * 1024;
constexpr size_t MAX_ERRORS = 50;

inline std::string to_upper(std::string text)
{
    for (char &c : text)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

inline std::string trim(const std::string &text)
{
    size_t start = text.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";
    size_t end = text.find_last_not_of(" \t\r");
    return text.substr(start, end - start + 1);
}

inline std::string hex_address(unsigned value)
{
    const char *digits = "0123456789ABCDEF";
    std::string out = "0x";
    out += digits[(value >> 4) & 0xF];
    out += digits[value & 0xF];
    return out;
}

// Mnemonic -> opcode, built from instruction_name() so it cannot drift from
// the CPU's instruction set.
inline const std::map<std::string, uint8_t> &mnemonic_table()
{
    static const std::map<std::string, uint8_t> table = []
    {
        std::map<std::string, uint8_t> result;
        for (int opcode = 0; opcode < 256; ++opcode)
        {
            std::string name = instruction_name(static_cast<uint8_t>(opcode));
            if (name != "UNKNOWN")
                result[name] = static_cast<uint8_t>(opcode);
        }
        return result;
    }();
    return table;
}

inline bool is_label_name(const std::string &text)
{
    if (text.empty() ||
        !(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_'))
        return false;
    for (char c : text)
    {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    }
    return true;
}

// Parses 42, 0x2A or 0b101010. Returns false if the text is not a number.
// Values above 255 are reported through `value` (clamped to 256+).
inline bool parse_number(const std::string &text, int &value)
{
    std::string digits = text;
    int base = 10;
    if (digits.size() > 2 && digits[0] == '0' &&
        (digits[1] == 'x' || digits[1] == 'X'))
    {
        base = 16;
        digits = digits.substr(2);
    }
    else if (digits.size() > 2 && digits[0] == '0' &&
             (digits[1] == 'b' || digits[1] == 'B'))
    {
        base = 2;
        digits = digits.substr(2);
    }
    if (digits.empty())
        return false;

    long long result = 0;
    for (char c : digits)
    {
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return false;
        if (digit >= base)
            return false;
        if (result < 100000)
            result = result * base + digit;
    }
    value = static_cast<int>(result);
    return true;
}

// One parsed source line, kept between the two passes.
struct Statement
{
    int line = 0;
    bool is_instruction = false;
    uint8_t opcode = 0;
    std::string mnemonic;
    std::vector<std::string> operands;
    uint16_t address = 0;
    uint16_t size = 0;
};

} // namespace assembler_detail

inline AssemblyResult assemble(const std::string &source)
{
    using namespace assembler_detail;

    AssemblyResult result;
    auto error = [&](int line, const std::string &text)
    {
        if (result.errors.size() < MAX_ERRORS)
            result.errors.push_back({line, text});
    };

    if (source.size() > MAX_SOURCE_BYTES)
    {
        error(0, "Source is too long (max " +
                     std::to_string(MAX_SOURCE_BYTES / 1024) + " KB)");
        return result;
    }

    // Pass 1: parse lines, assign addresses, collect labels.
    std::vector<Statement> statements;
    std::map<std::string, uint16_t> labels; // upper-case name -> address
    std::map<std::string, int> label_lines;
    uint32_t address = 0;

    size_t pos = 0;
    int line_number = 0;
    while (pos <= source.size())
    {
        size_t end = source.find('\n', pos);
        if (end == std::string::npos)
            end = source.size();
        std::string line = source.substr(pos, end - pos);
        pos = end + 1;
        ++line_number;

        size_t comment = line.find(';');
        if (comment != std::string::npos)
            line = line.substr(0, comment);
        line = trim(line);

        // Labels: "name:" at the start of the line, possibly several.
        size_t colon;
        while ((colon = line.find(':')) != std::string::npos)
        {
            std::string name = trim(line.substr(0, colon));
            if (!is_label_name(name))
            {
                error(line_number, "Invalid label name '" + name +
                                       "' (use letters, digits and _; "
                                       "start with a letter or _)");
                line.clear();
                break;
            }
            std::string key = to_upper(name);
            if (mnemonic_table().count(key) != 0 || key == ".BYTE")
            {
                error(line_number, "Label '" + name +
                                       "' has the same name as an instruction");
            }
            else if (labels.count(key) != 0)
            {
                error(line_number, "Duplicate label '" + name +
                                       "' (first defined on line " +
                                       std::to_string(label_lines[key]) + ")");
            }
            else
            {
                labels[key] = static_cast<uint16_t>(address);
                label_lines[key] = line_number;
            }
            line = trim(line.substr(colon + 1));
        }

        if (line.empty())
            continue;

        Statement statement;
        statement.line = line_number;
        statement.address = static_cast<uint16_t>(address);

        size_t space = line.find_first_of(" \t");
        statement.mnemonic = to_upper(line.substr(0, space));
        std::string rest =
            space == std::string::npos ? "" : trim(line.substr(space));

        if (statement.mnemonic == ".BYTE")
        {
            // Values separated by commas and/or spaces: ".byte 1, 2 3".
            std::string current;
            bool comma_pending = false; // a comma not yet followed by a value
            bool bad_comma = false;
            auto flush = [&]()
            {
                if (!current.empty())
                {
                    statement.operands.push_back(current);
                    current.clear();
                    comma_pending = false;
                }
            };
            for (char c : rest)
            {
                if (c == ',')
                {
                    flush();
                    if (statement.operands.empty() || comma_pending)
                        bad_comma = true;
                    comma_pending = true;
                }
                else if (c == ' ' || c == '\t')
                {
                    flush();
                }
                else
                {
                    current += c;
                }
            }
            flush();
            if (bad_comma || comma_pending)
            {
                error(line_number, "Misplaced comma in .byte list");
                continue;
            }
            if (statement.operands.empty())
            {
                error(line_number, ".byte needs at least one value");
                continue;
            }
            statement.size = static_cast<uint16_t>(statement.operands.size());
        }
        else
        {
            auto it = mnemonic_table().find(statement.mnemonic);
            if (it == mnemonic_table().end())
            {
                error(line_number, "Unknown instruction '" +
                                       line.substr(0, space) + "'");
                continue;
            }
            statement.is_instruction = true;
            statement.opcode = it->second;

            if (rest.find(',') != std::string::npos)
            {
                error(line_number, statement.mnemonic +
                                       " takes at most one operand; "
                                       "commas are not used");
                continue;
            }
            size_t i = 0;
            while (i < rest.size())
            {
                size_t next = rest.find_first_of(" \t", i);
                if (next == std::string::npos)
                    next = rest.size();
                if (next > i)
                    statement.operands.push_back(rest.substr(i, next - i));
                i = next + 1;
            }

            bool needs_operand =
                operand_kind(statement.opcode) != OperandKind::None;
            if (needs_operand && statement.operands.empty())
            {
                error(line_number, statement.mnemonic + " needs an operand");
                continue;
            }
            if (!needs_operand && !statement.operands.empty())
            {
                error(line_number, statement.mnemonic +
                                       " does not take an operand");
                continue;
            }
            if (statement.operands.size() > 1)
            {
                error(line_number, statement.mnemonic +
                                       " takes exactly one operand");
                continue;
            }
            statement.size = needs_operand ? 2 : 1;
        }

        address += statement.size;
        statements.push_back(statement);
    }

    if (address > CPU::STACK_LIMIT)
    {
        error(0, "Program is " + std::to_string(address) +
                     " bytes; the maximum is " +
                     std::to_string(CPU::STACK_LIMIT) +
                     " bytes (addresses 0x00-0xBF). Addresses 0xC0-0xFF "
                     "are reserved for the stack.");
    }
    if (statements.empty() && result.errors.empty())
    {
        error(0, "Program is empty. Write at least one instruction, "
                 "for example: LOAD 5 then HALT");
    }
    if (statements.empty() || address > CPU::STACK_LIMIT)
        return result;

    // After a pass-1 error some lines were skipped, so addresses may be
    // shifted. Pass 2 still reports operand errors, but skips the checks that
    // depend on the program layout to avoid misleading messages.
    bool layout_ok = result.errors.empty();

    uint16_t program_size = static_cast<uint16_t>(address);

    // Addresses where an instruction starts, to check numeric jump targets.
    std::vector<bool> instruction_start(program_size, false);
    std::vector<bool> data_start(program_size, false);
    for (const Statement &s : statements)
    {
        if (s.is_instruction)
            instruction_start[s.address] = true;
        else
            for (uint16_t i = 0; i < s.size; ++i)
                data_start[s.address + i] = true;
    }

    // Pass 2: resolve operands and emit bytes.
    bool has_halt = false;
    for (const Statement &s : statements)
    {
        result.source_map.push_back({s.line, s.address, s.size, s.is_instruction});

        if (!s.is_instruction)
        {
            for (const std::string &text : s.operands)
            {
                int value = 0;
                if (!parse_number(text, value))
                    error(s.line, "Invalid number '" + text + "' in .byte");
                else if (value > 255)
                    error(s.line, ".byte value " + text +
                                      " is out of range (0-255)");
                result.bytes.push_back(static_cast<uint8_t>(value));
            }
            continue;
        }

        result.bytes.push_back(s.opcode);
        if (s.opcode == CPU::OP_HALT)
            has_halt = true;

        OperandKind kind = operand_kind(s.opcode);
        if (kind == OperandKind::None)
            continue;

        const std::string &text = s.operands[0];
        int value = 0;
        bool is_number = parse_number(text, value);
        bool from_label = false;

        if (!is_number)
        {
            auto label = labels.find(to_upper(text));
            if (kind == OperandKind::Immediate)
            {
                error(s.line, s.mnemonic + " expects a number from 0 to 255, "
                                           "not '" + text + "'");
            }
            else if (label != labels.end())
            {
                value = label->second;
                from_label = true;
            }
            else if (is_label_name(text))
            {
                error(s.line, "Undefined label '" + text + "'");
            }
            else
            {
                error(s.line, "Invalid operand '" + text +
                                  "' (expected a number or a label)");
            }
            if (!from_label)
            {
                result.bytes.push_back(0);
                continue;
            }
        }

        if (value > 255)
        {
            error(s.line, "Value " + text + " is out of range (0-255)");
        }
        else if (kind == OperandKind::JumpTarget && layout_ok)
        {
            if (value >= program_size)
            {
                error(s.line, s.mnemonic + " target " + hex_address(value) +
                                  " is outside the program (last address " +
                                  hex_address(program_size - 1u) + ")");
            }
            else if (!from_label && !instruction_start[value] &&
                     !data_start[value])
            {
                error(s.line, s.mnemonic + " target " + hex_address(value) +
                                  " is in the middle of an instruction");
            }
        }
        else if (kind == OperandKind::WriteAddress &&
                 value >= CPU::STACK_LIMIT)
        {
            error(s.line, s.mnemonic + " address " + hex_address(value) +
                              " is in the stack region (0xC0-0xFF); "
                              "use an address from 0x00 to 0xBF");
        }

        result.bytes.push_back(static_cast<uint8_t>(value));
    }

    if (!result.errors.empty())
    {
        std::stable_sort(result.errors.begin(), result.errors.end(),
                         [](const AssemblyMessage &a, const AssemblyMessage &b)
                         { return a.line < b.line; });
        result.bytes.clear();
        result.source_map.clear();
        return result;
    }

    if (!has_halt)
    {
        result.warnings.push_back(
            {0, "The program has no HALT instruction, so it can only stop "
                "with a CPU error (for example by running past its last "
                "instruction or reaching the Run limit)."});
    }

    result.ok = true;
    return result;
}

#endif
