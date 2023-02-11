#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include "compile_time_constants.h"
#include "chess-library/src/chess.hpp"

enum Bound_Type : uint8_t {
    UPPER_BOUND, LOWER_BOUND, EXACT, EVALUATING
};

enum TT_Strategy {
    DEPTH_FIRST, RANDOM_REPLACE, REPLACE_LAST_ENTRY, TWO_TWO_SPLIT
};

struct TT_Info {
    Eval_Type eval;
    Chess::Move move;
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

template<TT_Strategy strategy>
class Transposition_Table {

private:
    struct Entry {
        uint64_t key = 0;
        TT_Info value = {};
    };

    struct alignas(64) Bucket {
        Entry entries[entries_per_bucket];
    };

public:
    explicit Transposition_Table(uint64_t size_in_mb = 8192) :
                size((1 << 20) * std::bit_floor(size_in_mb) / sizeof(Bucket)), mask(size - 1), table(size) {
    }

    void print_size() const {
        uint64_t num_elements = 0, exact_entries = 0;
        for (const Bucket& bucket : table) {
            for (auto & entry : bucket.entries) {
                if (entry.key != 0) {
                    num_elements++;
                    if (entry.value.type == EXACT) {
                        exact_entries++;
                    }
                }
            }
        }
        std::cout << "Table elements: " << num_elements << ", exact entries: " << exact_entries << ", total writes: "
                  << writes << " bucket count " << table.size() << ", bucket capacity: " << table.capacity() << std::endl;
    }

    template<TT_Strategy strat>
    void replace(Entry entries[entries_per_bucket], uint64_t key, TT_Info value);

    template<>
    void replace<RANDOM_REPLACE>(Entry entries[entries_per_bucket], uint64_t key, TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.key == 0) {
                entry.value = value;
                entry.key = key;
                return;
            }
        }
        entries[writes % entries_per_bucket].key = key; // Modulo but by a compile-time constant so this should be optimized
        entries[writes % entries_per_bucket].value = value; // Missed writes is basically random across different buckets
    }

    template<>
    void replace<TWO_TWO_SPLIT>(Entry entries[entries_per_bucket], uint64_t key, TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
        if (key != 0) { // So we didn't just overwrite an empty entry
            auto & entry = entries[2 + (writes & 1)]; // Writes & 1 "randomly" chooses the 3rd or 4th entry to overwrite
            std::swap(entry.value, value);
            std::swap(entry.key, key);
        }
    }

    template<>
    void replace<REPLACE_LAST_ENTRY>(Entry entries[entries_per_bucket], uint64_t key, TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value || i == 3) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }

    template<>
    void replace<DEPTH_FIRST>(Entry entries[entries_per_bucket], uint64_t key, TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }

    void emplace(uint64_t key, TT_Info value, int32_t depth) {
        if constexpr (!use_tt) {
            return;
        }
        auto & entries = table[pos(key, depth)].entries;
        for (auto & entry : entries) { // Check if the entry already exists
            if (entry.key == key) {
                assert(entry.value.depth == depth);
                assert(value.depth == depth);
                entry.value = value;
                return;
            }
        }
        writes++; // Entry does not exist yet so we create it
        replace<strategy>(entries, key, value); // Try to replace an existing (possibly empty) entry.
    }

    /**
     * This should ideally only be called after making sure the entry exists via the contains method.
     */
    [[nodiscard]] TT_Info at(uint64_t key, int32_t depth) const {
        for (auto& entry : table[pos(key, depth)].entries) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        return TT_Info{};
    }

    /**
     * Returns true and puts the value into the third parameter reference, if such an entry exists, and false otherwise.
     */
    [[nodiscard]] bool get_if_exists(uint64_t key, int32_t depth, TT_Info& info) const {
        if constexpr (!use_tt) {
            return false;
        }
        for (auto& entry : table[pos(key, depth)].entries) {
            if (entry.key == key) {
                info = entry.value;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool contains(uint64_t key, int32_t depth) const {
        if constexpr (!use_tt) {
            return false;
        }
        for (auto& entry : table[pos(key, depth)].entries) { // NOLINT(readability-use-anyofallof)
            if (entry.key == key) {
                return true;
            }
        }
        return false;
    }

    void print_pv(Board& board, int depth) {
        Board copy(board);
        while (depth > 0) {
            TT_Info info{};
            if (get_if_exists(copy.hashKey, depth, info)) {
                Move move = info.move;
                std::cout << convertMoveToUci(move) << " ";
                copy.makeMove(move);
                depth--;
            } else {
                break;
            }
        }
        std::cout << std::endl;
    }

    /*
     * Break down the key in the usual modulo/bit masking way. Since for each key we need an entry for each depth, to
     * not overload a single bucket we put each different depth entry in a different bucket. In theory, it does not
     * matter how to choose that different bucket, but in practice we often look up pairs of these keys, so it makes
     * sense to put them next to each other. (this gives a small but measurable speedup as well) Since we will usually
     * look up an entry of a certain depth and then the entry of the previous depth, by subtracting depth we make sure
     * that the second entry is the next entry in the vector, i.e. the next cache line.
     */
    [[nodiscard]] inline uint64_t pos(uint64_t key, int32_t depth) const {
        return (key - depth) & mask; // this is a compile-time constant and gets compiled to either a bit and or an efficient version of this
    }

    void clear() {
        writes = 0;
        std::fill(table.begin(), table.end(), Bucket());
    }

private:
    uint64_t size;
    uint64_t mask;
    std::vector<Bucket> table;

    uint64_t writes = 0;
};