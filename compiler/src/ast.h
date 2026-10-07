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

    enum class TypeKind {
        Unit,
        Path,
        Reference,
        Array
    };

    struct Type {
        TypeKind kind=TypeKind::Unit;
        std::string path;
        bool isMutable=false;
        std::string arrayLength;
        std::unique_ptr<Type> inner;
        std::unique_ptr<Type> element;
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
        StmtKind kind=StmtKind::Empty;
        std::string name;
        bool isMutable=false;
        std::string typeName;
        std::unique_ptr<Type> type;
        std::unique_ptr<Expr> expression;
        bool hasSemicolon=true;
    };

    struct Block {
        std::vector<std::unique_ptr<Stmt>> statements;
        std::unique_ptr<Expr> tail;
    };

    struct FunctionParameter {
        std::string name;
        std::string typeName;
        bool isMutable=false;
        bool isSelf=false;
        std::unique_ptr<Type> type;
    };

    struct Function {
        std::string name;
        std::vector<FunctionParameter> parameters;
        std::string resultType;
        std::unique_ptr<Type> result;
        std::unique_ptr<Block> body;
        bool hasSelfParam=false;
        std::string genericParams;
        std::string whereClause;
    };

    struct StructField {
        std::string name;
        std::string typeName;
        std::unique_ptr<Type> type;
    };

    struct Item {
        ItemKind kind=ItemKind::Use;
        std::string name;
        std::string text;
        std::string genericParams;
        std::string whereClause;
        std::vector<std::string> attributes;
        std::unique_ptr<Function> function;
        std::unique_ptr<Type> type;
        std::unique_ptr<Expr> value;
        std::vector<StructField> fields;
        std::vector<std::unique_ptr<Item>> associatedItems;
    };

    struct Crate {
        std::vector<std::unique_ptr<Item>> items;
    };
}
