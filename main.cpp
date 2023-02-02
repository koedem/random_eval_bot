#include <iostream>
#include "perft.h"
#include "chess.hpp"
#include "sequential_search.h"


int main() {
    init_tables();

    Board board;
    Search search(board);
    for (int depth = 1; depth < 7; depth++) {
        search.root_max(depth);
    }
    std::cout << eval(board) << std::endl;
    board.movePiece(WhitePawn, SQ_E2, SQ_E4);
    std::cout << eval(board) << std::endl;
    board.movePiece(BlackKnight, SQ_G8, SQ_F6);
    std::cout << eval(board) << std::endl;
    board.movePiece(WhitePawn, SQ_D2, SQ_D4);
    std::cout << eval(board) << std::endl;
    board.movePiece(BlackKnight, SQ_F6, SQ_E4);
    std::cout << eval(board) << std::endl;
    return 0;
}