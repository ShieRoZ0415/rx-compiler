#pragma once

#include <memory>
#include <string>
#include <vector>

namespace rx
{
    class Visitor;

    struct Node {
        virtual ~Node() = default;
        virtual void accept(Visitor& visitor) const = 0;
    };

    struct Type : Node {};

    struct Expr : Node {};

    struct Stmt : Node {};

    struct Item : Node {};

    struct GenericArg {
        bool isLifetime = false;
        std::string lifetime;
        std::unique_ptr<Type> type;
    };

    struct PathSegment {
        std::string name;
        std::vector<GenericArg> args;
    };

    struct Path {
        std::vector<PathSegment> segments;
    };

    struct LifetimeParam {
        std::string name;
        std::vector<std::string> bounds;
    };

    struct WherePredicate {
        bool isLifetime = false;
        std::string lifetime;
        std::unique_ptr<Type> type;
        std::vector<std::string> bounds;
    };

    struct StructField {
        std::string name;
        std::unique_ptr<Type> type;
    };

    struct FunctionParam {
        std::string name;
        std::string lifetime;
        bool isSelf = false;
        bool isReference = false;
        bool isMutable = false;
        std::unique_ptr<Type> type;
    };

    struct UseTree {
        std::string path;
        bool isGlob = false;
        bool isGroup = false;
        std::string alias;
        std::vector<UseTree> children;
    };

    struct Block : Node {
        std::vector<std::unique_ptr<Stmt>> statements;
        std::unique_ptr<Expr> tail;

        void accept(Visitor& visitor) const override;
    };

    struct UnitType : Type {
        void accept(Visitor& visitor) const override;
    };

    struct PathType : Type {
        Path path;

        void accept(Visitor& visitor) const override;
    };

    struct ReferenceType : Type {
        std::string lifetime;
        bool isMutable = false;
        std::unique_ptr<Type> inner;

        void accept(Visitor& visitor) const override;
    };

    struct ArrayType : Type {
        std::unique_ptr<Type> element;
        std::unique_ptr<Expr> length;

        void accept(Visitor& visitor) const override;
    };

    struct IntegerLiteralExpr : Expr {
        std::string text;

        void accept(Visitor& visitor) const override;
    };

    struct BooleanLiteralExpr : Expr {
        bool value = false;

        void accept(Visitor& visitor) const override;
    };

    struct PathExpr : Expr {
        Path path;

        void accept(Visitor& visitor) const override;
    };

    struct UnaryExpr : Expr {
        std::string op;
        std::unique_ptr<Expr> operand;

        void accept(Visitor& visitor) const override;
    };

    struct BinaryExpr : Expr {
        std::string op;
        std::unique_ptr<Expr> lhs;
        std::unique_ptr<Expr> rhs;

        void accept(Visitor& visitor) const override;
    };

    struct AssignmentExpr : Expr {
        std::string op;
        std::unique_ptr<Expr> lhs;
        std::unique_ptr<Expr> rhs;

        void accept(Visitor& visitor) const override;
    };

    struct CastExpr : Expr {
        std::unique_ptr<Expr> value;
        std::unique_ptr<Type> type;

        void accept(Visitor& visitor) const override;
    };

    struct CallExpr : Expr {
        std::unique_ptr<Expr> callee;
        std::vector<std::unique_ptr<Expr>> args;

        void accept(Visitor& visitor) const override;
    };

    struct MethodCallExpr : Expr {
        std::unique_ptr<Expr> receiver;
        std::string method;
        std::vector<std::unique_ptr<Expr>> args;

        void accept(Visitor& visitor) const override;
    };

    struct FieldAccessExpr : Expr {
        std::unique_ptr<Expr> receiver;
        std::string field;

        void accept(Visitor& visitor) const override;
    };

    struct IndexExpr : Expr {
        std::unique_ptr<Expr> base;
        std::unique_ptr<Expr> index;

        void accept(Visitor& visitor) const override;
    };

    struct ArrayExpr : Expr {
        std::vector<std::unique_ptr<Expr>> elements;
        std::unique_ptr<Expr> repeat;

        void accept(Visitor& visitor) const override;
    };

    struct StructExpr : Expr {
        Path path;
        std::vector<std::string> fields;
        std::vector<std::unique_ptr<Expr>> values;

        void accept(Visitor& visitor) const override;
    };

    struct BlockExpr : Expr {
        std::unique_ptr<Block> block;

        void accept(Visitor& visitor) const override;
    };

    struct IfExpr : Expr {
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Expr> thenBranch;
        std::unique_ptr<Expr> elseBranch;

        void accept(Visitor& visitor) const override;
    };

    struct LoopExpr : Expr {
        std::unique_ptr<Expr> body;

        void accept(Visitor& visitor) const override;
    };

    struct WhileExpr : Expr {
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Expr> body;

        void accept(Visitor& visitor) const override;
    };

    struct BreakExpr : Expr {
        std::unique_ptr<Expr> value;

        void accept(Visitor& visitor) const override;
    };

    struct ReturnExpr : Expr {
        std::unique_ptr<Expr> value;

        void accept(Visitor& visitor) const override;
    };

    struct ContinueExpr : Expr {
        void accept(Visitor& visitor) const override;
    };

    struct UnitExpr : Expr {
        void accept(Visitor& visitor) const override;
    };

    struct LetStmt : Stmt {
        std::string name;
        bool isMutable = false;
        std::unique_ptr<Type> type;
        std::unique_ptr<Expr> initializer;

        void accept(Visitor& visitor) const override;
    };

    struct ExprStmt : Stmt {
        std::unique_ptr<Expr> expression;
        bool hasSemicolon = true;

        void accept(Visitor& visitor) const override;
    };

    struct EmptyStmt : Stmt {
        void accept(Visitor& visitor) const override;
    };

    struct UseItem : Item {
        UseTree useTree;

        void accept(Visitor& visitor) const override;
    };

    struct StructItem : Item {
        std::string name;
        std::vector<LifetimeParam> genericParams;
        std::vector<WherePredicate> whereClause;
        std::vector<std::string> attributes;
        std::vector<StructField> fields;

        void accept(Visitor& visitor) const override;
    };

    struct FunctionItem : Item {
        std::string name;
        std::vector<LifetimeParam> genericParams;
        std::vector<WherePredicate> whereClause;
        std::vector<FunctionParam> parameters;
        std::unique_ptr<Type> resultType;
        std::unique_ptr<Block> body;

        void accept(Visitor& visitor) const override;
    };

    struct ConstItem : Item {
        std::string name;
        std::unique_ptr<Type> type;
        std::unique_ptr<Expr> value;

        void accept(Visitor& visitor) const override;
    };

    struct ImplItem : Item {
        std::unique_ptr<Type> selfType;
        std::vector<LifetimeParam> genericParams;
        std::vector<WherePredicate> whereClause;
        std::vector<std::unique_ptr<Item>> items;

        void accept(Visitor& visitor) const override;
    };

    struct Crate {
        std::vector<std::unique_ptr<Item>> items;

        void accept(Visitor& visitor) const;
    };

    class Visitor {
    public:
        virtual ~Visitor() = default;

        virtual void visitCrate(const Crate&) = 0;
        virtual void visitBlock(const Block&) = 0;

        virtual void visitUnitType(const UnitType&) = 0;
        virtual void visitPathType(const PathType&) = 0;
        virtual void visitReferenceType(const ReferenceType&) = 0;
        virtual void visitArrayType(const ArrayType&) = 0;

        virtual void visitIntegerLiteralExpr(const IntegerLiteralExpr&) = 0;
        virtual void visitBooleanLiteralExpr(const BooleanLiteralExpr&) = 0;
        virtual void visitPathExpr(const PathExpr&) = 0;
        virtual void visitUnaryExpr(const UnaryExpr&) = 0;
        virtual void visitBinaryExpr(const BinaryExpr&) = 0;
        virtual void visitAssignmentExpr(const AssignmentExpr&) = 0;
        virtual void visitCastExpr(const CastExpr&) = 0;
        virtual void visitCallExpr(const CallExpr&) = 0;
        virtual void visitMethodCallExpr(const MethodCallExpr&) = 0;
        virtual void visitFieldAccessExpr(const FieldAccessExpr&) = 0;
        virtual void visitIndexExpr(const IndexExpr&) = 0;
        virtual void visitArrayExpr(const ArrayExpr&) = 0;
        virtual void visitStructExpr(const StructExpr&) = 0;
        virtual void visitBlockExpr(const BlockExpr&) = 0;
        virtual void visitIfExpr(const IfExpr&) = 0;
        virtual void visitLoopExpr(const LoopExpr&) = 0;
        virtual void visitWhileExpr(const WhileExpr&) = 0;
        virtual void visitBreakExpr(const BreakExpr&) = 0;
        virtual void visitReturnExpr(const ReturnExpr&) = 0;
        virtual void visitContinueExpr(const ContinueExpr&) = 0;
        virtual void visitUnitExpr(const UnitExpr&) = 0;

        virtual void visitLetStmt(const LetStmt&) = 0;
        virtual void visitExprStmt(const ExprStmt&) = 0;
        virtual void visitEmptyStmt(const EmptyStmt&) = 0;

        virtual void visitUseItem(const UseItem&) = 0;
        virtual void visitStructItem(const StructItem&) = 0;
        virtual void visitFunctionItem(const FunctionItem&) = 0;
        virtual void visitConstItem(const ConstItem&) = 0;
        virtual void visitImplItem(const ImplItem&) = 0;
    };

    inline void Block::accept(Visitor& visitor) const { visitor.visitBlock(*this); }
    inline void UnitType::accept(Visitor& visitor) const { visitor.visitUnitType(*this); }
    inline void PathType::accept(Visitor& visitor) const { visitor.visitPathType(*this); }
    inline void ReferenceType::accept(Visitor& visitor) const { visitor.visitReferenceType(*this); }
    inline void ArrayType::accept(Visitor& visitor) const { visitor.visitArrayType(*this); }
    inline void IntegerLiteralExpr::accept(Visitor& visitor) const { visitor.visitIntegerLiteralExpr(*this); }
    inline void BooleanLiteralExpr::accept(Visitor& visitor) const { visitor.visitBooleanLiteralExpr(*this); }
    inline void PathExpr::accept(Visitor& visitor) const { visitor.visitPathExpr(*this); }
    inline void UnaryExpr::accept(Visitor& visitor) const { visitor.visitUnaryExpr(*this); }
    inline void BinaryExpr::accept(Visitor& visitor) const { visitor.visitBinaryExpr(*this); }
    inline void AssignmentExpr::accept(Visitor& visitor) const { visitor.visitAssignmentExpr(*this); }
    inline void CastExpr::accept(Visitor& visitor) const { visitor.visitCastExpr(*this); }
    inline void CallExpr::accept(Visitor& visitor) const { visitor.visitCallExpr(*this); }
    inline void MethodCallExpr::accept(Visitor& visitor) const { visitor.visitMethodCallExpr(*this); }
    inline void FieldAccessExpr::accept(Visitor& visitor) const { visitor.visitFieldAccessExpr(*this); }
    inline void IndexExpr::accept(Visitor& visitor) const { visitor.visitIndexExpr(*this); }
    inline void ArrayExpr::accept(Visitor& visitor) const { visitor.visitArrayExpr(*this); }
    inline void StructExpr::accept(Visitor& visitor) const { visitor.visitStructExpr(*this); }
    inline void BlockExpr::accept(Visitor& visitor) const { visitor.visitBlockExpr(*this); }
    inline void IfExpr::accept(Visitor& visitor) const { visitor.visitIfExpr(*this); }
    inline void LoopExpr::accept(Visitor& visitor) const { visitor.visitLoopExpr(*this); }
    inline void WhileExpr::accept(Visitor& visitor) const { visitor.visitWhileExpr(*this); }
    inline void BreakExpr::accept(Visitor& visitor) const { visitor.visitBreakExpr(*this); }
    inline void ReturnExpr::accept(Visitor& visitor) const { visitor.visitReturnExpr(*this); }
    inline void ContinueExpr::accept(Visitor& visitor) const { visitor.visitContinueExpr(*this); }
    inline void UnitExpr::accept(Visitor& visitor) const { visitor.visitUnitExpr(*this); }
    inline void LetStmt::accept(Visitor& visitor) const { visitor.visitLetStmt(*this); }
    inline void ExprStmt::accept(Visitor& visitor) const { visitor.visitExprStmt(*this); }
    inline void EmptyStmt::accept(Visitor& visitor) const { visitor.visitEmptyStmt(*this); }
    inline void UseItem::accept(Visitor& visitor) const { visitor.visitUseItem(*this); }
    inline void StructItem::accept(Visitor& visitor) const { visitor.visitStructItem(*this); }
    inline void FunctionItem::accept(Visitor& visitor) const { visitor.visitFunctionItem(*this); }
    inline void ConstItem::accept(Visitor& visitor) const { visitor.visitConstItem(*this); }
    inline void ImplItem::accept(Visitor& visitor) const { visitor.visitImplItem(*this); }
    inline void Crate::accept(Visitor& visitor) const { visitor.visitCrate(*this); }
}
