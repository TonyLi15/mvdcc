// Tests for operation of masking 64bits

#include <bitset>
#include <iostream>

uint64_t clear_left(uint64_t bits, int pos) {
    uint64_t mask = (1ULL << (pos + 1)) - 1;
    return bits & mask;
}

uint64_t clear_right(uint64_t bits, int pos) {
    uint64_t mask = ~((1ULL << pos) - 1);
    return bits & mask;
}


int main() {
    uint64_t bits = 0xFFFFFFFFFFFFFFFF;
    int pos = 2;

    uint64_t result_left = clear_left(bits, pos);
    uint64_t result_right = clear_right(bits, pos);
    std::cout << std::bitset<64>(result_left) << std::endl;
    std::cout << std::bitset<64>(result_right) << std::endl;

    return 0;
}