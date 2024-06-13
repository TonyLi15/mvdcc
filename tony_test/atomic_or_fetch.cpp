#include <bitset>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#define THREAD_SIZE 64

std::mutex cout_mutex;

uint64_t bit_map = 0;

void worker(int tid) {
    std::lock_guard<std::mutex> guard(cout_mutex);
    std::cout << "My TID is: " << tid << std::endl;

    std::cout << "The value before I fetch: " << std::bitset<64>(bit_map) << std::endl;
    __atomic_or_fetch(&bit_map, 1ULL << tid, __ATOMIC_SEQ_CST);

    std::cout << "The value after I fetch: " << std::bitset<64>(bit_map) << std::endl;
    // bit_map or (1 << tid)
    // 00000 or 00001 = 00001
    // 00001 or 10000 = 10001
    // 10001 or 01000 = 11001
    // 11001 or 00100 = 11101
    // 11101 or 00010 = 11111
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_SIZE; i++) {
        threads.emplace_back(worker, i);
    };
    for (int i = 0; i < THREAD_SIZE; i++) {
        threads[i].join();
    };
    std::cout << bit_map << std::endl;
    std::cout << std::bitset<64>(bit_map) << std::endl;

    return 0;
}