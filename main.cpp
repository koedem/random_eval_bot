#include <iostream>
#include "perft.h"
#include "sequential_search.h"
#include "simple_concurrent_search.h"
#include "abdada_tt.h"
#include "abdada_search.h"

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
    Locking_TT<REPLACE_LAST_ENTRY> locking_tt(256);

    Search<true, REPLACE_LAST_ENTRY> search(board, table);
    size_t num_threads = 11;
    Lazy_SMP<true, REPLACE_LAST_ENTRY> lazy_smp(num_threads, board, locking_tt);

    ABDADA_TT<REPLACE_LAST_ENTRY> abdada_tt(256);
    ABDADA_Search<true, REPLACE_LAST_ENTRY> abdada(num_threads, board, abdada_tt);
    for (int iteration = 0; iteration < 20; iteration++) {
        int up_to_depth = 11;
        lazy_smp.parallel_search<Search_Result, true>(up_to_depth, iteration);
            //result.print_table(iteration);
            //locking_tt.print_size();
            //locking_tt.print_pv(board, depth);
        locking_tt.clear();
    }
    return 0;
}