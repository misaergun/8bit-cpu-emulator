
#include <iostream>
#include "cpu.h"

int main() {
    CPU cpu;

    cpu.register_a = 5;
    std::cout << "Register A before ADD: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    cpu.add(3);

    std::cout << "Register A after ADD: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    cpu.register_a = 250;
    cpu.add(10);

    std::cout << "Register A after 250 + 10: "
              << static_cast<int>(cpu.register_a)
              << std::endl;

    return 0;
}
