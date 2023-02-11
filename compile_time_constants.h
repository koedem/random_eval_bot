#pragma once

constexpr bool use_tt = true;
constexpr uint32_t entries_per_bucket = 4;
constexpr bool DEBUG_OUTPUTS = false;
constexpr Eval_Type MIN_EVAL = INT16_MIN + 1, MAX_EVAL = INT16_MAX;
constexpr Eval_Type ON_EVALUATION = INT16_MIN;