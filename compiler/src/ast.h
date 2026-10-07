#pragma once

#include <memory>
#include <string>
#include <vector>

namespace rx
{
    struct Block;

    enum class ItemKind {
        Use,
        Struct,
        Function,
        Constant,
        Impl
    };

    enum class ExprKind {
        Unknown,
        IntegerLiteral,
        BooleanLiteral,
        Variable,
        Binary,
        Unary,
        Assignment,
        Cast,
        Break,
        Return,
        Continue,
        Call,
        Index,
        Method,
        Field,
        Array,
        Struct,
        Block,
        If,
        Loop,
        While, Unit
    };

    struct Expr {
        ExprKind kind = ExprKind::Unknown;
        std::string text;
        std::vector<std::unique_ptr<Expr>> operands;
        std::vector<std::string> fieldNames;
        std::unique_ptr<Block> block;
    };

    enum class StmtKind {
        Empty,
        Let,
        Expr
    };

    struct Stmt {
        StmtKind kind = StmtKind::Empty;

        std::string name;
        bool isMutable = false;
        std::string typeName;

        std::unique_ptr<Expr> expression;

        bool hasSemicolon = true;
    };

    struct Block {
        std::vector<std::unique_ptr<Stmt>> statements;
        std::unique_ptr<Expr> tail;
    };

    struct FunctionParameter {
        std::string name;
        std::string typeName;
        bool isMutable = false;
    };

    struct Function {
        std::string name;
        std::vector<FunctionParameter> parameters;
        std::string resultType;
        std::unique_ptr<Block> body;
    };

    struct Item {
        ItemKind kind;
        std::string name;

        std::unique_ptr<Function> function;
    };

    struct Crate {
        std::vector<std::unique_ptr<Item>> items;
    };
}
