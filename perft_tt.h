#pragma once

#include <cstdint>
#include <vector>
#include <iostream>

class Perft_TT {

private:
    static constexpr uint32_t entries_per_bucket = 4;

    struct Entry {
        uint64_t key;
        uint64_t value;
    };

    struct alignas(64) Bucket {
        Entry entries[entries_per_bucket];
    };



public:
    Perft_TT() : table(size) {
    }

    void print_size() {
        uint64_t num_elements = 0;
        for (Bucket& bucket : table) {
            for (auto & entry : bucket.entries) {
                if (entry.key != 0) {
                    num_elements++;
                }
            }
        }
        std::cout << "Table elements: " << num_elements << ", missed writes: " << missed_writes << " bucket count "
                  << table.size() << ", bucket capacity: " << table.capacity() << std::endl;
    }

    /*void emplace(uint64_t key, uint64_t value) {
        auto & entries = table[pos(key)].entries;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value || i == 3) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }*/

    void emplace(uint64_t key, uint64_t value) {
        auto & entries = table[pos(key)].entries;
        bool swapped = false;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
                swapped = true;
            }
        }
        if (!swapped) {
            missed_writes++;
            auto & entry = entries[2 + (missed_writes & 1)];
            std::swap(entry.value, value);
            std::swap(entry.key, key);
        }
    }

    /*void emplace(uint64_t key, uint64_t value) {
        auto & entries = table[pos(key)].entries;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
        missed_writes += value == 0 ? 0 : 1;
    }*/

    uint64_t at(uint64_t key) {
        for (auto& entry : table[pos(key)].entries) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        return 0;
    }

    bool contains(uint64_t key) {
        for (auto& entry : table[pos(key)].entries) { // NOLINT(readability-use-anyofallof)
            if (entry.key == key) {
                return true;
            }
        }
        return false;
    }

    static inline uint64_t pos(uint64_t key) {
        return key % size; // this is a compile-time constant and gets compiled to either a bit and or an efficient version of this
    }

private:
    static constexpr uint32_t size = 2 << 17;
    std::vector<Bucket> table;

    uint64_t missed_writes = 0;
};