cmake_minimum_required(VERSION 3.24)
project(exercise5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g -flto -march=native")

add_executable(exercise5 main.cpp perft_tt.h perft.h sequential_search.h chess.hpp)
