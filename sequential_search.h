#pragma once

#include "chess.hpp"

template<bool Q_SEARCH>
class Search {

private:
    Board board;
    uint64_t nodes = 0;

public:
    explicit Search(Board& board) : board(board) {
    }

    int q_search(int alpha, int beta) {
        int q_eval = board.eval();
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
            int inner_eval = -q_search(-beta, -alpha);
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

    int nw_q_search(int beta) {
        int q_eval = board.eval();
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
            int inner_eval = -nw_q_search(-beta + 1);
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

    int null_window_search(int beta, int depth) {
        int eval = INT32_MIN / 2;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);

        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -null_window_search(-beta + 1, depth - 1);
            } else {
                inner_eval = -nw_q_search(-beta + 1);
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                if (eval >= beta) {
                    break;
                }
            }
        }
        return eval;
    }

    int pv_search(int alpha, int beta, int depth) {
        int eval = INT32_MIN / 2;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);

        bool search_full_window = true;
        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if (search_full_window || (inner_eval = -null_window_search(-alpha, depth - 1)) > alpha){
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
                search_full_window = false;
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                if (eval >= beta) {
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                }

            }
        }
        return eval;
    }

    int nega_max(int alpha, int beta, int depth) {
        int eval = INT32_MIN / 2;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);

        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                inner_eval = -q_search(-beta, -alpha);
            }
            board.unmakeMove(move.move);

            if (inner_eval > eval) {
                eval = inner_eval;
                if (eval >= beta) {
                    break;
                }
                if (eval > alpha) {
                    alpha = eval;
                }

            }
        }
        return eval;
    }

    template<class Search_Result, bool PV_Search>
    Search_Result root_max(int alpha, int beta, int depth, Search_Result& result) {
        auto start = std::chrono::high_resolution_clock::now();
        nodes = 0;
        assert(depth > 0);
        /*if (depth == 0) {
            nodes++;
            return q_search(-beta, -alpha);
        }*/
        int eval = INT32_MIN / 2;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);
        Move best_move = NO_MOVE;

        bool search_full_window = true;
        for (auto& move_container : moves) {
            auto move = move_container.move;
            board.makeMove(move);
            int inner_eval;
            if (depth == 1) {
                inner_eval = -q_search(-beta, -alpha);
            } else if constexpr (!PV_Search) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else if (search_full_window || (inner_eval = -null_window_search(-alpha, depth - 1)) > alpha){
                inner_eval = -pv_search(-beta, -alpha, depth - 1);
                std::cout << convertMoveToUci(move) << " eval " << inner_eval << " nodes " << nodes << std::endl;
                search_full_window = false;
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
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        result.nodes = nodes;
        result.duration = duration.count();
        result.move = best_move;
        result.eval = eval;
        result.depth = depth;
        return result;
    }
};