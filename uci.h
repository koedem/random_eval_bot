#pragma once

#include <iostream>
#include <string>

#include "chess.hpp"
#include "simplified_abdada.h"


struct Search_Result {
    uint64_t nodes = 0;
    double duration = 0;
    Move move = Chess::NO_MOVE;
    Eval_Type eval = 0;
    uint16_t depth = 0;

    void print_human_readable() const {
        std::cout << "Depth " << depth << ": " << convertMoveToUci(move) << " eval " << eval << " nodes " << nodes
                  << " time " << duration << " nps " << (nodes / duration) << std::endl;
    }

    void print_table(int iteration, int num_threads) const {
        std::cout << iteration << "\t" << depth << "\t" << duration << "\t" << (nodes / duration) << "\t" << eval
                  << "\t" << nodes << "\t";

    }
};

class UCI {
    Board board;
    Locking_TT<REPLACE_LAST_ENTRY> table{16384};

public:
    void uci_loop() {
        std::string command;
        constexpr bool q_search = false;
        while (getline(std::cin, command), command != "quit") {
            if (command.starts_with("position fen ")) {
                board.eval();
                board.applyFen(command.substr(13));
                std::cout << board;
            } else if (command.starts_with("go depth")) {
                int depth = std::stoi(command.substr(8));
                Simplified_ABDADA_Search<q_search, REPLACE_LAST_ENTRY> search(10, board, table);
                search.parallel_search<Search_Result, true>(depth);
            } else if (command == "selfplay") {
                std::string full_game;
                int depth = 11;
                bool mate = false;
                while (!mate) {
                    table.clear();
                    Simplified_ABDADA_Search<q_search, REPLACE_LAST_ENTRY> search(10, board, table);
                    auto result = search.parallel_search<Search_Result, true>(depth);
                    board.makeMove(result.move);
                    full_game.append(convertMoveToUci(result.move)).append(" ");

                    Movelist moves;
                    Movegen::legalmoves<ALL>(board, moves);
                    if (moves.size == 0) {
                        mate = true;
                    }
                    if (full_game.length() > 2000) {
                        break;
                    }
                }
                std::cout << full_game << std::endl;
            }
        }
    }
};