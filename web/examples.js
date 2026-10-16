"use strict";

// Example programs for the Assembly Editor. Each one is tested in
// tests/test_frontend.mjs, which checks the value of register A after Run.
const ASM_EXAMPLES = [
  {
    id: "countdown",
    name: "Count down (loop)",
    expectedA: 0,
    source: `; Count down from 5 to 1 and store each value at address 0xB0.
; Click Load Program, then press Step to watch the loop.

        LOAD 5          ; A = 5
loop:   STORE 0xB0      ; memory[0xB0] = A
        SUB 1           ; A = A - 1
        JZ done         ; JZ jumps when A is 0
        JMP loop        ; otherwise repeat
done:   HALT
`,
  },
  {
    id: "multiply",
    name: "Multiply 6 × 7",
    expectedA: 42,
    source: `; Multiply 6 x 7 by adding 6 seven times.
; 'count' and 'result' are data bytes after the code.

        LOAD 7
        STORE count     ; count = 7
loop:   LOAD_MEM result
        ADD 6
        STORE result    ; result = result + 6
        LOAD_MEM count
        SUB 1
        STORE count     ; count = count - 1
        JZ done         ; stop when count is 0
        JMP loop
done:   LOAD_MEM result ; A = 42
        HALT

count:  .byte 0
result: .byte 0
`,
  },
  {
    id: "compare",
    name: "Compare two numbers",
    expectedA: 1,
    source: `; Compare A and B. Result in A: 1 = A < B, 2 = A > B, 3 = A == B

        LOAD 12         ; A = 12
        LOAD_B 30       ; B = 30
        CMP             ; sets the Zero, Less and Greater flags
        JL less
        JG greater
        LOAD 3          ; A == B
        HALT
less:   LOAD 1
        HALT
greater: LOAD 2
        HALT
`,
  },
  {
    id: "subroutine",
    name: "Subroutine and stack",
    expectedA: 42,
    source: `; CALL a subroutine that doubles A, then use PUSH and POP.

        LOAD 21
        CALL double     ; saves the return address on the stack
        PUSH            ; push A (42) onto the stack
        LOAD 0
        POP             ; pop it back: A = 42
        HALT

double: MOV_B_A         ; B = A
        ADD_B           ; A = A + B
        RET             ; continue after the CALL
`,
  },
];
