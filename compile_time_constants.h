#pragma once

constexpr bool use_tt = true;
constexpr uint32_t entries_per_bucket = 4;
constexpr bool DEBUG_OUTPUTS = false;
constexpr Eval_Type MIN_EVAL = -30000, MAX_EVAL = 30000, MAX_MATE_DEPTH = 255;
constexpr Eval_Type REPETITION_SCORE = 0, STALEMATE_SCORE = 0;
constexpr Eval_Type ON_EVALUATION = std::numeric_limits<int16_t>::min();
constexpr std::int32_t DEFER_DEPTH = 3;

constexpr int DEFAULT_DEPTH = 6;
constexpr uint64_t STARTING_SEED = 0;
