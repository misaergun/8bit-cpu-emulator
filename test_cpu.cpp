
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
    reset_cpu.write_memory(2, CPU::OP_HALT);
    reset_cpu.run();

    reset_cpu.reset();

    if (reset_cpu.register_a != 0 ||
        reset_cpu.program_counter != 0 ||
        reset_cpu.halted ||
        reset_cpu.read_memory(0) != 0 ||
        reset_cpu.read_memory(1) != 0) {
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

    std::cout << "All tests passed." << std::endl;
    return 0;
}
