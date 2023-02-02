#pragma once

#include "chess.hpp"

class Search {

private:
    Board board;
    uint64_t nodes = 0;

public:
    explicit Search(Board& board) : board(board) {
    }

    int q_search() {
        int q_eval = eval(board);
        nodes++;
        /*Movelist captures;
        Movegen::legalmoves<CAPTURE>(board, captures);
        for (auto& capture : captures) {
            board.makeMove(capture.move);
            int inner_eval = -q_search();
            if (inner_eval > q_eval) {
                q_eval = inner_eval;
            }
            board.unmakeMove(capture.move);
        }*/

        return q_eval;
    }

    int nega_max(int depth) {
        if (depth == 0) {
            nodes++;
            return eval(board);
        }
        int eval = INT32_MIN;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);

        for (auto& move : moves) {
            board.makeMove(move.move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(depth - 1);
            } else {
                inner_eval = -q_search();
            }

            if (inner_eval > eval) {
                eval = inner_eval;
            }
            board.unmakeMove(move.move);
        }
        return eval;
    }

    int root_max(int depth) {
        nodes = 0;
        if (depth == 0) {
            nodes++;
            return q_search();
        }
        int eval = INT32_MIN;
        Movelist moves;
        Movegen::legalmoves<ALL>(board, moves);
        Move best_move = NO_MOVE;
        for (auto& move_container : moves) {
            auto move = move_container.move;
            board.makeMove(move);
            int inner_eval;
            if (depth > 1) {
                inner_eval = -nega_max(depth - 1);
            } else {
                inner_eval = -q_search();
            }

            if (inner_eval > eval) {
                eval = inner_eval;
                best_move = move;
            }
            board.unmakeMove(move);
        }
        std::cout << "Depth " << depth << ": " << convertMoveToUci(best_move) << " eval " << eval << " nodes " << nodes << std::endl;
        return eval;
    }
};