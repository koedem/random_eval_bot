#pragma once

#include <thread>
#include <functional>
#include "locking_tt.h"

constexpr std::size_t searched_size = 32768;
constexpr std::size_t position_cache_size = 3;

struct alignas(64) Position_Cache {
    uint64_t searched_position[position_cache_size]{};
    Spin_Lock lock;
};

Position_Cache currently_searched[searched_size];

bool defer_position(uint64_t hash, int depth) {
    if (depth < DEFER_DEPTH) {
        return false;
    }
    std::size_t position = (hash + depth) & (searched_size - 1);
    Position_Cache& cache = currently_searched[position];
    std::lock_guard<Spin_Lock> guard(cache.lock);
    for (uint64_t stored_hash : cache.searched_position) {
        if (stored_hash == hash) {
            return true;
        }
    } // If we didn't find anything, we will search this position now
    for (uint32_t i = 0; i < position_cache_size; i++) {
        if (cache.searched_position[i] == 0) {
            cache.searched_position[i] = hash;
            break;
        } // If we don't find one, we might have two threads searching this position but that's ok if it happens rarely
    }
    return false;
}

void finished_search(uint64_t hash, int depth) {
    if (depth < DEFER_DEPTH) {
        return;
    }
    std::size_t position = (hash + depth) & (searched_size - 1);
    Position_Cache& cache = currently_searched[position];
    std::lock_guard<Spin_Lock> guard(cache.lock);
    for (uint64_t & stored_hash : cache.searched_position) {
        if (stored_hash == hash) {
            stored_hash = 0;
            return;
        }
    }
}

template<bool Q_SEARCH, TT_Strategy strategy>
class alignas (128) Simplified_ABDADA_Thread { // Let's go big with the alignas just in case

private:
    Board board;
    uint64_t nodes = 0;
    Locking_TT<strategy>& tt;
    std::atomic<bool>& finished;

    /**
     *
     * @param move Should be NO_Move, will contain the TT move if existing.
     * @param alpha Bound passed in by reference, will be updated
     * @param beta  Bound passed in by reference, will be updated
     * @param depth
     * @return true if the TT probe produced a cutoff, i.e. the search can be skipped, and the TT entry value,
     * stored in alpha, can be returned.
     */
    bool tt_probe(Move& move, Eval_Type& alpha, Eval_Type& beta, int depth) {
        Locked_TT_Info tt_entry{};
        if (tt.get_if_exists(board.hashKey, depth, tt_entry)) {
            assert(tt_entry.depth == depth);
            if (tt_entry.type == EXACT) {
                alpha = tt_entry.eval;
                return true;
            }
            if (tt_entry.type == UPPER_BOUND) {
                beta = std::min(beta, tt_entry.eval);
            } else if (tt_entry.type == LOWER_BOUND) {
                alpha = std::max(alpha, tt_entry.eval);
            }

            if (alpha >= beta) { // Our window is empty due to the TT hit
                alpha = tt_entry.eval;
                return true;
            }
            move = tt_entry.move;
        }
        if (move == NO_MOVE) { // If we didn't find a TT move, try from one depth earlier instead
            if (tt.get_if_exists(board.hashKey, depth - 1, tt_entry)) {
                assert(tt_entry.depth == depth - 1);
                move = tt_entry.move;
            }
        }
        return false;
    }

    template<Movetype TYPE>
    void generate_shuffled_moves(Movelist& moves) {
        Movegen::legalmoves<TYPE>(board, moves);
        thread_local static std::mt19937 mt(seed);

        for (int i = 0; i < moves.size; i++) {
            // Get a random index of the array past the current index.
            // ... The argument is an exclusive bound.
            //     It will not go past the array's end.
            int randomValue = i + (mt() % (moves.size - i)); // One could get rid of this mod but it doesn't appear to be a slowdown currently
            // Swap the random element with the present element.
            auto randomElement = moves[randomValue];
            moves[randomValue] = moves[i];
            moves[i] = randomElement;
        }
    }

public:
    explicit Simplified_ABDADA_Thread(Board& board, Locking_TT<strategy>& table, std::atomic<bool>& finished)
            : board(board), tt(table), finished(finished) {
    }

    Eval_Type q_search(Eval_Type alpha, Eval_Type beta) {
        Eval_Type q_eval = board.eval();
        if (q_eval < MIN_EVAL) { // Avoid overflow issues when inverting the eval.
            q_eval = MIN_EVAL;
        }
        nodes++;
        if constexpr (!Q_SEARCH) {
            return q_eval;
        }

        if (q_eval >= beta) {
            return q_eval;
        }
        if (q_eval > alpha) {
            alpha = q_eval;
        }

        Movelist captures;
        Movegen::legalmoves<CAPTURE>(board, captures);
        for (auto& capture : captures) {
            board.makeMove(capture.move);
            Eval_Type inner_eval = -q_search(-beta, -alpha);
            board.unmakeMove(capture.move);
            if (inner_eval > q_eval) {
                q_eval = inner_eval;
                if (q_eval >= beta) {
                    break;
                }
                if (q_eval > alpha) {
                    alpha = q_eval;
                }
            }
        }

        return q_eval;
    }

    Eval_Type nw_q_search(Eval_Type beta) {
        Eval_Type q_eval = board.eval();
        if (q_eval < MIN_EVAL) { // Avoid overflow issues when inverting the eval.
            q_eval = MIN_EVAL;
        }
        nodes++;
        if constexpr (!Q_SEARCH) {
            return q_eval;
        }

        if (q_eval >= beta) {
            return q_eval;
        }

        Movelist captures;
        Movegen::legalmoves<CAPTURE>(board, captures);
        for (auto& capture : captures) {
            board.makeMove(capture.move);
            Eval_Type inner_eval = -nw_q_search(-beta + 1);
            board.unmakeMove(capture.move);
            if (inner_eval > q_eval) {
                q_eval = inner_eval;
                if (q_eval >= beta) {
                    break;
                }
            }
        }

        return q_eval;
    }

    Eval_Type null_window_search(Eval_Type beta, int depth) {
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        Eval_Type alpha = beta - 1;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        Locked_TT_Info entry{eval, tt_move, (int8_t) depth, UPPER_BOUND}; // If we don't find a move, keep the old TT move
        Movelist moves;
        generate_shuffled_moves<ALL>(moves); // We could stop shuffling at low enough depth; won't gain much speedup,
        // but if we had proper move ordering it might produce faster cutoffs
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        std::vector<Move> deferred_moves{};
        deferred_moves.reserve(moves.size);

        for (int i = 0; i < moves.size; i++) {
            auto move = moves[i].move;
            board.makeMove(move);
            if (i != 0 && defer_position(board.hashKey, depth - 1)) {
                deferred_moves.emplace_back(move);
                board.unmakeMove(move);
                continue;
            }

            Eval_Type inner_eval;
            if (depth > 1) {
                inner_eval = -null_window_search(-beta + 1, depth - 1);
            } else {
                inner_eval = -nw_q_search(-beta + 1);
            }
            finished_search(board.hashKey, depth - 1); // Call this before we unmove and change the hashkey.
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move;
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
            }
            if (finished) { // If someone else already completed the search there is no reason for us to continue
                return eval;
            }
        }

        for (auto move : deferred_moves) {
            board.makeMove(move);
            Eval_Type inner_eval = -null_window_search(-beta + 1, depth - 1);
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move;
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
            }
            if (finished) { // If someone else already completed the search there is no reason for us to continue
                return eval;
            }
        }

        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }

    Eval_Type pv_search(Eval_Type alpha, Eval_Type beta, int depth) {
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        Locked_TT_Info entry{eval, tt_move, (int8_t) depth, UPPER_BOUND}; // If we don't find a move, keep the old TT move
        Movelist moves;
        generate_shuffled_moves<ALL>(moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        std::vector<Move> deferred_moves{};
        deferred_moves.reserve(moves.size);

        bool search_full_window = true;
        for (int i = 0; i < moves.size; i++) {
            auto move = moves[i].move;
            board.makeMove(move);
            if (i != 0 && defer_position(board.hashKey, depth - 1)) {
                deferred_moves.emplace_back(move);
                board.unmakeMove(move);
                continue;
            }

            Eval_Type inner_eval;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if (search_full_window || (inner_eval = -null_window_search(-alpha, depth - 1)) > alpha) {
                finished_search(board.hashKey, depth - 1); // Full window search means we want help from other threads; this will get called again below but that's fine

                inner_eval = -pv_search(-beta, -alpha, depth - 1);
                search_full_window = false;
            }
            finished_search(board.hashKey, depth - 1); // Call this before we unmove and change the hashkey.
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

                if (finished) { // If someone else already completed the search there is no reason for us to continue
                    return eval;
                }
            }
        }

        for (auto move : deferred_moves) {
            board.makeMove(move);
            Eval_Type inner_eval;
            if ((inner_eval = -null_window_search(-alpha, depth - 1)) > alpha) {
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
            }
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

                if (finished) { // If someone else already completed the search there is no reason for us to continue
                    return eval;
                }
            }
        }

        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }

    Eval_Type nega_max(Eval_Type alpha, Eval_Type beta, int depth) {
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        Locked_TT_Info entry{eval, tt_move, (int8_t) depth, UPPER_BOUND}; // If we don't find a move, keep the old TT move
        Movelist moves;
        generate_shuffled_moves<ALL>(moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        std::vector<Move> deferred_moves{};
        deferred_moves.reserve(moves.size);

        for (int i = 0; i < moves.size; i++) {
            auto move = moves[i].move;
            board.makeMove(move);
            if (i != 0 && defer_position(board.hashKey, depth - 1)) {
                deferred_moves.emplace_back(move);
                board.unmakeMove(move);
                continue;
            }

            Eval_Type inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                inner_eval = -q_search(-beta, -alpha);
            }
            finished_search(board.hashKey, depth - 1); // Call this before we unmove and change the hashkey.
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

                if (finished) { // If someone else already completed the search there is no reason for us to continue
                    return eval;
                }
            }
        }

        for (auto move : deferred_moves) {
            board.makeMove(move);
            Eval_Type inner_eval = -nega_max(-beta, -alpha, depth - 1);
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

                if (finished) { // If someone else already completed the search there is no reason for us to continue
                    return eval;
                }
            }
        }

        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }

    template<class Search_Result, bool PV_Search>
    void root_max(Eval_Type alpha, Eval_Type beta, int depth, Search_Result& result, std::atomic<uint64_t>& total_node_count) {
        auto start = std::chrono::high_resolution_clock::now();
        nodes = 0;
        assert(depth > 0);
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // This can probably never happen but maybe in parallel search
            return; // I'm claiming that if this happens, then we already have a search result from another thread so we don't need to return anything
        }

        Movelist moves;
        generate_shuffled_moves<ALL>(moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        std::vector<Move> deferred_moves{};
        deferred_moves.reserve(moves.size);

        Move best_move = NO_MOVE;

        bool search_full_window = true;
        for (int i = 0; i < moves.size; i++) {
            auto move = moves[i].move;
            board.makeMove(move);
            if (i != 0 && defer_position(board.hashKey, depth - 1)) {
                deferred_moves.emplace_back(move);
                board.unmakeMove(move);
                continue;
            }

            Eval_Type inner_eval;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if constexpr (!PV_Search) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else if (search_full_window || (inner_eval = -null_window_search(-alpha, depth - 1)) > alpha) {
                finished_search(board.hashKey, depth - 1); // Full window search means we want help from other threads; this will get called again below but that's fine
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
                search_full_window = false;
            }
            if constexpr (DEBUG_OUTPUTS) {
                std::cout << convertMoveToUci(move) << " eval " << inner_eval << " nodes " << nodes << std::endl;
                std::cout << convertMoveToUci(move) << " ";
                tt.print_pv(board, depth - 1);
                tt.print_size();
            }
            finished_search(board.hashKey, depth - 1); // Full window search means we want help from other threads; this will get called again below but that's fine
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                best_move = move;
                if (eval >= beta) {
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                }

                if (finished) {
                    total_node_count += nodes;
                    return;
                }
            }
        }

        for (auto move : deferred_moves) {
            board.makeMove(move);
            Eval_Type inner_eval;
            if constexpr (!PV_Search) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else if ((inner_eval = -null_window_search(-alpha, depth - 1)) > alpha) {
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
            }
            if constexpr (DEBUG_OUTPUTS) {
                std::cout << convertMoveToUci(move) << " eval " << inner_eval << " nodes " << nodes << std::endl;
                std::cout << convertMoveToUci(move) << " ";
                tt.print_pv(board, depth - 1);
                tt.print_size();
            }
            board.unmakeMove(move);

            if (inner_eval > eval) {
                eval = inner_eval;
                best_move = move;
                if (eval >= beta) {
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                }

                if (finished) {
                    total_node_count += nodes;
                    return;
                }
            }
        }

        tt.emplace(board.hashKey, {eval, best_move, (int8_t) depth, EXACT}, depth);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        bool i_am_first = !finished.exchange(true); // Setting finished to true tells all threads to finish.
        // Surprisingly, this can lead to a slowdown at low depths, in testing up to depth 9 which does take multiple
        // seconds. However, for depth 10 and much more so depth 11 this leads to a big speedup.

        if (i_am_first) { // The first thread to finish gets to write the search result
            result.duration = duration.count();
            result.move = best_move;
            result.eval = eval;
            result.depth = depth;
        }
        total_node_count += nodes;
    }
};

template<bool Q_SEARCH, TT_Strategy strategy>
class Simplified_ABDADA_Search {

    std::atomic<bool> finished = false;
    size_t num_threads;
    std::vector<Simplified_ABDADA_Thread<Q_SEARCH, strategy>> searchers;

public:
    Simplified_ABDADA_Search(size_t num_threads, Board& board, Locking_TT<strategy>& table) : num_threads(num_threads),
                                                                  searchers(num_threads, Simplified_ABDADA_Thread<Q_SEARCH, strategy>(board, table, finished)) {
    }

    /**
     *
     * @tparam Search_Result
     * @tparam PV_Search
     * @param up_to_depth Search for each depth from 1 to up_to_depth through iterative deepening.
     * @param iteration Optional parameter, if passed will be printed in the output. Useful for automated benchmarks.
     * @return
     */
    template<class Search_Result, bool PV_Search>
    Search_Result parallel_search(int up_to_depth, int iteration = 0) {
        Search_Result result;
        for (int depth = 1; depth <= up_to_depth; depth++) {
            std::vector<std::thread> search_threads;
            Eval_Type alpha = MIN_EVAL;
            Eval_Type beta = MAX_EVAL;
            finished = false;
            std::atomic<uint64_t > node_count = 0;
            for (size_t i = 0; i < num_threads; i++) {
                auto func = std::bind(&Simplified_ABDADA_Thread<Q_SEARCH, strategy>::template root_max<Search_Result, PV_Search>,
                                      &searchers[i], alpha, beta, depth, std::ref(result), std::ref(node_count));
                search_threads.emplace_back(func);
            }
            for (auto &thread: search_threads) {
                thread.join();
            }
            result.nodes = node_count;
            result.print_table(iteration, num_threads);
        }
        return result;
    }
};
