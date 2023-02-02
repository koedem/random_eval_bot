#pragma once

#include "chess-library/src/chess.hpp"

using namespace Chess;

enum Eval_Type {
    Incremental_PST,
    Full_PST,
    Random
};

constexpr static Eval_Type EVAL_TYPE = Full_PST;

inline void Board::movePiece(Piece piece, Square fromSq, Square toSq) {
    removePiece(piece, fromSq);
    placePiece(piece, toSq);
}

inline void Board::removePiece(Piece piece, Square sq)
{
    piecesBB[piece] &= ~(1ULL << sq);
    board[sq] = None;
}

inline void Board::placePiece(Piece piece, Square sq)
{
    piecesBB[piece] |= (1ULL << sq);
    board[sq] = piece;
}

void init_tables();


template<Eval_Type type>
int eval(Board& board);

int eval(Board& board);


/* These values are taken from https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
   and originate from the program Rofchade: http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&start=19 */

int midgame_table_per_piece[6][64] =
        {
                {
                        82, 82, 82, 82, 82, 82, 82, 82,
                        47, 81, 62, 59, 67, 106, 120, 60,
                        56, 78, 78, 72, 85, 85, 115, 70,
                        55, 80, 77, 94, 99, 88, 92, 57,
                        68, 95, 88, 103, 105, 94, 99, 59,
                        76, 89, 108, 113, 147, 138, 107, 62,
                        180, 216, 143, 177, 150, 208, 116, 71,
                        82, 82, 82, 82, 82, 82, 82, 82,
                }, {
                        232, 316, 279, 304, 320, 309, 318, 314,
                        308, 284, 325, 334, 336, 355, 323, 318,
                        314, 328, 349, 347, 356, 354, 362, 321,
                        324, 341, 353, 350, 365, 356, 358, 329,
                        328, 354, 356, 390, 374, 406, 355, 359,
                        290, 397, 374, 402, 421, 466, 410, 381,
                        264, 296, 409, 373, 360, 399, 344, 320,
                        170, 248, 303, 288, 398, 240, 322, 230,
                }, {
                        332, 362, 351, 344, 352, 353, 326, 344,
                        369, 380, 381, 365, 372, 386, 398, 366,
                        365, 380, 380, 380, 379, 392, 383, 375,
                        359, 378, 378, 391, 399, 377, 375, 369,
                        361, 370, 384, 415, 402, 402, 372, 363,
                        349, 402, 408, 405, 400, 415, 402, 363,
                        339, 381, 347, 352, 395, 424, 383, 318,
                        336, 369, 283, 328, 340, 323, 372, 357,
                }, {
                        458, 464, 478, 494, 493, 484, 440, 451,
                        433, 461, 457, 468, 476, 488, 471, 406,
                        432, 452, 461, 460, 480, 477, 472, 444,
                        441, 451, 465, 476, 486, 470, 483, 454,
                        453, 466, 484, 503, 501, 512, 469, 457,
                        472, 496, 503, 513, 494, 522, 538, 493,
                        504, 509, 535, 539, 557, 544, 503, 521,
                        509, 519, 509, 528, 540, 486, 508, 520,
                }, {
                        1024, 1007, 1016, 1035, 1010, 1000, 994, 975,
                        990, 1017, 1036, 1027, 1033, 1040, 1022, 1026,
                        1011, 1027, 1014, 1023, 1020, 1027, 1039, 1030,
                        1016, 999, 1016, 1015, 1023, 1021, 1028, 1022,
                        998, 998, 1009, 1009, 1024, 1042, 1023, 1026,
                        1012, 1008, 1032, 1033, 1054, 1081, 1072, 1082,
                        1001, 986, 1020, 1026, 1009, 1082, 1053, 1079,
                        997, 1025, 1054, 1037, 1084, 1069, 1068, 1070,
                }, {
                        -15, 36, 12, -54, 8, -28, 24, 14,
                        1, 7, -8, -64, -43, -16, 9, 8,
                        -14, -14, -22, -46, -44, -30, -15, -27,
                        -49, -1, -27, -39, -46, -44, -33, -51,
                        -17, -20, -12, -27, -30, -25, -14, -36,
                        -9, 24, 2, -16, -20, 6, 22, -22,
                        29, -1, -20, -7, -8, -4, -38, -29,
                        -65, 23, 16, -15, -56, -34, 2, 13,
                }
        };

int endgame_table_per_piece[6][64] =
        {
                {
                        94, 94, 94, 94, 94, 94, 94, 94,
                        107, 102, 102, 104, 107, 94, 96, 87,
                        98, 101, 88, 95, 94, 89, 93, 86,
                        107, 103, 91, 87, 87, 86, 97, 93,
                        126, 118, 107, 99, 92, 98, 111, 111,
                        188, 194, 179, 161, 150, 147, 176, 178,
                        272, 267, 252, 228, 241, 226, 259, 281,
                        94, 94, 94, 94, 94, 94, 94, 94,
                }, {
                        252, 230, 258, 266, 259, 263, 231, 217,
                        239, 261, 271, 276, 279, 261, 258, 237,
                        258, 278, 280, 296, 291, 278, 261, 259,
                        263, 275, 297, 306, 297, 298, 285, 263,
                        264, 284, 303, 303, 303, 292, 289, 263,
                        257, 261, 291, 290, 280, 272, 262, 240,
                        256, 273, 256, 279, 272, 256, 257, 229,
                        223, 243, 268, 253, 250, 254, 218, 182,
                }, {
                        274, 288, 274, 292, 288, 281, 292, 280,
                        283, 279, 290, 296, 301, 288, 282, 270,
                        285, 294, 305, 307, 310, 300, 290, 282,
                        291, 300, 310, 316, 304, 307, 294, 288,
                        294, 306, 309, 306, 311, 307, 300, 299,
                        299, 289, 297, 296, 295, 303, 297, 301,
                        289, 293, 304, 285, 294, 284, 293, 283,
                        283, 276, 286, 289, 290, 288, 280, 273,
                }, {
                        503, 514, 515, 511, 507, 499, 516, 492,
                        506, 506, 512, 514, 503, 503, 501, 509,
                        508, 512, 507, 511, 505, 500, 504, 496,
                        515, 517, 520, 516, 507, 506, 504, 501,
                        516, 515, 525, 513, 514, 513, 511, 514,
                        519, 519, 519, 517, 516, 509, 507, 509,
                        523, 525, 525, 523, 509, 515, 520, 515,
                        525, 522, 530, 527, 524, 524, 520, 517,
                }, {
                        903, 908, 914, 893, 931, 904, 916, 895,
                        914, 913, 906, 920, 920, 913, 900, 904,
                        920, 909, 951, 942, 945, 953, 946, 941,
                        918, 964, 955, 983, 967, 970, 975, 959,
                        939, 958, 960, 981, 993, 976, 993, 972,
                        916, 942, 945, 985, 983, 971, 955, 945,
                        919, 956, 968, 977, 994, 961, 966, 936,
                        927, 958, 958, 963, 963, 955, 946, 956,
                }, {
                        -53, -34, -21, -11, -28, -14, -24, -43,
                        -27, -11, 4, 13, 14, 4, -5, -17,
                        -19, -3, 11, 21, 23, 16, 7, -9,
                        -18, -4, 21, 24, 27, 23, 9, -11,
                        -8, 22, 24, 27, 26, 33, 26, 3,
                        10, 17, 23, 15, 20, 45, 44, 13,
                        -12, 17, 14, 17, 17, 38, 23, 11,
                        -74, -35, -18, -18, -11, 15, 4, -17,
                }
        };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-designator"
int gamephase_influence[12] = {[WhitePawn] = 0, [BlackPawn] = 0, [WhiteKnight] = 1, [BlackKnight] = 1,
                               [WhiteBishop] = 1, [BlackBishop] = 1, [WhiteRook] = 2, [BlackRook] = 2,
                               [WhiteQueen] = 4, [BlackQueen] = 4, [WhiteKing] = 0, [BlackKing] = 0 };
#pragma clang diagnostic pop

int midgame_table[12][64];
int endgame_table[12][64];

void init_tables()
{
    for (int piece = PAWN; piece <= KING; piece++) {
        for (int square = 0; square < 64; square++) {
            midgame_table[piece]    [square] = midgame_table_per_piece[piece][square];
            endgame_table[piece]    [square] = endgame_table_per_piece[piece][square];
            midgame_table[piece + 6][square] = midgame_table_per_piece[piece][square ^ 56]; // ^ 56 gets the corresponding square from a mirrored board
            endgame_table[piece + 6][square] = endgame_table_per_piece[piece][square ^ 56]; // We use that for the Black sides values
        }
    }
}

template<>
int eval<Full_PST>(Board& board)
{
    int midgame[2] = { 0, 0 };
    int endgame[2] = { 0, 0 };
    int gamePhase = 0;

    for (Square square = SQ_A1; square <= SQ_H8; ++square) {
        Piece piece = board.board[square];
        if (piece != None) {
            midgame[board.colorOf(square)] += midgame_table[piece][square];
            endgame[board.colorOf(square)] += endgame_table[piece][square];
            gamePhase += gamephase_influence[piece];
        }
    }

    int midgame_score_difference = midgame[board.sideToMove] - midgame[!board.sideToMove];
    int endgame_score_difference = endgame[board.sideToMove] - endgame[!board.sideToMove];
    int total_weight = 24;
    int midgame_weight = std::min(gamePhase, total_weight);
    int endgame_weight = total_weight - midgame_weight;
    return (midgame_score_difference * midgame_weight + endgame_score_difference * endgame_weight) / total_weight;
}

template<>
int eval<Incremental_PST>(Board& board) {
    return 0;
}

template<>
int eval<Random>(Board& board) {
    return 0;
}

int eval(Board& board) {
    return eval<EVAL_TYPE>(board);
}
