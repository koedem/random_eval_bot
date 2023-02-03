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

    int q_search(int alpha, int beta, int depth = 0) {
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
            int inner_eval = -q_search(-beta, -alpha, depth + 1);
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

    int nega_max(int alpha, int beta, int depth) {
        int eval = INT32_MIN;
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

    int root_max(int alpha, int beta, int depth) {
        auto start = std::chrono::high_resolution_clock::now();
        nodes = 0;
        if (depth == 0) {
            nodes++;
            return q_search(-beta, -alpha);
        }
        int eval = INT32_MIN;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);
        Move best_move = NO_MOVE;
        for (auto& move_container : moves) {
            auto move = move_container.move;
            //std::cout << convertMoveToUci(move) << " eval " << eval << " nodes " << nodes << std::endl;
            board.makeMove(move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(-beta, -alpha, depth - 1);
            } else {
                inner_eval = -q_search(-beta, -alpha);
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
        std::cout << "Depth " << depth << ": " << convertMoveToUci(best_move) << " eval " << eval << " nodes " << nodes
            << " time " << duration.count() << " nps " << (nodes / duration.count()) << std::endl;
        return eval;
    }
};