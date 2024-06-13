// setbit
#include <bitset>
#include <cstdint>
#include <iostream>
#include <string>

uint64_t setbit(uint64_t bits, int pos) {
    if (pos < 0 || pos >= 64) {
        // invalid pos return the original bits
        return bits;
    }
    return bits | (static_cast<uint64_t>(1) << pos);
}


int main() {
    uint64_t bits = 0x00;
    std::cout << bits << std::endl;
    std::cout << std::bitset<64>(bits) << std::endl;
    int pos = 63;

    uint64_t new_bits = setbit(bits, pos);
    std::cout << std::bitset<64>(new_bits) << std::endl;

    std::cout << new_bits << std::endl;

    return 0;
}