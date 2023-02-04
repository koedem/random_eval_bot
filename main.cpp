#include <iostream>
#include "perft.h"
#include "chess.hpp"
#include "sequential_search.h"
#include "chess-library/src/chess.hpp"

struct Search_Result {
    uint64_t nodes = 0;
    double duration = 0;
    Move move = Chess::NO_MOVE;
    int16_t eval = 0;
    uint16_t depth = 0;

    void print() {
        std::cout << "Depth " << depth << ": " << convertMoveToUci(move) << " eval " << eval << " nodes " << nodes
                  << " time " << duration << " nps " << (nodes / duration) << std::endl;
    }
};

int main() {
    init_tables();

    Board board;
    Search<true> search(board);
    for (int depth = 1; depth < 9; depth++) {
        Search_Result result;
        search.root_max(INT32_MIN / 2, INT32_MAX / 2, depth, result);
        result.print();
    }
    return 0;
}