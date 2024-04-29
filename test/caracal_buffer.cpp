/*
Test File for operations of buffer in Caracal
Last Update: Apr 27, 2024
*/

#include <algorithm>
#include <iostream>
#include <vector>

/*
To-Do
- Remove Sort of PerCoreBuffer
-
*/

class PerCoreBuffer { // Caracal's per-core buffer
  public:
    // use pair data structure
    std::vector<std::pair<uint64_t, uint64_t>> ids_slots_ = {{1, 0}};

    void initialize() { ids_slots_.clear(); }

    bool appendable(uint64_t version, uint64_t tx_id) {

        // append to per-core buffer

        // Find the position to append the version
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(),
                                   std::make_pair(tx_id, 0),
                                   [](const auto &pair, const auto &value) {
                                       return pair.first < value.first;
                                   });

        // Append to the corresponding position
        ids_slots_.insert(it, std::make_pair(tx_id, version));

        if (ids_slots_.size() < 4)
            std::cout << "buffer is appendable" << std::endl;
        return true; // the buffer is not full
        std::cout << "buffer is full" << std::endl;
        return false; // the buffer is full
    }

    void print() {
        for (uint64_t i = 0; i < ids_slots_.size(); i++) {
            std::cout << ids_slots_[i].first << " " << ids_slots_[i].second
                      << " " << std::endl;
        }
    }
};

class GlobalVersionArray {
  public:
    // change version's type to uint64_t for easy test
    std::vector<std::pair<uint64_t, uint64_t>> ids_slots_ = {{0, 0}};

    void append(uint64_t version, uint64_t tx_id) {

        // Find the position to append the version
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(),
                                   std::make_pair(tx_id, 0),
                                   [](const auto &pair, const auto &value) {
                                       return pair.first < value.first;
                                   });

        // Append to the corresponding position
        ids_slots_.insert(it, std::make_pair(tx_id, version));
    }

    void batch_append(PerCoreBuffer *buffer) {
        for (uint64_t i = 0; i < buffer->ids_slots_.size(); i++) {
            append(buffer->ids_slots_[i].second, buffer->ids_slots_[i].first);
        }
        buffer->initialize();
    }

    bool is_exist(uint64_t tx) {
        // Use std::find with a custom comparator
        auto it =
            std::find_if(ids_slots_.begin(), ids_slots_.end(),
                         [tx](const auto &pair) { return pair.first == tx; });

        return it != ids_slots_.end();
    }

    std::pair<uint64_t, uint64_t> search_visible_version(uint64_t tx_id) {
        // Handle the case where tx_id is smaller than or equal to the smallest
        // tx_id in ids_slots_
        if (ids_slots_.empty() || tx_id <= ids_slots_.begin()->first) {
            return {0, 0};
        }

        // Find the first element whose tx_id is greater than the given tx_id
        auto it = std::upper_bound(ids_slots_.begin(), ids_slots_.end(), tx_id,
                                   [](uint64_t tx_id, const auto &pair) {
                                       return tx_id < pair.first;
                                   });

        // Move the iterator back to the previous element
        --it;

        // Return the tx_id and the corresponding Version* object
        return {it->first, it->second};
    }

    void initialize() { ids_slots_.clear(); }

    bool is_empty() { return ids_slots_.empty(); }

    std::pair<uint64_t, uint64_t> latest() {
        if (is_empty())
            return {0, 0};

        return ids_slots_.back();
    }

    void print() {
        for (uint64_t i = 0; i < ids_slots_.size(); i++) {
            std::cout << ids_slots_[i].first << " " << ids_slots_[i].second
                      << std::endl;
        }
    }
};

GlobalVersionArray array;
PerCoreBuffer *buffer;

int main() {
    buffer = new PerCoreBuffer();
    std::cout << "buffer test" << std::endl;
    buffer->appendable(4, 4);
    buffer->appendable(3, 3);
    buffer->appendable(6, 7);
    buffer->appendable(12, 13);
    buffer->print();

    std::cout << "global array test" << std::endl;
    array.append(2, 3);
    array.append(6, 7);
    array.append(4, 5);
    array.print();

    std::cout << "global array batched append from buffer" << std::endl;
    array.batch_append(buffer);

    std::cout << "global array after batch append" << std::endl;
    array.print();

    std::cout << array.is_exist(13) << std::endl; // true

    std::cout << array.search_visible_version(12).first << " "
              << array.search_visible_version(12).second << std::endl;

    std::cout << array.latest().first << " " << array.latest().second
              << std::endl;

    std::cout << array.is_empty() << std::endl; // false

    return 0;
}
