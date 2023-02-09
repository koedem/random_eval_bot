#include <iostream>
#include "perft.h"
#include "sequential_search.h"
#include "simple_concurrent_search.h"

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
    Board board;
    Transposition_Table<REPLACE_LAST_ENTRY> table(16);
    Locking_TT<REPLACE_LAST_ENTRY> locking_tt(16384);

    Search<true, REPLACE_LAST_ENTRY> search(board, table);
    Lazy_SMP<true, REPLACE_LAST_ENTRY> lazy_smp(1, board, locking_tt);
    for (int iteration = 0; iteration < 10; iteration++) {
        for (int depth = 1; depth < 15; depth++) {
            Search_Result result;
            lazy_smp.root_max<Search_Result, true>(INT16_MIN + 1, INT16_MAX, depth, result);
            //result.print_table(iteration);
            //locking_tt.print_size();
            //locking_tt.print_pv(board, depth);
        }
        locking_tt.clear();
    }
    return 0;
}