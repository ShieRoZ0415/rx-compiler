#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"
#include "symbol.h"
#include "type.h"

namespace rx
{
    class SemanticAnalyzer : public Visitor {
    public:
        bool run(Crate& crate);
        bool ok() const { return errorCount_ == 0; }

    private:
        std::deque<Scope> scopes_;

        SemType currentType_;
        int errorCount_ = 0;

        SemType returnType_ = unitType();
        SemType selfType_ = neverType();

        struct LoopContext {
            bool allowsValue = true;
            bool hasBreak = false;
            bool hasExpected = false;
            bool blockedResult = false;
            SemType expected;
            SemType breakType = neverType();
        };
        std::vector<LoopContext> loops_;

        std::unordered_map<std::string, std::vector<const FunctionItem*>> methods_;

        std::unordered_map<const StructItem*, int> layoutState_;
        bool checkLayout(const SemType& type);

        std::unordered_map<std::string, Symbol> structTypes_;

        struct ConstantInfo {
            const ConstItem* definition = nullptr;
            const StructItem* owner = nullptr;
            SemType type;
            long long value = 0;
            int state = 0;
        };
        std::unordered_map<std::string, ConstantInfo> constants_;
        bool evaluatingConstant_ = false;

        bool readInteger(
            const std::string& text,
            long long& value,
            SemType& type,
            bool& hasSuffix
        );

        ConstantInfo* findConstant(const Path& path);

        bool evaluateConstant(ConstantInfo& constant);

        bool evaluateConstantExpr(
            const Expr& expr,
            const SemType& expected,
            SemType& actual,
            long long& value
        );

        struct ExprInfo {
            SemType type;
            bool isPlace = false;
            bool canWrite = true;
            bool blocked = false;
        };
        std::unordered_map<const Expr*, ExprInfo> exprInfo_;
        ExprInfo dereference(ExprInfo info);
        SemType checkExprType(const Expr& expr,const SemType* expected);
        SemType checkArguments(const std::vector<SemType>& parameters,const std::vector<std::unique_ptr<Expr>>& arguments,const SemType& result);
        SemType checkFunctionCall(const FunctionItem& function,const SemType& owner,const std::vector<std::unique_ptr<Expr>>& arguments,std::size_t firstParameter);
        bool supportsTrait(const SemType& type,const std::string& trait);
        bool supportsTrait(const SemType& type,const std::string& trait,std::unordered_set<const StructItem*>& visiting);

        Scope& currentScope() { return scopes_.back(); }
        void error(const std::string& message);
        SemType resolveType(const Type& type);
        SemType typeFromSegment(const PathSegment& segment);
        SemType resolveStruct(const Path& path);
        const FunctionItem* findMethod(const SemType& receiver, const std::string& name);
        SemType findField(const SemType& receiver, const std::string& name, bool* found);
        bool checkArgs(const FunctionItem& fn, const std::vector<SemType>& args);
        SemType checkExpr(const Expr& expr,const SemType* expected = nullptr);
        int readArrayLength(const Expr& expr);
        SemType checkBlockBody(const Block& block, const SemType* expected = nullptr);

        void visitCrate(const Crate&) override;
        void visitBlock(const Block&) override;

        void visitUnitType(const UnitType&) override;
        void visitPathType(const PathType&) override;
        void visitReferenceType(const ReferenceType&) override;
        void visitArrayType(const ArrayType&) override;

        void visitIntegerLiteralExpr(const IntegerLiteralExpr&) override;
        void visitBooleanLiteralExpr(const BooleanLiteralExpr&) override;
        void visitPathExpr(const PathExpr&) override;
        void visitUnaryExpr(const UnaryExpr&) override;
        void visitBinaryExpr(const BinaryExpr&) override;
        void visitAssignmentExpr(const AssignmentExpr&) override;
        void visitCastExpr(const CastExpr&) override;
        void visitCallExpr(const CallExpr&) override;
        void visitMethodCallExpr(const MethodCallExpr&) override;
        void visitFieldAccessExpr(const FieldAccessExpr&) override;
        void visitIndexExpr(const IndexExpr&) override;
        void visitArrayExpr(const ArrayExpr&) override;
        void visitStructExpr(const StructExpr&) override;
        void visitIfExpr(const IfExpr&) override;
        void visitLoopExpr(const LoopExpr&) override;
        void visitWhileExpr(const WhileExpr&) override;
        void visitBreakExpr(const BreakExpr&) override;
        void visitReturnExpr(const ReturnExpr&) override;
        void visitContinueExpr(const ContinueExpr&) override;
        void visitUnitExpr(const UnitExpr&) override;

        void visitLetStmt(const LetStmt&) override;
        void visitExprStmt(const ExprStmt&) override;
        void visitEmptyStmt(const EmptyStmt&) override;

        void visitUseItem(const UseItem&) override;
        void visitStructItem(const StructItem&) override;
        void visitFunctionItem(const FunctionItem&) override;
        void visitConstItem(const ConstItem&) override;
        void visitImplItem(const ImplItem&) override;
    };
}
