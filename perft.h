#pragma once

#include <sstream>
#include <chrono>
#include <iostream>
#include <iomanip>
#include "perft_tt.h"
#include "chess-library/src/chess.hpp"

using namespace Chess;

template<class T>
class PerftTest
{
private:
    Perft_TT& simpleTT;
    uint64_t position_generations = 0;
public:
    PerftTest(T& TT) : simpleTT(TT) {
    }

    uint64_t hash_perft(Board &board, int depth) {
        if (depth == 0) {
            return 1;
        }
        if (depth > 1 && simpleTT.contains(board.hashKey + depth)) {
            return simpleTT.at(board.hashKey + depth);
        }

        uint64_t nodes = 0;
        Movelist moveslist;
        Movegen::legalmoves<ALL>(board, moveslist);
        position_generations++;

        if (depth == 1)
        {
            nodes = moveslist.size;
        } else {
            for (int i = 0; i < moveslist.size; i++) {
                Move &move = moveslist[i].move;
                board.makeMove(move);
                nodes += hash_perft(board, depth - 1);
                board.unmakeMove(move);
            }
            simpleTT.emplace(board.hashKey + depth, nodes);
        }
        return nodes;
    }

    /**
     * Standard perft implementation from the library
     * @param board
     * @param depth
     * @return
     */
    uint64_t perft(Board &board, int depth)
    {
        Movelist moveslist;
        Movegen::legalmoves<ALL>(board, moveslist);

        if (depth == 1)
        {
            return int(moveslist.size);
        }

        uint64_t nodes = 0;

        for (int i = 0; i < int(moveslist.size); i++)
        {
            Move move = moveslist[i].move;
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.unmakeMove(move);
        }

        return nodes;
    }

    /**
     * Taken from the library code
     * @param board
     * @param depth
     * @param expectedNodeCount
     * @return
     */
    uint64_t testPositionPerft(Board &board, int depth, uint64_t expectedNodeCount)
    {
        std::stringstream ss;

        const auto t1 = std::chrono::high_resolution_clock::now();
        const uint64_t n = hash_perft(board, depth);
        const auto t2 = std::chrono::high_resolution_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        if (n != expectedNodeCount)
        {
            ss << "Wrong node count ";
        }

        ss << "depth " << std::left << std::setw(2) << depth << " time " << std::setw(5) << ms << " nodes "
           << std::setw(12) << n << " nps " << std::setw(9) << (n * 1000) / (ms + 1) << " fen " << std::setw(87)
           << board.getFen();
        ss << "\n" << "Move generator calls " << position_generations << ", per second: " << (position_generations * 1000) / (ms + 1);
        std::cout << ss.str() << std::endl;

        return n;
    }

    static void start_pos_perfts() {
        uint64_t results[] = { 1, 20, 400, 8902, 197281, 4865609, 119060324, 3195901860,
                               84998978956, 2439530234167, 69352859712417 };
        Perft_TT perftTT{};
        for (int depth = 0; depth < 11; depth++) {
            Board board = Board(DEFAULT_POS);
            auto perft_improved = PerftTest<Perft_TT>(perftTT);
            perft_improved.testPositionPerft(board, depth, results[depth]);
            perftTT.print_size();
        }
    }
};

