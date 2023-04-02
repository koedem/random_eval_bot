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

    void print_table(int iteration) const {
        std::cout << iteration << "\t" << depth << "\t" << duration << "\t" << (nodes / duration) << "\t" << eval
                  << "\t" << nodes << "\t";

    }

    void print_uci() const {
        int millis = (int) (duration * 1000);
        std::cout << "info depth " << depth << " score cp " << eval / 100 << " time " << millis << " nodes " << nodes
                  << " nps " << (nodes / (millis / 1000 + 1)) << " pv ";
    }
};

class UCI {
    Board board;
    Locking_TT<REPLACE_LAST_ENTRY> table{256};

public:
    void uci_loop() {
        std::string command;
        constexpr bool q_search = false;
        while (getline(std::cin, command), command != "quit") {
            if (command == "uci") {
                std::cout << "uciok" << std::endl;
            } else if (command == "isready") {
                std::cout << "readyok" << std::endl;
            } else if (command == "ucinewgame") {
                table.clear();
                board.applyFen(DEFAULT_POS);
            } else if (command.starts_with("position fen ")) {
                table.clear();
                board.eval();
                board.applyFen(command.substr(13));
                std::cout << board;
            } else if (command.starts_with("position startpos moves ")) {
                table.clear();
                board.applyFen(DEFAULT_POS);
                auto moves = splitInput(command.substr(24));
                for (auto move : moves) {
                    board.makeMove(convertUciToMove(board, move));
                }
            } else if (command.starts_with("go depth")) {
                table.clear();
                int depth = std::stoi(command.substr(8));
                Simplified_ABDADA_Search<q_search, REPLACE_LAST_ENTRY> search(10, board, table);
                search.parallel_search<Search_Result, true>(depth);
            } else if (command.starts_with("go movetime")) {
                table.clear();
                int depth = DEFAULT_DEPTH;
                Simplified_ABDADA_Search<q_search, REPLACE_LAST_ENTRY> search(10, board, table);
                auto result = search.parallel_search<Search_Result, true>(depth);
                std::cout << "bestmove " << convertMoveToUci(result.move) << std::endl;
            } else if (command == "selfplay") {
                std::string full_game;
                int depth = 9;
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