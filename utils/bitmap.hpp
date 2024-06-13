#pragma once

// 1000...0000
// 注： 符号付きの右シフトは左が符号ビットで埋まる
// int64_t set_upper_bit___signed() { return UINT64_MAX ^ (UINT64_MAX >>
// 1); }
int64_t set_upper_bit___signed() { return ~(~0ULL >> 1); }
uint64_t set_upper_bit___unsigned() { return ~(~0ULL >> 1); } // TODO

uint64_t set_bit_at_the_given_location(uint64_t pos) { // TODO
    return set_upper_bit___unsigned() >> pos;
}
uint64_t set_bit_at_the_given_location(uint64_t bitmap, uint64_t pos) {
    assert(pos < 64); // TODO
    return bitmap | set_bit_at_the_given_location(pos);
}

uint64_t fill_the_left_side_until_before_the_given_position(int pos) {
    /*
    pos: 0
    0000 0000 0000 0000
    */
    if (pos == 0)
        return 0;

    /*
    pos: 1
    1000 0000 0000 0000

    pos: 2
    1100 0000 0000 0000

    pos: 3
    1110 0000 0000 0000
    */
    return set_upper_bit___signed() >> (pos - 1);
}

uint64_t fill_the_left_side_until_the_given_position(int pos) {
    /*
    pos: 0
    1000 0000 0000 0000

    pos: 1
    1100 0000 0000 0000

    pos: 2
    1110 0000 0000 0000

    pos: 3
    1111 0000 0000 0000
    */
    return set_upper_bit___signed() >> pos;
}

bool is_bit_set_at_the_position(uint64_t bitmap, int pos) {
    return (bitmap & set_bit_at_the_given_location(pos)) != 0;
}
// returns the location of 1st "1" from right
int find_the_location_of_first_bit_from_right(uint64_t bits) {
    return __builtin_ffsll(bits);
}

// TODO: refactor the name
int find_the_largest(uint64_t bits) {
    assert(bits != 0);
    int result = 64 - find_the_location_of_first_bit_from_right(bits);
    assert(0 <= result && result < 64);
    return result;
}

int find_the_largest_among_or_less_than(uint64_t bitmap, int pos) {
    assert(bitmap != 0);
    assert(0 <= pos && pos < 64);
    uint64_t smallers =
        bitmap & fill_the_left_side_until_the_given_position(pos);
    if (smallers == 0)
        return -1;
    return find_the_largest(smallers);
}

int find_the_largest_among_less_than(uint64_t bitmap, int pos) {
    assert(bitmap != 0);
    assert(0 <= pos && pos < 64);
    uint64_t smallers =
        bitmap & fill_the_left_side_until_before_the_given_position(pos);
    if (smallers == 0)
        return -1;
    return find_the_largest(smallers);
}
