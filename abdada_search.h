#pragma once

#include <thread>
#include <functional>
#include "locking_tt.h"
#include "abdada_tt.h"
#include "compile_time_constants.h"

template<bool Q_SEARCH, TT_Strategy strategy>
class alignas (128) ABDADA_Thread { // Let's go big with the alignas just in case

private:
    Board board;
    uint64_t nodes = 0;
    ABDADA_TT<strategy>& tt;
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
    bool tt_probe(Move& move, Eval_Type& alpha, Eval_Type& beta, int depth, bool exclusive) {
        ABDADA_TT_Info tt_entry{};
        if (tt.template get_if_exists<true>(board.hashKey, depth, tt_entry, exclusive)) {
            assert(tt_entry.depth == depth);
            assert(tt_entry.eval != ON_EVALUATION);

            if (exclusive && tt_entry.proc_number > 0) { // No proc incremented
                alpha = ON_EVALUATION;
                return true; // "Cutoff" because another thread is already searching this node.
            }

            if (tt_entry.type == EXACT) { // No proc incremented
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
                tt.decrement_proc(board.hashKey, depth); // We incremented this and now skip the search, so decrement again.
                return true;
            }
            move = tt_entry.move;
        }
        if (move == NO_MOVE) { // If we didn't find a TT move, try from one depth earlier instead
            if (tt.template get_if_exists<false>(board.hashKey, depth - 1, tt_entry, exclusive)) {
                assert(tt_entry.depth == depth - 1);
                move = tt_entry.move;
            }
        }
        return false;
    }

    template<Movetype TYPE>
    void generate_shuffled_moves(Movelist& moves) {
        Movegen::legalmoves<TYPE>(board, moves);
        thread_local static std::mt19937 mt;

        for (int i = 0; i < moves.size; i++) {
            // Get a random index of the array past the current index.
            // ... The argument is an exclusive bound.
            //     It will not go past the array's end.
            int randomValue = i + (mt() % (moves.size - i)); // One could try to get rid of this mod but appears not worth it in terms of performance.
            // Swap the random element with the present element.
            auto randomElement = moves[randomValue];
            moves[randomValue] = moves[i];
            moves[i] = randomElement;
        }
    }

public:
    explicit ABDADA_Thread(Board& board, ABDADA_TT<strategy>& table, std::atomic<bool>& finished)
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

    Eval_Type null_window_search(Eval_Type beta, int depth, bool exclusive) {
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        Eval_Type alpha = beta - 1;
        if (tt_probe(tt_move, alpha, beta, depth, exclusive)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        ABDADA_TT_Info entry{eval, tt_move, (int8_t) depth, UPPER_BOUND, 0}; // If we don't find a move, keep the old TT move
        Movelist moves;
        generate_shuffled_moves<ALL>(moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        std::vector<Move> deferred_moves{};
        deferred_moves.reserve(moves.size);

        for (int move_index = 0; move_index < moves.size; move_index++) {
            Move move = moves[move_index].move;
            board.makeMove(move);
            Eval_Type inner_eval;
            if (depth > 1) {
                inner_eval = -null_window_search(-beta + 1, depth - 1, move_index != 0);
                if (inner_eval == (Eval_Type) -ON_EVALUATION) {
                    deferred_moves.emplace_back(move);
                }
            } else {
                inner_eval = -nw_q_search(-beta + 1);
            }
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
                tt.decrement_proc(board.hashKey, depth); // We stop searching
                return eval;
            }
        }

        for (Move move : deferred_moves) {
            board.makeMove(move);
            Eval_Type inner_eval;
            inner_eval = -null_window_search(-beta + 1, depth - 1, false);
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
                tt.decrement_proc(board.hashKey, depth); // We stop searching
                return eval;
            }
        }

        entry.eval = eval;
        tt.template emplace<true>(board.hashKey, entry, depth);
        return eval;
    }

    Eval_Type pv_search(Eval_Type alpha, Eval_Type beta, int depth) {
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth, false)) { // I.e. if cutoff
            return alpha; // TT entry value is put here
        }

        ABDADA_TT_Info entry{eval, tt_move, (int8_t) depth, UPPER_BOUND, 0}; // If we don't find a move, keep the old TT move
        Movelist moves;
        generate_shuffled_moves<ALL>(moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }

        std::vector<Move> deferred_moves{};
        deferred_moves.reserve(moves.size);

        bool search_full_window = true; // TODO can this be removed?
        for (int move_index = 0; move_index < moves.size; move_index++) {
            Move move = moves[move_index].move;
            board.makeMove(move);
            Eval_Type inner_eval = MAX_EVAL; // Hack so that further down below inner eval is bigger than alpha if no search was done
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else {
                if (!search_full_window) {
                    inner_eval = -null_window_search(-alpha, depth - 1, true);
                    if (inner_eval == (Eval_Type) -ON_EVALUATION) {
                        deferred_moves.emplace_back(move);
                    }
                }
                if (inner_eval > alpha) {
                    inner_eval = -pv_search(-beta, -alpha, depth - 1);
                    search_full_window = false;
                }
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
                    tt.decrement_proc(board.hashKey, depth); // We stop searching
                    return eval;
                }
            }
        }

        for (auto& move : deferred_moves) { // We never enter this at depth 1, also never search full window right away.
            board.makeMove(move);
            Eval_Type inner_eval = -null_window_search(-alpha, depth - 1, false);
            if (inner_eval > alpha) {
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
                    tt.decrement_proc(board.hashKey, depth); // We stop searching
                    return eval;
                }
            }
        }

        entry.eval = eval;
        tt.template emplace<true>(board.hashKey, entry, depth);
        return eval;
    }

    /**
     *
     * @param alpha
     * @param beta
     * @param depth
     * @param exclusive_searched This parameter does double duties. It should be passed in whether this node should be
     * calculated exclusively. In this case, if another thread already calculates this node, we don't return a result.
     * However, upon returning this bool contains whether we actually searched the node. If no, this node should be
     * recalculated later.
     * @return
     */
    /*Eval_Type nega_max(Eval_Type alpha, Eval_Type beta, int depth, bool& exclusive_searched) {
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth)) { // I.e. if cutoff
            exclusive_searched = true;
            return alpha; // TT entry value is put here
        }

        Locked_TT_Info entry{eval, tt_move, (int8_t) depth, UPPER_BOUND}; // If we don't find a move, keep the old TT move
        Movelist moves;
        generate_shuffled_moves<ALL>(moves);
        int tt_move_index = moves.find(tt_move);
        if (tt_move_index > 0) {
            std::swap(moves[0], moves[tt_move_index]); // Search the TT move first
        }
        for (auto& move : moves) {
            board.makeMove(move.move);
            Eval_Type inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                inner_eval = -q_search(-beta, -alpha);
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                entry.move = move.move; // If it stays this way, this is the best move
                if (eval >= beta) {
                    entry.type = LOWER_BOUND;
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                    entry.type = EXACT; // We raised alpha, so it's no longer a lower bound, either exact or upper bound
                }

                if (finished) { // If someone else already completed the search there is no reason for us to continue
                    tt.decrement_proc(board.hashKey, depth); // We stop searching
                    return eval;
                }
            }
        }
        entry.eval = eval;
        tt.emplace(board.hashKey, entry, depth);
        return eval;
    }*/

    template<class Search_Result, bool PV_Search>
    void root_max(Eval_Type alpha, Eval_Type beta, int depth, Search_Result& result) {
        auto start = std::chrono::high_resolution_clock::now();
        nodes = 0;
        assert(depth > 0);
        Eval_Type eval = MIN_EVAL;
        Move tt_move = NO_MOVE;
        if (tt_probe(tt_move, alpha, beta, depth, false)) { // This can probably never happen but maybe in parallel search
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
        for (int move_index = 0; move_index < moves.size; move_index++) {
            Move move = moves[move_index].move;
            board.makeMove(move);
            Eval_Type inner_eval = MAX_EVAL;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if constexpr (!PV_Search) {
                //inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                if (!search_full_window) {
                    inner_eval = -null_window_search(-alpha, depth - 1, true);
                    if (inner_eval == (Eval_Type) -ON_EVALUATION) {
                        deferred_moves.emplace_back(move);
                    }
                }
                if (inner_eval > alpha){
                    inner_eval = -pv_search(-beta, -alpha, depth - 1);
                    search_full_window = false;
                }
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
                    tt.decrement_proc(board.hashKey, depth); // We stop searching
                    return;
                }
            }
        }

        for (Move move : deferred_moves) {
            board.makeMove(move);
            Eval_Type inner_eval = MAX_EVAL;
            if constexpr (!PV_Search) {
                //inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                inner_eval = -null_window_search(-alpha, depth - 1, true);
                if (inner_eval > alpha){
                    inner_eval = -pv_search(-beta, -alpha, depth - 1);
                }
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
                    tt.decrement_proc(board.hashKey, depth); // We stop searching
                    return;
                }
            }
        }


        tt.template emplace<true>(board.hashKey, {eval, best_move, (int8_t) depth, EXACT, 0}, depth);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        bool i_am_first = !finished.exchange(true);

        if (i_am_first) { // The first thread to finish gets to write the search result
            result.nodes = nodes;
            result.duration = duration.count();
            result.move = best_move;
            result.eval = eval;
            result.depth = depth;
        }
    }
};

template<bool Q_SEARCH, TT_Strategy strategy>
class ABDADA_Search {

    std::atomic<bool> finished = false;
    size_t num_threads;
    std::vector<ABDADA_Thread<Q_SEARCH, strategy>> searchers;

public:
    ABDADA_Search(size_t num_threads, Board& board, ABDADA_TT<strategy>& table) : num_threads(num_threads),
                                      searchers(num_threads, ABDADA_Thread<Q_SEARCH, strategy>(board, table, finished)) {
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
            Eval_Type alpha = MIN_EVAL; // Don't use INT16_MIN because negating it causes an overflow
            Eval_Type beta = MAX_EVAL;
            finished = false;
            for (size_t i = 0; i < num_threads; i++) {
                auto func = std::bind(&ABDADA_Thread<Q_SEARCH, strategy>::template root_max<Search_Result, PV_Search>,
                                      &searchers[i], alpha, beta, depth, std::ref(result));
                search_threads.emplace_back(func);
            }
            for (auto &thread: search_threads) {
                thread.join();
            }
            result.print_table(iteration);
        }
        return result;
    }
};
