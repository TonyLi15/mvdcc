// find_latest_pos: used for core bitmap and transaction bitmap
// for checking the correctness: https://bytetool.web.app/en/
#include <iostream>
#include <string>
#include <bitset>
#include <cstdint>


uint64_t clear_left(uint64_t bits, int pos) {
    uint64_t mask = (1ULL << (pos + 1)) - 1;
    return bits & mask;
}

template <typename T>
int num_leading_zeros(T x) {
    static_assert(std::is_same_v<T, uint64_t>, "T must be uint64_t");
    return __builtin_clzll(x);
}

// find the most-left "1" after clearing all bits from pos
int find_latest_pos(uint64_t bits, int pos){
    // mask the core bitmap
    uint64_t masked_bits = clear_left(bits, pos);
    std::cout << std::bitset<64>(masked_bits) << std::endl;
    return (64 - num_leading_zeros(masked_bits));
}


int main() {
   uint64_t bits = 0xFC;
    std::cout << bits << std::endl;
    std::cout << std::bitset<64>(bits) << std::endl;
    
    int pos = 4;
    
    int latest_pos = find_latest_pos(bits, pos);

    std::cout << latest_pos << std::endl;
    
    
    return 0;
}