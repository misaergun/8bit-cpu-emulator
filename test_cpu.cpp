
#include <iostream>
#include <stdexcept>
#include "cpu.h"
#include <string>

int main() {
    // Test 1: Normal program
    CPU cpu;
    cpu.write_memory(0, CPU::OP_LOAD);
    cpu.write_memory(1, 5);
    cpu.write_memory(2, CPU::OP_ADD);
    cpu.write_memory(3, 3);
    cpu.write_memory(4, CPU::OP_HALT);
    cpu.run();

    if (cpu.register_a != 8) {
        std::cerr << "FAIL: Normal program" << std::endl;
        return 1;
    }
    std::cout << "PASS: Normal program" << std::endl;

    // Test 2: Memory boundary
    try {
        cpu.read_memory(256);
        std::cerr << "FAIL: Memory boundary check" << std::endl;
        return 1;
    } catch (const std::out_of_range&) {
        std::cout << "PASS: Memory boundary check" << std::endl;
    }

    // Test 3: Unknown instruction
    CPU invalid_cpu;
    invalid_cpu.write_memory(0, 99);

    try {
        invalid_cpu.run();
        std::cerr << "FAIL: Unknown instruction" << std::endl;
        return 1;
    } catch (const std::runtime_error&) {
        std::cout << "PASS: Unknown instruction" << std::endl;
    }

    // Test 4: Missing parameter
    CPU incomplete_cpu;
    incomplete_cpu.write_memory(255, CPU::OP_LOAD);
    incomplete_cpu.program_counter = 255;

    try {
        incomplete_cpu.run();
        std::cerr << "FAIL: Missing parameter" << std::endl;
        return 1;
    } catch (const std::out_of_range&) {
        std::cout << "PASS: Missing parameter" << std::endl;
    }

    // Test 5: Unconditional jump
    CPU jump_cpu;
    jump_cpu.write_memory(0, CPU::OP_JMP);
    jump_cpu.write_memory(1, 4);
    jump_cpu.write_memory(2, CPU::OP_LOAD);
    jump_cpu.write_memory(3, 99);
    jump_cpu.write_memory(4, CPU::OP_HALT);
    jump_cpu.run();

    if (jump_cpu.register_a != 0 || !jump_cpu.halted) {
        std::cerr << "FAIL: Unconditional jump" << std::endl;
        return 1;
    }
    std::cout << "PASS: Unconditional jump" << std::endl;

    // Test 6: Jump when register A is zero
    CPU zero_cpu;
    zero_cpu.write_memory(0, CPU::OP_LOAD);
    zero_cpu.write_memory(1, 0);
    zero_cpu.write_memory(2, CPU::OP_JZ);
    zero_cpu.write_memory(3, 6);
    zero_cpu.write_memory(4, CPU::OP_LOAD);
    zero_cpu.write_memory(5, 99);
    zero_cpu.write_memory(6, CPU::OP_HALT);
    zero_cpu.run();

    if (zero_cpu.register_a != 0 || !zero_cpu.halted) {
        std::cerr << "FAIL: JZ when register A is zero" << std::endl;
        return 1;
    }
    std::cout << "PASS: JZ when register A is zero" << std::endl;

    // Test 7: Do not jump when register A is nonzero
    CPU nonzero_cpu;
    nonzero_cpu.write_memory(0, CPU::OP_LOAD);
    nonzero_cpu.write_memory(1, 5);
    nonzero_cpu.write_memory(2, CPU::OP_JZ);
    nonzero_cpu.write_memory(3, 6);
    nonzero_cpu.write_memory(4, CPU::OP_LOAD);
    nonzero_cpu.write_memory(5, 99);
    nonzero_cpu.write_memory(6, CPU::OP_HALT);
    nonzero_cpu.run();

    if (nonzero_cpu.register_a != 99 || !nonzero_cpu.halted) {
        std::cerr << "FAIL: JZ when register A is nonzero" << std::endl;
        return 1;
    }
    std::cout << "PASS: JZ when register A is nonzero" << std::endl;

    
    // Test 8: Loop until register A reaches zero
    CPU loop_cpu;
    loop_cpu.write_memory(0, CPU::OP_LOAD);
    loop_cpu.write_memory(1, 3);
    loop_cpu.write_memory(2, CPU::OP_SUB);
    loop_cpu.write_memory(3, 1);
    loop_cpu.write_memory(4, CPU::OP_JZ);
    loop_cpu.write_memory(5, 8);
    loop_cpu.write_memory(6, CPU::OP_JMP);
    loop_cpu.write_memory(7, 2);
    loop_cpu.write_memory(8, CPU::OP_HALT);

    loop_cpu.run();

    if (loop_cpu.register_a != 0 || !loop_cpu.halted) {
        std::cerr << "FAIL: Loop until zero" << std::endl;
        return 1;
    }

    std::cout << "PASS: Loop until zero" << std::endl;


    // Test 9: Detect an infinite loop
    CPU infinite_cpu;
    infinite_cpu.write_memory(0, CPU::OP_JMP);
    infinite_cpu.write_memory(1, 0);

    try {
        infinite_cpu.run();
        std::cerr << "FAIL: Infinite loop detection" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) !=
            "Maximum instruction limit reached") {
            std::cerr << "FAIL: Unexpected error: "
                      << error.what() << std::endl;
            return 1;
        }

        std::cout << "PASS: Infinite loop detection" << std::endl;
    }

    
    // Test 10: Reset CPU state
    CPU reset_cpu;

    reset_cpu.write_memory(0, CPU::OP_LOAD);
    reset_cpu.write_memory(1, 42);
    reset_cpu.write_memory(2, CPU::OP_LOAD_B);
    reset_cpu.write_memory(3, 10);
    reset_cpu.write_memory(4, CPU::OP_CMP);
    reset_cpu.write_memory(5, CPU::OP_HALT);

    reset_cpu.run();

    reset_cpu.reset();

    if (reset_cpu.register_a != 0 ||
        reset_cpu.register_b != 0 ||
        reset_cpu.program_counter != 0 ||
        reset_cpu.halted ||
        reset_cpu.zero_flag ||
        reset_cpu.less_flag ||
        reset_cpu.greater_flag ||
        reset_cpu.read_memory(0) != 0 ||
        reset_cpu.read_memory(1) != 0 ||
        reset_cpu.read_memory(2) != 0 ||
        reset_cpu.read_memory(3) != 0 ||
        reset_cpu.read_memory(4) != 0 ||
        reset_cpu.read_memory(5) != 0) {
        std::cerr << "FAIL: Reset CPU state" << std::endl;
        return 1;
    }

    std::cout << "PASS: Reset CPU state" << std::endl;



    // Test 11: Load value from memory
    CPU load_mem_cpu;
    load_mem_cpu.write_memory(20, 42);
    load_mem_cpu.write_memory(0, CPU::OP_LOAD_MEM);
    load_mem_cpu.write_memory(1, 20);
    load_mem_cpu.write_memory(2, CPU::OP_HALT);

    load_mem_cpu.run();

    if (load_mem_cpu.register_a != 42) {
        std::cerr << "FAIL: Load from memory" << std::endl;
        return 1;
    }

    std::cout << "PASS: Load from memory" << std::endl;


    // Test 12: Store register A in memory
    CPU store_cpu;
    store_cpu.write_memory(0, CPU::OP_LOAD);
    store_cpu.write_memory(1, 73);
    store_cpu.write_memory(2, CPU::OP_STORE);
    store_cpu.write_memory(3, 30);
    store_cpu.write_memory(4, CPU::OP_HALT);

    store_cpu.run();

    if (store_cpu.read_memory(30) != 73) {
        std::cerr << "FAIL: Store to memory" << std::endl;
        return 1;
    }

    std::cout << "PASS: Store to memory" << std::endl;


    // Test 13: Load a value into register B
    CPU load_b_cpu;
    load_b_cpu.write_memory(0, CPU::OP_LOAD_B);
    load_b_cpu.write_memory(1, 55);
    load_b_cpu.write_memory(2, CPU::OP_HALT);
    load_b_cpu.run();

    if (load_b_cpu.register_b != 55) {
        std::cerr << "FAIL: Load register B" << std::endl;
        return 1;
    }

    std::cout << "PASS: Load register B" << std::endl;

    // Test 14: Store register B in memory
    CPU store_b_cpu;
    store_b_cpu.write_memory(0, CPU::OP_LOAD_B);
    store_b_cpu.write_memory(1, 77);
    store_b_cpu.write_memory(2, CPU::OP_STORE_B);
    store_b_cpu.write_memory(3, 30);
    store_b_cpu.write_memory(4, CPU::OP_HALT);
    store_b_cpu.run();

    if (store_b_cpu.read_memory(30) != 77) {
        std::cerr << "FAIL: Store register B" << std::endl;
        return 1;
    }

    std::cout << "PASS: Store register B" << std::endl;

    // Test 15: Reset register B
    CPU reset_b_cpu;
    reset_b_cpu.register_b = 99;
    reset_b_cpu.reset();

    if (reset_b_cpu.register_b != 0) {
        std::cerr << "FAIL: Reset register B" << std::endl;
        return 1;
    }

    std::cout << "PASS: Reset register B" << std::endl;


    // Test 16: Copy register B to register A
    CPU mov_a_b_cpu;
    mov_a_b_cpu.register_a = 10;
    mov_a_b_cpu.register_b = 5;
    mov_a_b_cpu.write_memory(0, CPU::OP_MOV_A_B);
    mov_a_b_cpu.write_memory(1, CPU::OP_HALT);
    mov_a_b_cpu.run();

    if (mov_a_b_cpu.register_a != 5 ||
        mov_a_b_cpu.register_b != 5) {
        std::cerr << "FAIL: Move B to A" << std::endl;
        return 1;
    }

    std::cout << "PASS: Move B to A" << std::endl;

    // Test 17: Copy register A to register B
    CPU mov_b_a_cpu;
    mov_b_a_cpu.register_a = 25;
    mov_b_a_cpu.register_b = 7;
    mov_b_a_cpu.write_memory(0, CPU::OP_MOV_B_A);
    mov_b_a_cpu.write_memory(1, CPU::OP_HALT);
    mov_b_a_cpu.run();

    if (mov_b_a_cpu.register_a != 25 ||
        mov_b_a_cpu.register_b != 25) {
        std::cerr << "FAIL: Move A to B" << std::endl;
        return 1;
    }

    std::cout << "PASS: Move A to B" << std::endl;

    // Test 18: Add register B to register A
    CPU add_b_cpu;
    add_b_cpu.register_a = 10;
    add_b_cpu.register_b = 5;
    add_b_cpu.write_memory(0, CPU::OP_ADD_B);
    add_b_cpu.write_memory(1, CPU::OP_HALT);
    add_b_cpu.run();

    if (add_b_cpu.register_a != 15 ||
        add_b_cpu.register_b != 5) {
        std::cerr << "FAIL: Add register B to A" << std::endl;
        return 1;
    }

    std::cout << "PASS: Add register B to A" << std::endl;

    // Test 19: Subtract register B from register A
    CPU sub_b_cpu;
    sub_b_cpu.register_a = 10;
    sub_b_cpu.register_b = 3;
    sub_b_cpu.write_memory(0, CPU::OP_SUB_B);
    sub_b_cpu.write_memory(1, CPU::OP_HALT);
    sub_b_cpu.run();

    if (sub_b_cpu.register_a != 7 ||
        sub_b_cpu.register_b != 3) {
        std::cerr << "FAIL: Subtract register B from A" << std::endl;
        return 1;
    }

    std::cout << "PASS: Subtract register B from A" << std::endl;

    // Test 20: Compare equal registers
    CPU cmp_cpu;
    cmp_cpu.register_a = 10;
    cmp_cpu.register_b = 10;
    cmp_cpu.write_memory(0, CPU::OP_CMP);
    cmp_cpu.write_memory(1, CPU::OP_HALT);
    cmp_cpu.run();

    if (!cmp_cpu.zero_flag ||
        cmp_cpu.register_a != 10 ||
        cmp_cpu.register_b != 10) {
        std::cerr << "FAIL: Compare equal registers" << std::endl;
        return 1;
    }

    std::cout << "PASS: Compare equal registers" << std::endl;

    // Test 21: Jump when registers are equal
    CPU je_equal_cpu;
    je_equal_cpu.register_a = 10;
    je_equal_cpu.register_b = 10;
    je_equal_cpu.write_memory(0, CPU::OP_CMP);
    je_equal_cpu.write_memory(1, CPU::OP_JE);
    je_equal_cpu.write_memory(2, 6);
    je_equal_cpu.write_memory(3, CPU::OP_LOAD);
    je_equal_cpu.write_memory(4, 99);
    je_equal_cpu.write_memory(6, CPU::OP_HALT);
    je_equal_cpu.run();

    if (je_equal_cpu.register_a != 10 ||
        !je_equal_cpu.halted) {
        std::cerr << "FAIL: JE when registers are equal" << std::endl;
        return 1;
    }

    std::cout << "PASS: JE when registers are equal" << std::endl;

    // Test 22: Do not jump when registers are different
    CPU je_different_cpu;
    je_different_cpu.register_a = 10;
    je_different_cpu.register_b = 5;
    je_different_cpu.write_memory(0, CPU::OP_CMP);
    je_different_cpu.write_memory(1, CPU::OP_JE);
    je_different_cpu.write_memory(2, 6);
    je_different_cpu.write_memory(3, CPU::OP_LOAD);
    je_different_cpu.write_memory(4, 99);
    je_different_cpu.write_memory(5, CPU::OP_HALT);
    je_different_cpu.write_memory(6, CPU::OP_HALT);
    je_different_cpu.run();

    if (je_different_cpu.register_a != 99 ||
        !je_different_cpu.halted) {
        std::cerr << "FAIL: JE when registers differ" << std::endl;
        return 1;
    }

    std::cout << "PASS: JE when registers differ" << std::endl;


    // Test 23: Compare less than and greater than
    CPU compare_cpu;

    compare_cpu.register_a = 3;
    compare_cpu.register_b = 5;
    compare_cpu.write_memory(0, CPU::OP_CMP);
    compare_cpu.write_memory(1, CPU::OP_HALT);
    compare_cpu.run();

    if (compare_cpu.zero_flag ||
        !compare_cpu.less_flag ||
        compare_cpu.greater_flag) {
        std::cerr << "FAIL: A less than B" << std::endl;
        return 1;
    }

    std::cout << "PASS: A less than B" << std::endl;

    compare_cpu.reset();
    compare_cpu.register_a = 8;
    compare_cpu.register_b = 5;
    compare_cpu.write_memory(0, CPU::OP_CMP);
    compare_cpu.write_memory(1, CPU::OP_HALT);
    compare_cpu.run();

    if (compare_cpu.zero_flag ||
        compare_cpu.less_flag ||
        !compare_cpu.greater_flag) {
        std::cerr << "FAIL: A greater than B" << std::endl;
        return 1;
    }

    std::cout << "PASS: A greater than B" << std::endl;

    
    // Test 25: Jump when A is less than B
    CPU jl_cpu;
    jl_cpu.register_a = 3;
    jl_cpu.register_b = 5;
    jl_cpu.write_memory(0, CPU::OP_CMP);
    jl_cpu.write_memory(1, CPU::OP_JL);
    jl_cpu.write_memory(2, 6);
    jl_cpu.write_memory(3, CPU::OP_LOAD);
    jl_cpu.write_memory(4, 99);
    jl_cpu.write_memory(6, CPU::OP_HALT);
    jl_cpu.run();

    if (jl_cpu.register_a != 3 || !jl_cpu.halted) {
        std::cerr << "FAIL: JL when A is less than B" << std::endl;
        return 1;
    }

    std::cout << "PASS: JL when A is less than B" << std::endl;

    // Test 26: Jump when A is greater than B
    CPU jg_cpu;
    jg_cpu.register_a = 8;
    jg_cpu.register_b = 5;
    jg_cpu.write_memory(0, CPU::OP_CMP);
    jg_cpu.write_memory(1, CPU::OP_JG);
    jg_cpu.write_memory(2, 6);
    jg_cpu.write_memory(3, CPU::OP_LOAD);
    jg_cpu.write_memory(4, 99);
    jg_cpu.write_memory(6, CPU::OP_HALT);
    jg_cpu.run();

    if (jg_cpu.register_a != 8 || !jg_cpu.halted) {
        std::cerr << "FAIL: JG when A is greater than B" << std::endl;
        return 1;
    }

    std::cout << "PASS: JG when A is greater than B" << std::endl;

    
    // Test 27: Addition program (12 + 4 = 16)
    CPU addition_test;
    addition_test.write_memory(0, CPU::OP_LOAD);
    addition_test.write_memory(1, 12);
    addition_test.write_memory(2, CPU::OP_LOAD_B);
    addition_test.write_memory(3, 4);
    addition_test.write_memory(4, CPU::OP_ADD_B);
    addition_test.write_memory(5, CPU::OP_HALT);
    addition_test.run();

    if (addition_test.register_a != 16 ||
        !addition_test.halted) {
        std::cerr << "FAIL: Addition program (12 + 4)" << std::endl;
        return 1;
    }

    std::cout << "PASS: Addition program (12 + 4)" << std::endl;


    // Test 28: Subtraction program (12 - 4 = 8)
    CPU subtraction_test;
    subtraction_test.write_memory(0, CPU::OP_LOAD);
    subtraction_test.write_memory(1, 12);
    subtraction_test.write_memory(2, CPU::OP_LOAD_B);
    subtraction_test.write_memory(3, 4);
    subtraction_test.write_memory(4, CPU::OP_SUB_B);
    subtraction_test.write_memory(5, CPU::OP_HALT);
    subtraction_test.run();

    if (subtraction_test.register_a != 8 ||
        !subtraction_test.halted) {
        std::cerr << "FAIL: Subtraction program (12 - 4)" << std::endl;
        return 1;
    }

    std::cout << "PASS: Subtraction program (12 - 4)" << std::endl;


    // Test 29: Equality comparison program (7 == 7)
    CPU equal_program_test;
    const uint8_t equal_program[] = {
        CPU::OP_LOAD, 7,
        CPU::OP_LOAD_B, 7,
        CPU::OP_CMP,
        CPU::OP_JL, 12,
        CPU::OP_JG, 16,
        CPU::OP_JE, 20,
        CPU::OP_HALT,
        CPU::OP_LOAD, 1,
        CPU::OP_HALT,
        0,
        CPU::OP_LOAD, 2,
        CPU::OP_HALT,
        0,
        CPU::OP_LOAD, 3,
        CPU::OP_HALT
    };

    for (size_t i = 0; i < sizeof(equal_program); ++i) {
        equal_program_test.write_memory(
            static_cast<uint16_t>(i), equal_program[i]
        );
    }

    equal_program_test.run();

    if (equal_program_test.register_a != 3 ||
        !equal_program_test.halted) {
        std::cerr << "FAIL: Equality comparison program" << std::endl;
        return 1;
    }

    std::cout << "PASS: Equality comparison program" << std::endl;


    // Test 30: Less-than comparison program (4 < 12)
    CPU less_program_test;
    const uint8_t less_program[] = {
        CPU::OP_LOAD, 4,
        CPU::OP_LOAD_B, 12,
        CPU::OP_CMP,
        CPU::OP_JL, 12,
        CPU::OP_JG, 16,
        CPU::OP_JE, 20,
        CPU::OP_HALT,
        CPU::OP_LOAD, 1,
        CPU::OP_HALT,
        0,
        CPU::OP_LOAD, 2,
        CPU::OP_HALT,
        0,
        CPU::OP_LOAD, 3,
        CPU::OP_HALT
    };

    for (size_t i = 0; i < sizeof(less_program); ++i) {
        less_program_test.write_memory(
            static_cast<uint16_t>(i), less_program[i]
        );
    }

    less_program_test.run();

    if (less_program_test.register_a != 1 ||
        !less_program_test.halted) {
        std::cerr << "FAIL: Less-than comparison program" << std::endl;
        return 1;
    }

    std::cout << "PASS: Less-than comparison program" << std::endl;
    
    
    // Test 31: Bitwise AND
    CPU and_cpu;

    and_cpu.write_memory(0, CPU::OP_LOAD);
    and_cpu.write_memory(1, 12);
    and_cpu.write_memory(2, CPU::OP_LOAD_B);
    and_cpu.write_memory(3, 10);
    and_cpu.write_memory(4, CPU::OP_AND);
    and_cpu.write_memory(5, CPU::OP_HALT);

    and_cpu.run();

    if (and_cpu.register_a != 8 ||
        and_cpu.register_b != 10 ||
        !and_cpu.halted) {
        std::cerr << "FAIL: Bitwise AND" << std::endl;
        return 1;
    }

    std::cout << "PASS: Bitwise AND" << std::endl;
    
    // Test 32: Bitwise OR
    CPU or_cpu;

    or_cpu.write_memory(0, CPU::OP_LOAD);
    or_cpu.write_memory(1, 12);
    or_cpu.write_memory(2, CPU::OP_LOAD_B);
    or_cpu.write_memory(3, 10);
    or_cpu.write_memory(4, CPU::OP_OR);
    or_cpu.write_memory(5, CPU::OP_HALT);

    or_cpu.run();

    if (or_cpu.register_a != 14 ||
        or_cpu.register_b != 10 ||
        !or_cpu.halted) {
        std::cerr << "FAIL: Bitwise OR" << std::endl;
        return 1;
    }

    std::cout << "PASS: Bitwise OR" << std::endl;


    // Test 33: Bitwise XOR
    CPU xor_cpu;

    xor_cpu.write_memory(0, CPU::OP_LOAD);
    xor_cpu.write_memory(1, 12);
    xor_cpu.write_memory(2, CPU::OP_LOAD_B);
    xor_cpu.write_memory(3, 10);
    xor_cpu.write_memory(4, CPU::OP_XOR);
    xor_cpu.write_memory(5, CPU::OP_HALT);

    xor_cpu.run();

    if (xor_cpu.register_a != 6 ||
        xor_cpu.register_b != 10 ||
        !xor_cpu.halted) {
        std::cerr << "FAIL: Bitwise XOR" << std::endl;
        return 1;
    }

    std::cout << "PASS: Bitwise XOR" << std::endl;


    // Test 34: AND produces zero
    CPU and_zero_cpu;

    and_zero_cpu.write_memory(0, CPU::OP_LOAD);
    and_zero_cpu.write_memory(1, 12);
    and_zero_cpu.write_memory(2, CPU::OP_LOAD_B);
    and_zero_cpu.write_memory(3, 3);
    and_zero_cpu.write_memory(4, CPU::OP_AND);
    and_zero_cpu.write_memory(5, CPU::OP_HALT);

    and_zero_cpu.run();

    if (and_zero_cpu.register_a != 0 ||
        !and_zero_cpu.zero_flag ||
        !and_zero_cpu.halted) {
        std::cerr << "FAIL: AND produces zero" << std::endl;
        return 1;
    }

    std::cout << "PASS: AND produces zero" << std::endl;

    // Test 35: OR produces zero
    CPU or_zero_cpu;

    or_zero_cpu.write_memory(0, CPU::OP_LOAD);
    or_zero_cpu.write_memory(1, 0);
    or_zero_cpu.write_memory(2, CPU::OP_LOAD_B);
    or_zero_cpu.write_memory(3, 0);
    or_zero_cpu.write_memory(4, CPU::OP_OR);
    or_zero_cpu.write_memory(5, CPU::OP_HALT);

    or_zero_cpu.run();

    if (or_zero_cpu.register_a != 0 ||
        !or_zero_cpu.zero_flag ||
        !or_zero_cpu.halted) {
        std::cerr << "FAIL: OR produces zero" << std::endl;
        return 1;
    }

    std::cout << "PASS: OR produces zero" << std::endl;


    // Test 36: XOR produces zero
    CPU xor_zero_cpu;

    xor_zero_cpu.write_memory(0, CPU::OP_LOAD);
    xor_zero_cpu.write_memory(1, 5);
    xor_zero_cpu.write_memory(2, CPU::OP_LOAD_B);
    xor_zero_cpu.write_memory(3, 5);
    xor_zero_cpu.write_memory(4, CPU::OP_XOR);
    xor_zero_cpu.write_memory(5, CPU::OP_HALT);

    xor_zero_cpu.run();

    if (xor_zero_cpu.register_a != 0 ||
        !xor_zero_cpu.zero_flag ||
        !xor_zero_cpu.halted) {
        std::cerr << "FAIL: XOR produces zero" << std::endl;
        return 1;
    }

    std::cout << "PASS: XOR produces zero" << std::endl;


    // Test 37: Bitwise NOT
    CPU not_cpu;
    not_cpu.write_memory(0, CPU::OP_LOAD);
    not_cpu.write_memory(1, 12);
    not_cpu.write_memory(2, CPU::OP_NOT);
    not_cpu.write_memory(3, CPU::OP_HALT);
    not_cpu.run();

    if (not_cpu.register_a != 243 ||
        not_cpu.zero_flag ||
        !not_cpu.halted) {
        std::cerr << "FAIL: Bitwise NOT" << std::endl;
        return 1;
    }
    std::cout << "PASS: Bitwise NOT" << std::endl;


    // Test 38: NOT produces zero
    CPU not_zero_cpu;
    not_zero_cpu.write_memory(0, CPU::OP_LOAD);
    not_zero_cpu.write_memory(1, 255);
    not_zero_cpu.write_memory(2, CPU::OP_NOT);
    not_zero_cpu.write_memory(3, CPU::OP_HALT);
    not_zero_cpu.run();

    if (not_zero_cpu.register_a != 0 ||
        !not_zero_cpu.zero_flag ||
        !not_zero_cpu.halted) {
        std::cerr << "FAIL: NOT produces zero" << std::endl;
        return 1;
    }
    std::cout << "PASS: NOT produces zero" << std::endl;


    // Test 39: Shift left
    CPU shl_cpu;
    shl_cpu.write_memory(0, CPU::OP_LOAD);
    shl_cpu.write_memory(1, 12);
    shl_cpu.write_memory(2, CPU::OP_SHL);
    shl_cpu.write_memory(3, CPU::OP_HALT);
    shl_cpu.run();

    if (shl_cpu.register_a != 24 ||
        shl_cpu.zero_flag ||
        !shl_cpu.halted) {
        std::cerr << "FAIL: Shift left" << std::endl;
        return 1;
    }
    std::cout << "PASS: Shift left" << std::endl;


    // Test 40: Shift left overflow
    CPU shl_overflow_cpu;
    shl_overflow_cpu.write_memory(0, CPU::OP_LOAD);
    shl_overflow_cpu.write_memory(1, 128);
    shl_overflow_cpu.write_memory(2, CPU::OP_SHL);
    shl_overflow_cpu.write_memory(3, CPU::OP_HALT);
    shl_overflow_cpu.run();

    if (shl_overflow_cpu.register_a != 0 ||
        !shl_overflow_cpu.zero_flag ||
        !shl_overflow_cpu.halted) {
        std::cerr << "FAIL: Shift left overflow" << std::endl;
        return 1;
    }
    std::cout << "PASS: Shift left overflow" << std::endl;


    // Test 41: Shift right
    CPU shr_cpu;
    shr_cpu.write_memory(0, CPU::OP_LOAD);
    shr_cpu.write_memory(1, 12);
    shr_cpu.write_memory(2, CPU::OP_SHR);
    shr_cpu.write_memory(3, CPU::OP_HALT);
    shr_cpu.run();

    if (shr_cpu.register_a != 6 ||
        shr_cpu.zero_flag ||
        !shr_cpu.halted) {
        std::cerr << "FAIL: Shift right" << std::endl;
        return 1;
    }
    std::cout << "PASS: Shift right" << std::endl;


    // Test 42: Shift right produces zero
    CPU shr_zero_cpu;
    shr_zero_cpu.write_memory(0, CPU::OP_LOAD);
    shr_zero_cpu.write_memory(1, 1);
    shr_zero_cpu.write_memory(2, CPU::OP_SHR);
    shr_zero_cpu.write_memory(3, CPU::OP_HALT);
    shr_zero_cpu.run();

    if (shr_zero_cpu.register_a != 0 ||
        !shr_zero_cpu.zero_flag ||
        !shr_zero_cpu.halted) {
        std::cerr << "FAIL: Shift right produces zero" << std::endl;
        return 1;
    }
    std::cout << "PASS: Shift right produces zero" << std::endl;


    // Test 43: PUSH and POP preserve register A
    CPU push_pop_cpu;
    push_pop_cpu.write_memory(0, CPU::OP_LOAD);
    push_pop_cpu.write_memory(1, 42);
    push_pop_cpu.write_memory(2, CPU::OP_PUSH);
    push_pop_cpu.write_memory(3, CPU::OP_LOAD);
    push_pop_cpu.write_memory(4, 10);
    push_pop_cpu.write_memory(5, CPU::OP_POP);
    push_pop_cpu.write_memory(6, CPU::OP_HALT);
    push_pop_cpu.run();

    if (push_pop_cpu.register_a != 42 ||
        push_pop_cpu.stack_pointer != CPU::MEMORY_SIZE ||
        !push_pop_cpu.halted) {
        std::cerr << "FAIL: PUSH and POP preserve register A"
                  << std::endl;
        return 1;
    }
    std::cout << "PASS: PUSH and POP preserve register A"
              << std::endl;


    // Test 44: Stack underflow
    CPU underflow_cpu;
    underflow_cpu.write_memory(0, CPU::OP_POP);
    underflow_cpu.write_memory(1, CPU::OP_HALT);

    try {
        underflow_cpu.run();
        std::cerr << "FAIL: Stack underflow" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != "Stack underflow") {
            std::cerr << "FAIL: Unexpected stack error: "
                      << error.what() << std::endl;
            return 1;
        }
        std::cout << "PASS: Stack underflow" << std::endl;
    }


    // Test 45: Stack overflow
    CPU overflow_cpu;
    overflow_cpu.stack_pointer = 0;
    overflow_cpu.write_memory(0, CPU::OP_PUSH);
    overflow_cpu.write_memory(1, CPU::OP_HALT);

    try {
        overflow_cpu.run();
        std::cerr << "FAIL: Stack overflow" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != "Stack overflow") {
            std::cerr << "FAIL: Unexpected stack error: "
                      << error.what() << std::endl;
            return 1;
        }
        std::cout << "PASS: Stack overflow" << std::endl;
    }


    // Test 46: CALL and RET return to the main program
    CPU call_ret_cpu;

    call_ret_cpu.write_memory(0, CPU::OP_CALL);
    call_ret_cpu.write_memory(1, 6);
    call_ret_cpu.write_memory(2, CPU::OP_HALT);

    // Subroutine at address 6
    call_ret_cpu.write_memory(6, CPU::OP_LOAD);
    call_ret_cpu.write_memory(7, 42);
    call_ret_cpu.write_memory(8, CPU::OP_RET);

    call_ret_cpu.run();

    if (call_ret_cpu.register_a != 42 ||
        call_ret_cpu.program_counter != 3 ||
        call_ret_cpu.stack_pointer != CPU::MEMORY_SIZE ||
        !call_ret_cpu.halted) {
        std::cerr << "FAIL: CALL and RET return to main program"
                  << std::endl;
        return 1;
    }

    std::cout << "PASS: CALL and RET return to main program"
              << std::endl;

    
    // Test 47: Nested CALL and RET
    CPU nested_call_cpu;

    // Main program
    nested_call_cpu.write_memory(0, CPU::OP_CALL);
    nested_call_cpu.write_memory(1, 10);
    nested_call_cpu.write_memory(2, CPU::OP_HALT);

    // Subroutine 1 at address 10
    nested_call_cpu.write_memory(10, CPU::OP_CALL);
    nested_call_cpu.write_memory(11, 20);
    nested_call_cpu.write_memory(12, CPU::OP_LOAD);
    nested_call_cpu.write_memory(13, 42);
    nested_call_cpu.write_memory(14, CPU::OP_RET);

    // Subroutine 2 at address 20
    nested_call_cpu.write_memory(20, CPU::OP_LOAD);
    nested_call_cpu.write_memory(21, 99);
    nested_call_cpu.write_memory(22, CPU::OP_RET);

    nested_call_cpu.run();

    if (nested_call_cpu.register_a != 42 ||
        nested_call_cpu.program_counter != 3 ||
        nested_call_cpu.stack_pointer != CPU::MEMORY_SIZE ||
        !nested_call_cpu.halted) {
        std::cerr << "FAIL: Nested CALL and RET" << std::endl;
        return 1;
    }

    std::cout << "PASS: Nested CALL and RET" << std::endl;


    // Test 48: CALL stack overflow
    CPU call_overflow_cpu;

    call_overflow_cpu.stack_pointer = 1;
    call_overflow_cpu.write_memory(0, CPU::OP_CALL);
    call_overflow_cpu.write_memory(1, 10);
    call_overflow_cpu.write_memory(10, CPU::OP_HALT);

    try {
        call_overflow_cpu.run();
        std::cerr << "FAIL: CALL stack overflow" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != "Stack overflow") {
            std::cerr << "FAIL: Unexpected error: "
                      << error.what() << std::endl;
            return 1;
        }

        std::cout << "PASS: CALL stack overflow" << std::endl;
    }
        // Test 49: RET without CALL must fail
    CPU ret_without_call_cpu;

    ret_without_call_cpu.write_memory(0, CPU::OP_PUSH);
    ret_without_call_cpu.write_memory(1, CPU::OP_PUSH);
    ret_without_call_cpu.write_memory(2, CPU::OP_RET);

    try {
        ret_without_call_cpu.run();
        std::cerr << "FAIL: RET without CALL" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != "Stack underflow") {
            std::cerr << "FAIL: Unexpected RET error: "
                      << error.what() << std::endl;
            return 1;
        }

        std::cout << "PASS: RET without CALL" << std::endl;
    }

    // Test 50: RET after PUSH must fail
    CPU ret_after_push_cpu;

    ret_after_push_cpu.write_memory(0, CPU::OP_CALL);
    ret_after_push_cpu.write_memory(1, 6);
    ret_after_push_cpu.write_memory(2, CPU::OP_HALT);

    ret_after_push_cpu.write_memory(6, CPU::OP_PUSH);
    ret_after_push_cpu.write_memory(7, CPU::OP_RET);

    try {
        ret_after_push_cpu.run();
        std::cerr << "FAIL: RET after PUSH" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != "Stack underflow") {
            std::cerr << "FAIL: Unexpected RET after PUSH error: "
                      << error.what() << std::endl;
            return 1;
        }

        std::cout << "PASS: RET after PUSH" << std::endl;
    }

    // Test 51: POP must not consume a return address
    CPU pop_return_address_cpu;

    pop_return_address_cpu.write_memory(0, CPU::OP_CALL);
    pop_return_address_cpu.write_memory(1, 6);
    pop_return_address_cpu.write_memory(2, CPU::OP_HALT);

    pop_return_address_cpu.write_memory(6, CPU::OP_POP);
    pop_return_address_cpu.write_memory(7, CPU::OP_RET);

    try {
        pop_return_address_cpu.run();
        std::cerr << "FAIL: POP consumed return address" << std::endl;
        return 1;
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) != "Stack underflow") {
            std::cerr << "FAIL: Unexpected error: "
                      << error.what() << std::endl;
            return 1;
        }

        std::cout << "PASS: POP cannot consume return address"
                  << std::endl;
    }


    
    std::cout << "All tests passed." << std::endl;
    return 0;
}
