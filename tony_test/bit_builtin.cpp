// Tests for GCC Builtin Functions for bit operations
// Reference: https://naoyat.hatenablog.jp/entry/2014/05/12/143650
// Reference: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html

/* Functions
(1) __buitin_ffs : returns the location of first "1" from right
(2) __buitlin_clz : returns the number of leading 0s
(3) __buitlin_ctz : returns the number of trailing 0s
*/


#include <bitset>
#include <iostream>
#include <type_traits>  // For type casting -> Input: uint64_t ; Output: int


template <typename T>
int first_one_from_right(T x) {
    static_assert(std::is_same_v<T, uint64_t>, "T must be uint64_t");
    return __builtin_ffsll(x);
}

template <typename T>
int num_leading_zeros(T x) {
    static_assert(std::is_same_v<T, uint64_t>, "T must be uint64_t");
    return __builtin_clzll(x);
}

template <typename T>
int num_trailing_zeros(T x) {
    static_assert(std::is_same_v<T, uint64_t>, "T must be uint64_t");
    return __builtin_ctzll(x);
}

template <typename T>
void print(T x) {
    std::cout << x << std::endl;
}

int main() {
    // Change "bits" to bits that need to be tested
    uint64_t bits = 0b0000000000000000000000000000000000000000000000000000000010101011;
    print("The number in decimal format: ");
    print(bits);

    print("The number in binary format: ");
    print(std::bitset<64>(bits));

    print("First one from right (in binary format): ");
    print(first_one_from_right(bits));

    print("Number of leading zeros: ");
    print(num_leading_zeros(bits));

    print("Number of trailing zeros: ");
    print(num_trailing_zeros(bits));
}