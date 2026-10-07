cmake_minimum_required(VERSION 3.15)

project(rx_compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories(
    src
    generated/grammar
    /usr/local/include/antlr4-runtime
)


file(GLOB GENERATED_SRC
    generated/grammar/*.cpp
)


add_executable(
    compiler

    src/main.cpp

    ${GENERATED_SRC}
        src/ast.h
        src/ast_builder.h
        src/ast_builder.cpp
)


target_link_libraries(
    compiler
    antlr4-runtime
)
