#pragma once

constexpr bool use_tt = true;
constexpr uint32_t entries_per_bucket = 4;
constexpr bool DEBUG_OUTPUTS = false;
constexpr Eval_Type MIN_EVAL = std::numeric_limits<int16_t>::min() + 1, MAX_EVAL = std::numeric_limits<int16_t>::max();
constexpr Eval_Type ON_EVALUATION = std::numeric_limits<int16_t>::min();
constexpr std::int32_t DEFER_DEPTH = 3;
constexpr bool PRINT_TO_FILE = true;