#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cpu.h"
#include "instruction_names.h"
#include "program_loader.h"
#include "programs.h"

namespace
{

constexpr int DEFAULT_PORT = 8080;
constexpr size_t MAX_HEADER_BYTES = 8192;
constexpr size_t MAX_BODY_BYTES = 4096;
constexpr uint32_t MAX_RUN_STEPS = 1000;
constexpr size_t MAX_TRACE_ENTRIES = 2000;
constexpr int SOCKET_TIMEOUT_SECONDS = 5;

int server_port = DEFAULT_PORT;
std::string web_root = "web";

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

std::string hex_byte(unsigned value)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02X", value & 0xFF);
    return buffer;
}

std::string json_escape(const std::string &text)
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

const char *json_bool(bool value)
{
    return value ? "true" : "false";
}

std::string to_lower(std::string text)
{
    for (char &c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string trim(const std::string &text)
{
    size_t start = text.find_first_not_of(" \t");
    if (start == std::string::npos)
    {
        return "";
    }
    size_t end = text.find_last_not_of(" \t");
    return text.substr(start, end - start + 1);
}

// ---------------------------------------------------------------------------
// Minimal JSON parser for flat request objects:
// {"key": "string" | integer | true | false | null, ...}
// ---------------------------------------------------------------------------

struct JsonValue
{
    enum class Type
    {
        Null,
        Bool,
        Integer,
        Fraction,
        String
    };

    Type type = Type::Null;
    bool boolean = false;
    long long integer = 0;
    std::string text;
};

using JsonObject = std::map<std::string, JsonValue>;

class FlatJsonParser
{
public:
    explicit FlatJsonParser(const std::string &input) : input_(input) {}

    // Throws std::invalid_argument when the input is not a valid flat object.
    JsonObject parse_object()
    {
        JsonObject result;
        skip_whitespace();
        expect('{');
        skip_whitespace();

        if (peek() == '}')
        {
            ++pos_;
        }
        else
        {
            while (true)
            {
                skip_whitespace();
                std::string key = parse_string();
                skip_whitespace();
                expect(':');
                skip_whitespace();
                result[key] = parse_value();
                skip_whitespace();

                if (peek() == ',')
                {
                    ++pos_;
                    continue;
                }
                expect('}');
                break;
            }
        }

        skip_whitespace();
        if (pos_ != input_.size())
        {
            fail("unexpected data after the JSON object");
        }
        return result;
    }

private:
    const std::string &input_;
    size_t pos_ = 0;

    [[noreturn]] void fail(const std::string &message) const
    {
        throw std::invalid_argument(
            "Malformed JSON at position " + std::to_string(pos_) +
            ": " + message);
    }

    char peek() const
    {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    void expect(char c)
    {
        if (peek() != c)
        {
            fail(std::string("expected '") + c + "'");
        }
        ++pos_;
    }

    void skip_whitespace()
    {
        while (pos_ < input_.size() &&
               (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                input_[pos_] == '\n' || input_[pos_] == '\r'))
        {
            ++pos_;
        }
    }

    bool consume_literal(const char *literal)
    {
        size_t length = std::strlen(literal);
        if (input_.compare(pos_, length, literal) == 0)
        {
            pos_ += length;
            return true;
        }
        return false;
    }

    static void append_utf8(std::string &out, unsigned code)
    {
        if (code < 0x80)
        {
            out += static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }

    std::string parse_string()
    {
        expect('"');
        std::string out;

        while (true)
        {
            if (pos_ >= input_.size())
            {
                fail("unterminated string");
            }

            unsigned char c = static_cast<unsigned char>(input_[pos_++]);
            if (c == '"')
            {
                return out;
            }
            if (c < 0x20)
            {
                fail("control character in string");
            }
            if (c != '\\')
            {
                out += static_cast<char>(c);
                continue;
            }

            char escape = peek();
            ++pos_;
            switch (escape)
            {
            case '"':
            case '\\':
            case '/':
                out += escape;
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u':
            {
                unsigned code = 0;
                for (int i = 0; i < 4; ++i)
                {
                    char h = peek();
                    ++pos_;
                    code <<= 4;
                    if (h >= '0' && h <= '9')
                        code |= static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f')
                        code |= static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F')
                        code |= static_cast<unsigned>(h - 'A' + 10);
                    else
                        fail("invalid \\u escape");
                }
                append_utf8(out, code);
                break;
            }
            default:
                fail("invalid escape sequence");
            }
        }
    }

    JsonValue parse_number()
    {
        JsonValue value;
        value.type = JsonValue::Type::Integer;
        bool negative = false;

        if (peek() == '-')
        {
            negative = true;
            ++pos_;
        }

        if (!std::isdigit(static_cast<unsigned char>(peek())))
        {
            fail("invalid number");
        }

        long long magnitude = 0;
        if (peek() == '0')
        {
            ++pos_;
        }
        else
        {
            while (std::isdigit(static_cast<unsigned char>(peek())))
            {
                // Clamp huge values; range validation rejects them later.
                if (magnitude < 1000000000LL)
                {
                    magnitude = magnitude * 10 + (peek() - '0');
                }
                ++pos_;
            }
        }

        if (peek() == '.')
        {
            ++pos_;
            if (!std::isdigit(static_cast<unsigned char>(peek())))
            {
                fail("invalid number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek())))
            {
                ++pos_;
            }
            value.type = JsonValue::Type::Fraction;
        }

        if (peek() == 'e' || peek() == 'E')
        {
            ++pos_;
            if (peek() == '+' || peek() == '-')
            {
                ++pos_;
            }
            if (!std::isdigit(static_cast<unsigned char>(peek())))
            {
                fail("invalid number");
            }
            while (std::isdigit(static_cast<unsigned char>(peek())))
            {
                ++pos_;
            }
            value.type = JsonValue::Type::Fraction;
        }

        value.integer = negative ? -magnitude : magnitude;
        return value;
    }

    JsonValue parse_value()
    {
        JsonValue value;
        char c = peek();

        if (c == '"')
        {
            value.type = JsonValue::Type::String;
            value.text = parse_string();
        }
        else if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
        {
            value = parse_number();
        }
        else if (consume_literal("true"))
        {
            value.type = JsonValue::Type::Bool;
            value.boolean = true;
        }
        else if (consume_literal("false"))
        {
            value.type = JsonValue::Type::Bool;
        }
        else if (consume_literal("null"))
        {
            value.type = JsonValue::Type::Null;
        }
        else if (c == '{' || c == '[')
        {
            fail("nested objects and arrays are not supported");
        }
        else
        {
            fail("unexpected value");
        }
        return value;
    }
};

// ---------------------------------------------------------------------------
// CPU session
// ---------------------------------------------------------------------------

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

bool instruction_has_operand(uint8_t opcode)
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

void write_snapshot_json(std::ostream &out, const CpuSnapshot &s)
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

EmulatorSession session;

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

struct HttpRequest
{
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers; // lower-case names
    std::string body;

    std::string header(const std::string &name) const
    {
        auto it = headers.find(name);
        return it == headers.end() ? "" : it->second;
    }
};

struct HttpResponse
{
    int status = 200;
    std::string content_type = "text/plain; charset=utf-8";
    std::string body;
    std::string allow;
};

const char *status_text(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 408:
        return "Request Timeout";
    case 409:
        return "Conflict";
    case 411:
        return "Length Required";
    case 413:
        return "Payload Too Large";
    case 415:
        return "Unsupported Media Type";
    case 422:
        return "Unprocessable Entity";
    case 431:
        return "Request Header Fields Too Large";
    case 500:
        return "Internal Server Error";
    default:
        return "Error";
    }
}

HttpResponse json_response(int status, const std::string &body)
{
    HttpResponse response;
    response.status = status;
    response.content_type = "application/json; charset=utf-8";
    response.body = body;
    return response;
}

HttpResponse json_error(int status, const std::string &message,
                        bool include_state = false)
{
    std::ostringstream out;
    out << "{\"ok\":false,\"error\":" << json_escape(message);
    if (include_state)
    {
        out << ",\"state\":";
        session.write_json(out);
    }
    out << '}';
    return json_response(status, out.str());
}

HttpResponse state_response(int status = 200)
{
    std::ostringstream out;
    out << "{\"ok\":true,\"state\":";
    session.write_json(out);
    out << '}';
    return json_response(status, out.str());
}

bool send_all(int fd, const std::string &data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

void send_response(int fd, const HttpResponse &response)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << ' '
        << status_text(response.status) << "\r\n"
        << "Content-Type: " << response.content_type << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "X-Content-Type-Options: nosniff\r\n";
    if (!response.allow.empty())
    {
        out << "Allow: " << response.allow << "\r\n";
    }
    out << "Connection: close\r\n\r\n"
        << response.body;

    if (!send_all(fd, out.str()))
    {
        std::cerr << "Warning: failed to send response: "
                  << std::strerror(errno) << '\n';
    }
}

// Reads one HTTP request. On failure, sets error_status (0 means the client
// disconnected and no response should be sent).
bool read_request(int fd, HttpRequest &request, int &error_status,
                  std::string &error_message)
{
    std::string buffer;
    char chunk[2048];
    size_t header_end = std::string::npos;

    auto receive_more = [&]() -> bool
    {
        while (true)
        {
            ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
            if (n > 0)
            {
                buffer.append(chunk, static_cast<size_t>(n));
                return true;
            }
            if (n == 0)
            {
                error_status = 0;
                return false;
            }
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                error_status = 408;
                error_message = "Timed out while reading the request";
            }
            else
            {
                error_status = 0;
            }
            return false;
        }
    };

    while ((header_end = buffer.find("\r\n\r\n")) == std::string::npos)
    {
        if (buffer.size() > MAX_HEADER_BYTES)
        {
            error_status = 431;
            error_message = "Request headers are too large";
            return false;
        }
        if (!receive_more())
        {
            return false;
        }
    }

    if (header_end > MAX_HEADER_BYTES)
    {
        error_status = 431;
        error_message = "Request headers are too large";
        return false;
    }

    std::istringstream lines(buffer.substr(0, header_end));
    std::string request_line;
    std::getline(lines, request_line);
    if (!request_line.empty() && request_line.back() == '\r')
        request_line.pop_back();

    std::istringstream parts(request_line);
    std::string target;
    std::string version;
    std::string extra;
    if (!(parts >> request.method >> target >> version) || (parts >> extra) ||
        version.rfind("HTTP/1.", 0) != 0 || target.empty() || target[0] != '/')
    {
        error_status = 400;
        error_message = "Malformed request line";
        return false;
    }

    size_t query = target.find('?');
    request.path = target.substr(0, query);

    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0)
        {
            error_status = 400;
            error_message = "Malformed header line";
            return false;
        }
        request.headers[to_lower(line.substr(0, colon))] =
            trim(line.substr(colon + 1));
    }

    if (!request.header("transfer-encoding").empty())
    {
        error_status = 411;
        error_message = "Chunked request bodies are not supported; "
                        "send Content-Length";
        return false;
    }

    size_t content_length = 0;
    std::string length_text = request.header("content-length");
    if (!length_text.empty())
    {
        if (length_text.size() > 9 ||
            length_text.find_first_not_of("0123456789") != std::string::npos)
        {
            error_status = 400;
            error_message = "Invalid Content-Length header";
            return false;
        }
        content_length = std::stoul(length_text);
        if (content_length > MAX_BODY_BYTES)
        {
            error_status = 413;
            error_message = "Request body is too large (max " +
                            std::to_string(MAX_BODY_BYTES) + " bytes)";
            return false;
        }
    }

    size_t body_start = header_end + 4;
    while (buffer.size() - body_start < content_length)
    {
        if (!receive_more())
        {
            if (error_status == 0)
            {
                error_status = 400;
                error_message = "Request body is shorter than Content-Length";
            }
            return false;
        }
    }

    request.body = buffer.substr(body_start, content_length);
    return true;
}

// The server only listens on 127.0.0.1. These checks additionally block
// DNS-rebinding and cross-site requests from pages in other origins.
bool is_allowed_host(const std::string &host)
{
    std::string port = std::to_string(server_port);
    return host == "127.0.0.1:" + port || host == "localhost:" + port;
}

bool is_allowed_origin(const std::string &origin)
{
    std::string port = std::to_string(server_port);
    return origin == "http://127.0.0.1:" + port ||
           origin == "http://localhost:" + port;
}

// ---------------------------------------------------------------------------
// Handlers
// ---------------------------------------------------------------------------

bool read_byte_field(const JsonObject &body, const char *name,
                     bool required, uint8_t &out, std::string &error)
{
    auto it = body.find(name);
    if (it == body.end() || it->second.type == JsonValue::Type::Null)
    {
        if (required)
        {
            error = std::string("Input ") + name + " is required";
            return false;
        }
        out = 0;
        return true;
    }

    const JsonValue &value = it->second;
    if (value.type != JsonValue::Type::Integer ||
        value.integer < 0 || value.integer > 255)
    {
        error = std::string("Input ") + name +
                " must be a whole number from 0 to 255";
        return false;
    }

    out = static_cast<uint8_t>(value.integer);
    return true;
}

HttpResponse handle_load_program(const HttpRequest &request)
{
    std::string content_type = to_lower(request.header("content-type"));
    if (content_type.rfind("application/json", 0) != 0)
    {
        return json_error(415, "Content-Type must be application/json");
    }

    JsonObject body;
    try
    {
        body = FlatJsonParser(request.body).parse_object();
    }
    catch (const std::invalid_argument &e)
    {
        return json_error(400, e.what());
    }

    auto program_field = body.find("program");
    if (program_field == body.end() ||
        program_field->second.type != JsonValue::Type::String)
    {
        return json_error(400, "Field \"program\" must be a string");
    }

    const ProgramInfo *program = find_program(program_field->second.text);
    if (program == nullptr)
    {
        return json_error(400, "Unknown program: " +
                                   program_field->second.text);
    }

    uint8_t a = 0;
    uint8_t b = 0;
    std::string error;
    if (!read_byte_field(body, "a", true, a, error) ||
        !read_byte_field(body, "b", program->needs_second_input, b, error))
    {
        return json_error(400, error);
    }
    if (!program->needs_second_input)
    {
        b = 0;
    }

    try
    {
        session.load(*program, a, b);
    }
    catch (const std::exception &e)
    {
        return json_error(500, std::string("Could not load program: ") +
                                   e.what());
    }

    return state_response();
}

// Returns an error response if the CPU cannot execute, otherwise status 0.
HttpResponse check_can_execute()
{
    if (!session.has_program())
        return json_error(409, "No program loaded. Select a program first.",
                          true);
    if (session.has_error())
        return json_error(409, "CPU stopped with an error. Reset to try again.",
                          true);
    if (session.halted())
        return json_error(409, "CPU is halted. Reset to run the program again.",
                          true);

    HttpResponse ok;
    ok.status = 0;
    return ok;
}

HttpResponse handle_step()
{
    HttpResponse blocked = check_can_execute();
    if (blocked.status != 0)
        return blocked;

    if (!session.execute_one())
        return json_error(422, session.error(), true);
    return state_response();
}

HttpResponse handle_run()
{
    HttpResponse blocked = check_can_execute();
    if (blocked.status != 0)
        return blocked;

    if (!session.run_to_halt())
        return json_error(422, session.error(), true);
    return state_response();
}

HttpResponse handle_programs()
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
    return json_response(200, out.str());
}

bool read_file(const std::string &path, std::string &content)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad())
    {
        return false;
    }
    content = buffer.str();
    return true;
}

HttpResponse serve_static(const std::string &file_name,
                          const std::string &content_type)
{
    HttpResponse response;
    std::string path = web_root + "/" + file_name;
    if (!read_file(path, response.body))
    {
        std::cerr << "Error: could not read " << path << '\n';
        response.status = 500;
        response.body = "500 - Could not read " + path;
        return response;
    }
    response.content_type = content_type;
    return response;
}

HttpResponse method_not_allowed(const std::string &allow)
{
    HttpResponse response = json_error(405, "Method not allowed. Use " + allow);
    response.allow = allow;
    return response;
}

HttpResponse route(const HttpRequest &request)
{
    if (!is_allowed_host(request.header("host")))
    {
        return json_error(403, "Requests are only accepted for 127.0.0.1:" +
                                   std::to_string(server_port));
    }

    const std::string &path = request.path;
    const std::string &method = request.method;

    if (path.rfind("/api/", 0) == 0)
    {
        if (method == "POST")
        {
            std::string origin = request.header("origin");
            if (!origin.empty() && !is_allowed_origin(origin))
            {
                return json_error(403, "Cross-origin requests are not allowed");
            }
        }

        if (path == "/api/state")
            return method == "GET" ? state_response() : method_not_allowed("GET");
        if (path == "/api/programs")
            return method == "GET" ? handle_programs() : method_not_allowed("GET");
        if (path == "/api/program")
            return method == "POST" ? handle_load_program(request)
                                    : method_not_allowed("POST");
        if (path == "/api/step")
            return method == "POST" ? handle_step() : method_not_allowed("POST");
        if (path == "/api/run")
            return method == "POST" ? handle_run() : method_not_allowed("POST");
        if (path == "/api/reset")
        {
            if (method != "POST")
                return method_not_allowed("POST");
            session.reset();
            return state_response();
        }
        return json_error(404, "Unknown API endpoint: " + path);
    }

    struct StaticFile
    {
        const char *url;
        const char *file;
        const char *type;
    };
    static const StaticFile files[] = {
        {"/", "index.html", "text/html; charset=utf-8"},
        {"/index.html", "index.html", "text/html; charset=utf-8"},
        {"/style.css", "style.css", "text/css; charset=utf-8"},
        {"/app.js", "app.js", "application/javascript; charset=utf-8"},
    };

    for (const StaticFile &file : files)
    {
        if (path == file.url)
        {
            if (method != "GET")
            {
                HttpResponse response;
                response.status = 405;
                response.body = "405 - Method Not Allowed";
                response.allow = "GET";
                return response;
            }
            return serve_static(file.file, file.type);
        }
    }

    HttpResponse not_found;
    not_found.status = 404;
    not_found.body = "404 - Not Found";
    return not_found;
}

void handle_client(int client_fd)
{
    timeval timeout{};
    timeout.tv_sec = SOCKET_TIMEOUT_SECONDS;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
    int no_sigpipe = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE,
               &no_sigpipe, sizeof(no_sigpipe));
#endif

    HttpRequest request;
    int error_status = 0;
    std::string error_message;
    HttpResponse response;

    if (!read_request(client_fd, request, error_status, error_message))
    {
        if (error_status == 0)
        {
            return; // client disconnected
        }
        response = json_error(error_status, error_message);
    }
    else
    {
        try
        {
            response = route(request);
        }
        catch (const std::exception &e)
        {
            response = json_error(500, std::string("Internal error: ") +
                                           e.what());
        }
    }

    send_response(client_fd, response);
    std::cout << (request.method.empty() ? "-" : request.method) << ' '
              << (request.path.empty() ? "-" : request.path) << " -> "
              << response.status << std::endl;
}

std::string directory_of(const std::string &path)
{
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

bool find_web_root(const char *argv0)
{
    // The current directory first, then next to the executable, then the
    // project root when the executable is in build/.
    std::vector<std::string> candidates = {"web"};
    if (argv0 != nullptr)
    {
        candidates.push_back(directory_of(argv0) + "/web");
        candidates.push_back(directory_of(argv0) + "/../web");
    }

    for (const std::string &candidate : candidates)
    {
        std::ifstream index(candidate + "/index.html");
        if (index)
        {
            web_root = candidate;
            return true;
        }
    }
    return false;
}

bool parse_arguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc)
        {
            std::string value = argv[++i];
            if (value.empty() || value.size() > 5 ||
                value.find_first_not_of("0123456789") != std::string::npos)
            {
                std::cerr << "Invalid port: " << value << '\n';
                return false;
            }
            int port = std::stoi(value);
            if (port < 1 || port > 65535)
            {
                std::cerr << "Port must be between 1 and 65535.\n";
                return false;
            }
            server_port = port;
        }
        else
        {
            std::cerr << "Usage: " << argv[0] << " [--port PORT]\n";
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    if (!parse_arguments(argc, argv))
    {
        return 1;
    }

    // A client closing its connection early must not terminate the server.
    std::signal(SIGPIPE, SIG_IGN);

    if (!find_web_root(argc > 0 ? argv[0] : nullptr))
    {
        std::cerr << "Error: could not find web/index.html.\n"
                  << "Run ./web_server from the project directory "
                     "that contains the web/ folder.\n";
        return 1;
    }

    const char *required_files[] = {"index.html", "style.css", "app.js"};
    for (const char *name : required_files)
    {
        std::ifstream file(web_root + "/" + name);
        if (!file)
        {
            std::cerr << "Error: missing required file " << web_root << '/'
                      << name << '\n';
            return 1;
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        std::cerr << "Failed to create socket: " << std::strerror(errno)
                  << '\n';
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "Warning: could not set SO_REUSEADDR: "
                  << std::strerror(errno) << '\n';
    }

    // Bind to the loopback interface only, so the API is not reachable
    // from other machines.
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(server_port));

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) < 0)
    {
        std::cerr << "Failed to bind port " << server_port << ": "
                  << std::strerror(errno)
                  << ". Is another server using it?\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0)
    {
        std::cerr << "Failed to start listening: " << std::strerror(errno)
                  << '\n';
        close(server_fd);
        return 1;
    }

    std::cout << "CPU emulator web server running at:\n"
              << "http://127.0.0.1:" << server_port << '\n'
              << "Serving files from: " << web_root << "/\n"
              << "Press Ctrl+C to stop." << std::endl;

    while (true)
    {
        int client_fd = accept(server_fd, nullptr, nullptr);

        if (client_fd < 0)
        {
            if (errno != EINTR)
            {
                std::cerr << "accept() failed: " << std::strerror(errno)
                          << '\n';
            }
            continue;
        }

        handle_client(client_fd);
        close(client_fd);
    }
}
