#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include <bit>
#include "compile_time_constants.h"
#include "transposition_table.h"
#include "chess.hpp"

struct Spin_Lock {
    std::atomic<bool> spin_lock = false;

    void lock() {
        while(spin_lock.exchange(true));
    }

    void unlock() {
        spin_lock.store(false);
    }
};

struct Locked_TT_Info {
    Eval_Type eval;
    Chess::Move move;
    int8_t depth;
    Bound_Type type;

    /**
     * We don't lock anything here, it is the users responsibility to make sure the surrounding structs are locked.
     * @param other
     * @return
     */
    bool operator<(Locked_TT_Info& other) const {
        if (type == EXACT && other.type != EXACT) {
            return false;
        } else if (type != EXACT && other.type == EXACT) {
            return true;
        }
        return depth < other.depth;
    }
};

template<TT_Strategy strategy>
class Locking_TT {

private:
    struct Entry {
        uint64_t key = 0;
        Locked_TT_Info value = {};
        Spin_Lock spin_lock;
    };

    struct alignas(64) Bucket {
        Entry entries[entries_per_bucket];
    };

public:
    explicit Locking_TT(uint64_t size_in_mb = 8192) :
                size((1 << 20) * std::bit_floor(size_in_mb) / sizeof(Bucket)), mask(size - 1), table(size) {
    }

    /**
     * This method is not thread safe because there's not really a reason to make it.
     */
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

    /**
     * This method assumes that if necessary the corresponding entries lock has already been acquired.
     * @tparam strat
     * @param entries
     * @param key
     * @param value
     */
    template<TT_Strategy strat>
    void replace(Entry entries[entries_per_bucket], uint64_t key, Locked_TT_Info value);

    template<>
    void replace<RANDOM_REPLACE>(Entry entries[entries_per_bucket], uint64_t key, Locked_TT_Info value) {
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
    void replace<TWO_TWO_SPLIT>(Entry entries[entries_per_bucket], uint64_t key, Locked_TT_Info value) {
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
    void replace<REPLACE_LAST_ENTRY>(Entry entries[entries_per_bucket], uint64_t key, Locked_TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value || i == 3) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }

    template<>
    void replace<DEPTH_FIRST>(Entry entries[entries_per_bucket], uint64_t key, Locked_TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry.value < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }

    /**
     * Locks the corresponding bucket and writes the entry. This is a blocking write.
     * @param key
     * @param value
     * @param depth
     */
    void emplace(uint64_t key, Locked_TT_Info value, int32_t depth) {
        if constexpr (!use_tt) {
            return;
        }
        auto position = pos(key, depth);
        Spin_Lock& spin_lock = table[position].entries[0].spin_lock;
        std::lock_guard<Spin_Lock> guard(spin_lock);
        auto & entries = table[position].entries;
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
    [[nodiscard]] Locked_TT_Info at(uint64_t key, int32_t depth) const {
        auto position = pos(key, depth);
        std::lock_guard<Spin_Lock> guard(table[position].entries[0].spin_lock);
        auto & entries = table[position].entries;
        for (auto& entry : entries) {
            if (entry.key == key) {
                return entry.value;
            }
        }
        return Locked_TT_Info{};
    }

    /**
     * Returns true and puts the value into the third parameter reference, if such an entry exists, and false otherwise.
     */
    [[nodiscard]] bool get_if_exists(uint64_t key, int32_t depth, Locked_TT_Info& info) {
        if constexpr (!use_tt) {
            return false;
        }
        auto position = pos(key, depth);
        Spin_Lock& spin_lock = table[position].entries[0].spin_lock;
        std::lock_guard<Spin_Lock> guard(spin_lock);
        auto & entries = table[position].entries;
        for (auto& entry : entries) {
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
        auto position = pos(key, depth);
        std::lock_guard<Spin_Lock> guard(table[position].entries[0].spin_lock);
        auto & entries = table[position].entries;
        for (auto& entry : entries) { // NOLINT(readability-use-anyofallof)
            if (entry.key == key) {
                return true;
            }
        }
        return false;
    }

    /**
     * This method is thread-safe, I think.
     * @param board
     * @param depth
     */
    void print_pv(Board& board, int depth) {
        Board copy(board);
        while (depth > 0) {
            Locked_TT_Info info{};
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

    /**
     * Imo doesn't make much sense locking this.
     */
    void clear() {
        writes = 0;
        std::fill(table.begin(), table.end(), Bucket());
    }

private:
    uint64_t size;
    uint64_t mask;
    std::vector<Bucket> table;

    std::atomic<uint64_t> writes = 0;
};