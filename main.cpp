#include <iostream>
#include <fstream>
#include "perft.h"
#include "sequential_search.h"
#include "simple_concurrent_search.h"
#include "abdada_tt.h"
#include "abdada_search.h"

std::ofstream out;

template <class T>
void print(const T& t, size_t w)
{
    out.width(w);
    out  << t << " " << std::flush;
    std::cout.width(w);
    std::cout << t << " " << std::flush;
}

void print_headline()
{
    print("it", 3);
    print("num_thr", 7);
    print("ply", 4);
    print("time" , 11);
    print("nps", 9);
    print("eval", 5);
    print("nodes", 15);
    print("move", 5);
    out       << std::endl;
    std::cout << std::endl;
}

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
        if constexpr (PRINT_TO_FILE) {
            print_for_file(iteration, num_threads);
        } else {
            std::cout << iteration << "\t" << depth << "\t" << duration << "\t" << (nodes / duration) << "\t" << eval
                      << "\t" << nodes << "\t" << convertMoveToUci(move) << std::endl;
        }
    }

    void print_for_file(int iteration, int num_threads) const {
        print(iteration, 3);
        print(num_threads, 7);
        print(depth, 4);
        print(duration, 11);
        print((std::size_t) (nodes / duration), 9);
        print(eval, 5);
        print(nodes, 15);
        print(convertMoveToUci(move), 5);
        out << std::endl;
        std::cout << std::endl;
    }
};

template<class Transposition_Table, class Search>
void run_tests(Board& board, std::size_t hash_size, std::size_t max_threads, int depth_limit, int number_of_iterations) {
    Transposition_Table tt(hash_size);
    for (int iteration = 0; iteration < number_of_iterations; iteration++) {
        for (std::size_t num_threads = 1; num_threads <= max_threads; num_threads++) {
            Search search(num_threads, board, tt);
            int up_to_depth = depth_limit;
            search.template parallel_search<Search_Result, true>(up_to_depth, iteration);
            tt.clear();
        }
    }
}

int main() {
    Board board;

    int depth = 8;
    std::size_t max_threads = std::thread::hardware_concurrency();
    int hash_size = 16384;
    int iterations = 2;

    out = std::ofstream("./pos1_16384_abdada.txt");
    print_headline();
    run_tests<ABDADA_TT<REPLACE_LAST_ENTRY>,ABDADA_Search<true, REPLACE_LAST_ENTRY>>(board, hash_size, max_threads, depth, iterations);

    hash_size = 1024;
    out = std::ofstream("./pos1_1024_abdada.txt");
    print_headline();
    run_tests<ABDADA_TT<REPLACE_LAST_ENTRY>,ABDADA_Search<true, REPLACE_LAST_ENTRY>>(board, hash_size, max_threads, depth, iterations);


    //Lockless_TT<REPLACE_LAST_ENTRY> lockless(128);
    return 0;
}