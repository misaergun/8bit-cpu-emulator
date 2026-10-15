# 8-Bit CPU Emulator

A small 8-bit CPU emulator written in C++17. It simulates a CPU with two general-purpose registers, comparison flags, 256 bytes of memory, a hardware-style stack, and subroutine calls. You run it from the command line: pick a built-in program, enter its inputs, and watch it execute one instruction at a time or all at once.

This is a learning project about CPU architecture, the fetch–decode–execute cycle, and testing low-level code.

## Features

- **28 instructions** covering data movement, arithmetic, bitwise logic, shifts, comparisons, conditional and unconditional jumps, stack operations, and subroutine calls
- **Two 8-bit registers** (`A` and `B`), a 16-bit program counter, and a stack pointer
- **Three status flags**: zero, less, and greater
- **256 bytes of memory**, split into a program/data region and a stack region
- **Stack support** with `PUSH`, `POP`, `CALL`, and `RET`, including overflow and underflow detection
- **Two execution modes**: step mode (one instruction per Enter key press) and run mode (runs until `HALT`)
- **CPU state output** after every instruction: registers, program counter, flags, and halt status
- **Memory inspection**: read a single address, or dump a range of addresses in hex and decimal
- **Input validation** for every prompt, with clean exit on end of input (EOF)
- **Unit and integration tests**, plus one script that builds and runs both

## Instruction Set

Each opcode is one byte. Instructions that take an operand read one extra byte right after the opcode.

| Opcode | Mnemonic   | Operand | Description                                             |
| -----: | ---------- | ------- | ------------------------------------------------------- |
|      1 | `LOAD`     | value   | Load an immediate value into register A                 |
|      2 | `ADD`      | value   | Add an immediate value to register A                    |
|      3 | `SUB`      | value   | Subtract an immediate value from register A             |
|      4 | `JMP`      | address | Jump to address unconditionally                         |
|      5 | `JZ`       | address | Jump if register A is zero                              |
|      6 | `LOAD_MEM` | address | Load the byte at a memory address into register A       |
|      7 | `STORE`    | address | Store register A at a memory address                    |
|      8 | `LOAD_B`   | value   | Load an immediate value into register B                 |
|      9 | `STORE_B`  | address | Store register B at a memory address                    |
|     10 | `MOV_A_B`  | —       | Copy register B into register A                         |
|     11 | `MOV_B_A`  | —       | Copy register A into register B                         |
|     12 | `ADD_B`    | —       | Add register B to register A                            |
|     13 | `SUB_B`    | —       | Subtract register B from register A                     |
|     14 | `CMP`      | —       | Compare A with B and set the zero, less, and greater flags |
|     15 | `JE`       | address | Jump if the zero flag is set (equal)                    |
|     16 | `JL`       | address | Jump if the less flag is set (A < B)                    |
|     17 | `JG`       | address | Jump if the greater flag is set (A > B)                 |
|     18 | `AND`      | —       | A = A AND B                                             |
|     19 | `OR`       | —       | A = A OR B                                              |
|     20 | `XOR`      | —       | A = A XOR B                                             |
|     21 | `NOT`      | —       | A = NOT A                                               |
|     22 | `SHL`      | —       | Shift register A left by one bit                        |
|     23 | `SHR`      | —       | Shift register A right by one bit                       |
|     24 | `PUSH`     | —       | Push register A onto the stack                          |
|     25 | `POP`      | —       | Pop the top of the stack into register A                |
|     26 | `CALL`     | address | Push the return address and jump to a subroutine        |
|     27 | `RET`      | —       | Return from a subroutine                                |
|    255 | `HALT`     | —       | Stop execution                                          |

Opcode values are defined as constants in the `CPU` class in [`cpu.h`](cpu.h). Their names, used for the execution trace, are in [`instruction_names.h`](instruction_names.h).

## Project Structure

```text
8bit-cpu-emulator/
├── .gitignore
├── README.md
├── cpu.h                 # CPU class: registers, flags, memory, stack, instruction execution
├── program_loader.h      # load_program(): copies a program into memory
├── instruction_names.h   # Maps opcodes to mnemonics for the execution trace
├── main.cpp              # Interactive emulator: program menu, execution modes, memory inspection
├── test_cpu.cpp          # Unit tests for the CPU, loader, and instruction names
├── test_main.sh          # Integration tests for the interactive emulator
├── run_test.py           # Runs ./cpu_emulator with piped input and a 3-second timeout
└── run_all_tests.sh      # Builds everything and runs all tests
```

## Requirements

- A C++17 compiler. The build scripts use `clang++`; `g++` with `-std=c++17` should also work.
- Bash, to run the test scripts.
- Python 3, used by the integration tests to run the emulator with a timeout.

The project uses only the C++ standard library. It has no external dependencies and no build system.

## Build and Run

Compile the emulator:

```bash
clang++ -std=c++17 main.cpp -o cpu_emulator
```

Start it:

```bash
./cpu_emulator
```

## Usage

The emulator is interactive and guides you through each step.

### 1. Choose a program

```text
=== 8-Bit CPU Emulator ===
Choose a program:
1 - Comparison
2 - Addition
3 - Subtraction
4 - Bitwise AND
5 - Bitwise OR
6 - Bitwise XOR
7 - Bitwise NOT
8 - Shift Left (SHL)
9 - Shift Right (SHR)
```

### 2. Enter input values

Each value must be an integer from 0 to 255.

- Programs **1–6** ask for two numbers. The first is loaded into register A and the second into register B.
- Programs **7–9** ask for one number, which is loaded into register A.

Invalid or out-of-range input shows an error and asks again.

### 3. Choose an execution mode

- `s` — **Step mode**: runs one instruction, prints the CPU state, and waits for Enter before the next one.
- `r` — **Run mode**: runs every instruction until `HALT`, printing the CPU state after each one.

Both modes print each instruction before running it, for example:

```text
Executing: ADD_B (address 4)
Register A: 15
Register B: 5
Program Counter: 5
Zero flag: false
Less flag: false
Greater flag: false
CPU halted: no
------------------------
```

### 4. Inspect memory (optional)

After the program halts, you can:

1. Read a single address (0–255). Its value is shown in decimal and hex.
2. Dump a range of memory by giving a start address and a byte count. The range must stay inside the 256-byte memory.

```text
Memory Dump
Address   Hex   Decimal
-----------------------
0x00      01      1
0x01      0A     10
```

### 5. See the result

The emulator prints the final value of register A. For the comparison program, register A holds a result code:

| Register A | Meaning |
| ---------: | ------- |
|          1 | A < B   |
|          2 | A > B   |
|          3 | A == B  |

You can then run another program (`y`), which resets the CPU first, or exit (`n`). If input ends (EOF), the emulator exits.

## Testing

### Run all tests

```bash
./run_all_tests.sh
```

This script:

1. Builds `cpu_emulator` from `main.cpp`
2. Builds and runs the unit tests in `test_cpu.cpp`
3. Runs the integration tests in `test_main.sh`

It stops at the first failure.

### Unit tests

[`test_cpu.cpp`](test_cpu.cpp) tests the `CPU` class directly. It covers:

- Every instruction, including how each one affects registers and flags
- 8-bit wraparound on addition, subtraction, and left shift
- Conditional jumps when taken and not taken, and rejection of jump targets outside program memory
- Stack behavior: `PUSH`/`POP` order, overflow, underflow, nested `CALL`/`RET`, `RET` without `CALL`, and preventing `POP` from consuming a return address
- Memory protection: `STORE` and `STORE_B` cannot write into the stack region
- Error cases: unknown opcodes, an operand that would fall past the end of program memory, out-of-range reads, and runaway loops
- `reset()`, the `load_program()` size limit, and the mnemonic for every opcode

Build and run them on their own:

```bash
clang++ -std=c++17 test_cpu.cpp -o test_cpu
./test_cpu
```

Each check prints `PASS: ...`. On the first failure, the program prints `FAIL: ...` and exits with a non-zero status.

### Integration tests

[`test_main.sh`](test_main.sh) tests the interactive emulator end to end. Each test pipes scripted input through [`run_test.py`](run_test.py), which runs `./cpu_emulator` with a 3-second timeout. The test then checks both the output and the exit code. The tests cover:

- Invalid program choice and invalid number input
- EOF while waiting for the first or second number
- Correct final results for addition, subtraction, AND, OR, XOR, NOT, SHL, and SHR

The emulator must be built first:

```bash
clang++ -std=c++17 main.cpp -o cpu_emulator
./test_main.sh
```

## Technical Details

### Memory layout

Memory is 256 bytes, addressed from `0x00` to `0xFF`:

| Range                | Size     | Purpose                    |
| -------------------- | -------- | -------------------------- |
| `0x00`–`0xBF` (0–191)   | 192 bytes | Program code and data |
| `0xC0`–`0xFF` (192–255) | 64 bytes  | Stack                 |

- Programs are loaded starting at address 0. `load_program()` rejects programs longer than 192 bytes.
- Any address can be read. `STORE` and `STORE_B` can only write to the program/data region.
- The program counter and all jump and call targets must stay inside the program/data region.

### Registers

| Register          | Width  | Description                                |
| ----------------- | ------ | ------------------------------------------ |
| A                 | 8-bit  | Accumulator. Holds the result of arithmetic, logic, and shift operations |
| B                 | 8-bit  | Second operand for register-based instructions |
| Program counter   | 16-bit | Address of the next byte to fetch          |
| Stack pointer     | 16-bit | Starts at 256 and moves down as values are pushed |

Arithmetic and shifts wrap around modulo 256. For example, `255 + 1` gives `0`, and `0 - 1` gives `255`.

### Flags

| Flag    | Set by                                              |
| ------- | --------------------------------------------------- |
| Zero    | `ADD`, `SUB`, `ADD_B`, `SUB_B`, `AND`, `OR`, `XOR`, `NOT`, `SHL`, `SHR`, and `POP` (result is 0); `CMP` (A == B) |
| Less    | `CMP` only (A < B)                                  |
| Greater | `CMP` only (A > B)                                  |

Load and move instructions do not change any flags. `JE` checks the zero flag, while `JZ` checks register A directly.

### Stack

- The stack grows downward from the top of memory into the 64-byte stack region.
- `PUSH` stores register A at the next lower address. `POP` reads it back and updates the zero flag.
- `CALL` pushes a 2-byte return address (low byte first) and jumps to the target. `RET` pops it and continues from there.
- The CPU keeps track of where each return address is stored. `RET` fails if the top of the stack is not a return address (for example, after an unmatched `PUSH`), and `POP` cannot read a return address.

### Error handling

Invalid operations throw C++ exceptions (`std::runtime_error` or `std::out_of_range`) instead of failing silently. These include:

- Unknown opcodes
- Writing outside the allowed memory region
- Reading past the end of memory
- The program counter, or a jump or call target, leaving the program/data region
- Stack overflow and stack underflow
- Loading a program larger than 192 bytes
- Going over 1,000 instructions in `CPU::run()`, which catches infinite loops

The interactive emulator in `main.cpp` runs the CPU with its own `step()` loop rather than `CPU::run()`, so the 1,000-instruction limit applies only when `run()` is called directly, as in the unit tests.
