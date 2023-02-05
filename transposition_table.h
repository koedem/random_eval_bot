#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include "chess-library/src/chess.hpp"

class Transposition_Table {

private:
    static constexpr uint32_t entries_per_bucket = 4;

    enum Bound_Type : uint8_t {
        ALPHA, BETA, EXACT
    };

    struct TT_Info {
        Chess::Move move;
        int16_t eval;
        int8_t depth;
        Bound_Type type;

        bool operator<(TT_Info& other) const {
            if (type == EXACT && other.type != EXACT) {
                return false;
            } else if (type != EXACT && other.type == EXACT) {
                return true;
            }
            return depth < other.depth;
        }
    };

    struct Entry {
        uint64_t key;
        TT_Info value;
    };

    struct alignas(64) Bucket {
        Entry entries[entries_per_bucket];
    };



public:
    Transposition_Table() : table(size) {
    }

    void print_size() const {
        uint64_t num_elements = 0;
        for (const Bucket& bucket : table) {
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

    void emplace(uint64_t key, TT_Info value, uint32_t depth) {
        auto & entries = table[pos(key + depth)].entries;
        for (auto & entry : entries) { // Check if the entry already exists
            if (entry.key == key) {
                assert(entry.info.depth == depth);
                assert(value.depth == depth);
                entry.value = value;
                return;
            }
        }

        bool swapped = false;
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
                swapped = true;
            }
        }
        /*if (!swapped) {
            missed_writes++;
            auto & entry = entries[2 + (missed_writes & 1)];
            std::swap(entry.value, value);
            std::swap(entry.key, key);
        }*/
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

    [[nodiscard]] TT_Info at(uint64_t key) const {
        for (auto& entry : table[pos(key)].entries) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        return TT_Info{};
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