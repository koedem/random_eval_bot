#include <iostream>
#include "perft.h"
#include "chess.hpp"
#include "sequential_search.h"


int main() {
    init_tables();

    Eval_Board board;
    Search<true> search(board);
    for (int depth = 1; depth < 10; depth++) {
        search.root_max(INT32_MIN / 2, INT32_MAX / 2, depth);
    }
    std::cout << board.eval() << std::endl;
    board.movePiece(WhitePawn, SQ_E2, SQ_E4);
    std::cout << board.eval() << std::endl;
    board.movePiece(BlackKnight, SQ_G8, SQ_F6);
    std::cout << board.eval() << std::endl;
    board.movePiece(WhitePawn, SQ_D2, SQ_D4);
    std::cout << board.eval() << std::endl;
    board.movePiece(BlackKnight, SQ_F6, SQ_E4);
    std::cout << board.eval() << std::endl;
    return 0;
}