#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
#include <bit>
#include "compile_time_constants.h"
#include "transposition_table.h"
#include "chess.hpp"
#include "locking_tt.h"

/**
 * This was originally supposed to be a template specialization of a templated locking TT, however, I could not get that
 * to work. So here the unfortunate partly code duplicated version.
 */

struct __attribute__((packed)) ABDADA_TT_Info {
    Eval_Type eval;
    Chess::Move move;
    int8_t depth;
    Bound_Type type;
    std::int8_t proc_number;
};

template<TT_Strategy strategy>
class ABDADA_TT {

private:
    struct Entry {
        uint64_t key = 0;
        ABDADA_TT_Info value = {};
        Spin_Lock spin_lock;

        /**
         * We don't lock anything here, it is the users responsibility to make sure the surrounding structs are locked.
         * @param other
         * @return
         */
        bool operator<(ABDADA_TT_Info& other) const {
            if (value.proc_number > 0) {  // If we are currently searching on this entry, then this should not be replaced,
                return false;       // so gets higher priority.
            }
            if (value.type == EXACT && other.type != EXACT) {
                return false;
            } else if (value.type != EXACT && other.type == EXACT) {
                return true;
            }
            return value.depth < other.depth;
        }
    };

    struct alignas(64) Bucket {
        Entry entries[entries_per_bucket];
    };

public:
    explicit ABDADA_TT(uint64_t size_in_mb = 8192) :
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
    void replace(Entry entries[entries_per_bucket], uint64_t key, ABDADA_TT_Info value);

    template<>
    void replace<RANDOM_REPLACE>(Entry entries[entries_per_bucket], uint64_t key, ABDADA_TT_Info value) {
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
    void replace<TWO_TWO_SPLIT>(Entry entries[entries_per_bucket], uint64_t key, ABDADA_TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry < value) { // last slot is always replace
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
    void replace<REPLACE_LAST_ENTRY>(Entry entries[entries_per_bucket], uint64_t key, ABDADA_TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry < value || i == 3) { // last slot is always replace
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }

    template<>
    void replace<DEPTH_FIRST>(Entry entries[entries_per_bucket], uint64_t key, ABDADA_TT_Info value) {
        for (int i = 0; i < 4; i++) {
            auto & entry = entries[i];
            if (entry < value) {
                std::swap(entry.value, value);
                std::swap(entry.key, key);
            }
        }
    }

    /**
     * Locks the corresponding bucket and writes the entry. This is a blocking write.
     * @tparam DECREMENTING If this is set to true, the function will decrement the proc counter if the entry already exists.
     * @param key
     * @param value
     * @param depth
     */
    template<bool DECREMENTING>
    void emplace(uint64_t key, ABDADA_TT_Info value, int32_t depth) {
        if constexpr (!use_tt) {
            return;
        }
        auto position = pos(key, depth);
        Spin_Lock& spin_lock = table[position].entries[0].spin_lock;
        std::lock_guard<Spin_Lock> guard(spin_lock);
        auto & entries = table[position].entries;
        for (int i = 0; i < 4; i++) { // Check if the entry already exists
            auto & entry = entries[i];
            if (entry.key == key) {
                assert(entry.value.depth == depth);
                assert(value.depth == depth);
                value.proc_number = entry.value.proc_number; // We will write the value to that position so remember the proc count
                if constexpr (DECREMENTING) {
                    if (value.proc_number > 0) {
                        value.proc_number--;
                        while (i < 3 && entries[i] < entries[i + 1].value) { // Decrementing the proc counter decreases our priority
                            std::swap(entries[i].value, entries[i + 1].value); // So we should move down as far as possible to not
                            std::swap(entries[i].key, entries[i + 1].key); // replace higher priority entries instead of us
                            i++;
                        }
                    }
                }
                entry.value = value;
                return;
            }
        }
        writes++; // Entry does not exist yet so we create it
        replace<strategy>(entries, key, value); // Try to replace an existing (possibly empty) entry.
    }

    /** TODO if ever used this should probably be looked at again
     * This should ideally only be called after making sure the entry exists via the contains method.
     */
    template<bool INCREMENTING>
    [[nodiscard]] Locked_TT_Info at(uint64_t key, int32_t depth, bool exclusive) {
        auto position = pos(key, depth);
        std::lock_guard<Spin_Lock> guard(table[position].entries[0].spin_lock);
        auto & entries = table[position].entries;
        for (auto& entry : entries) {
            if (entry.key == key) {
                if constexpr (INCREMENTING) {
                    if (entry.value.proc_number == 0 || !exclusive) { // This node is likely getting searched
                        entry.value.proc_number++;
                    } // else this won't be searched because another thread already does. So don't increment proc then
                }
                return entry.value;
            }
        }
        return Locked_TT_Info{};
    }

    void decrement_proc(uint64_t key, int32_t depth) {
        auto position = pos(key, depth);
        std::lock_guard<Spin_Lock> guard(table[position].entries[0].spin_lock);
        auto & entries = table[position].entries;
        for (int i = 0; i < 4; i++) { // NOLINT(readability-use-anyofallof)
            auto& entry = entries[i];
            if (entry.key == key) {
                entry.value.proc_number--;
                while (i < 3 && entries[i] < entries[i + 1].value) { // Decrementing the proc counter decreases our priority
                    std::swap(entries[i].value, entries[i + 1].value); // So we should move down as far as possible to not
                    std::swap(entries[i].key, entries[i + 1].key); // replace higher priority entries instead of us
                    i++;
                }
            }
        }
    }

    /**
     * Returns true and puts the value into the third parameter reference, if such an entry exists, and false otherwise.
     */
    template<bool INCREMENTING>
    [[nodiscard]] bool get_if_exists(uint64_t key, int32_t depth, ABDADA_TT_Info& info, bool exclusive) {
        if constexpr (!use_tt) {
            return false;
        }
        auto position = pos(key, depth);
        { // Put the lockguard in an inner scope because if we emplace a new entry below, we reacquire the lock there
            Spin_Lock &spin_lock = table[position].entries[0].spin_lock;
            std::lock_guard<Spin_Lock> guard(spin_lock);
            auto &entries = table[position].entries;
            for (int i = 0; i < 4; i++) {
                auto &entry = entries[i];
                if (entry.key == key) {
                    info = entry.value;
                    if constexpr (INCREMENTING) {
                        if (entry.value.type != EXACT // Otherwise cutoff and no search
                            && (entry.value.proc_number == 0 || !exclusive)) { // Otherwise skip and no search
                            entry.value.proc_number++; // If likely search, increment proc_number
                            while (i > 0 && entries[i - 1] < entries[i].value) { // Incrementing the proc counter increases our priority
                                std::swap(entries[i - 1].value, entries[i].value); // So we should move up as far as possible to not get replaced
                                std::swap(entries[i - 1].key, entries[i].key);
                                i--;
                            }
                        }
                    }
                    return true;
                }
            }
        }
        if constexpr (INCREMENTING) { // I.e. we are planning to search this
            if (depth >= DEFER_DEPTH) { // The entry does not exist yet, but we want to search it, so create new entry
                info.proc_number = 1; // and set the search processors to 1.
                info.depth = depth;
                info.type = EVALUATING;
                info.move = NO_MOVE;
                emplace<false>(key, info, depth);
            }
        }
        return false;
    }

    /**
     *
     * @param key
     * @param depth
     * @return
     */
    [[nodiscard]] bool contains(uint64_t key, int32_t depth) {
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
            if (get_if_exists<false>(copy.hashKey, depth, info)) {
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
        for (Bucket& bucket : table) {
            for (Entry& entry : bucket.entries) {
                entry.key = 0;
                entry.value = {};
                entry.spin_lock.unlock(); // Just in case
            }
        }
    }

private:
    uint64_t size;
    uint64_t mask;
    std::vector<Bucket> table;

    std::atomic<uint64_t> writes = 0;
};
