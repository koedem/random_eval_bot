#include <iostream>
#include "perft.h"
#include "chess.hpp"
#include "sequential_search.h"
#include "chess-library/src/chess.hpp"

struct Search_Result {
    uint64_t nodes = 0;
    double duration = 0;
    Move move = Chess::NO_MOVE;
    Eval_Type eval = 0;
    uint16_t depth = 0;

    void print() {
        std::cout << "Depth " << depth << ": " << convertMoveToUci(move) << " eval " << eval << " nodes " << nodes
                  << " time " << duration << " nps " << (nodes / duration) << std::endl;
    }

    void print_table(int iteration) {
        std::cout << iteration << "\t" << depth << "\t" << duration << "\t" << (nodes / duration) << "\t" << eval
        << "\t" << nodes << "\t" << convertMoveToUci(move) << std::endl;
    }
};

int main() {
    init_tables();

    Board board;
    Transposition_Table<REPLACE_LAST_ENTRY> table(16384);
    Search<true, REPLACE_LAST_ENTRY> search(board, table);
    for (int iteration = 0; iteration < 10; iteration++) {
        for (int depth = 1; depth < 14; depth++) {
            Search_Result result;
            search.root_max<Search_Result, true>(INT16_MIN + 1, INT16_MAX, depth, result);
            result.print_table(iteration);
            table.print_size();
            //search.root_max<Search_Result, false>(INT32_MIN / 2, INT32_MAX / 2, depth, result);
            //result.print_table(iteration);
        }
        table.clear();
        //change_seed();
    }
    return 0;
}