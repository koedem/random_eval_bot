cmake_minimum_required(VERSION 3.24)
project(exercise5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g -flto -march=native")
#  -fno-inline-functions -fsanitize=integer -fsanitize=address -fsanitize=thread
add_executable(exercise5 main.cpp perft_tt.h perft.h sequential_search.h chess.hpp transposition_table.h compile_time_constants.h lockless_tt.h locking_tt.h simple_concurrent_search.h abdada_search.h abdada_tt.h simplified_abdada.h)

#set_property(TARGET exercise5 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)