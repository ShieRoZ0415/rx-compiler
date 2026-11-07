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

        src/semantic/type.h
        src/semantic/type.cpp
        src/semantic/symbol.h
        src/semantic/symbol.cpp
        src/semantic/analyzer.h
        src/semantic/analyzer.cpp

        src/ir/ir.h
        src/ir/ir.cpp
        src/ir/generator.h
        src/ir/generator.cpp
)


target_link_libraries(
    compiler
    antlr4-runtime
)
