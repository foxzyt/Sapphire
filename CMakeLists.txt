cmake_minimum_required(VERSION 3.10)
project(Sapphire)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sapphire
    src/ast.cpp
    src/evaluator.cpp
    src/lexer.cpp
    src/main.cpp
    src/parser.cpp
)