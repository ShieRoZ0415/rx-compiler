#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "ir.h"
#include "semantic/analyzer.h"

namespace rx
{
    class IRGenerator {
    public:
        bool run(const Crate& crate, const SemanticAnalyzer& semantic, std::ostream& output);
    private:
        const SemanticAnalyzer* semantic_ = nullptr;
        std::ostream* output_ = nullptr;
        IRBuilder builder_;
        std::vector<std::unordered_map<std::string, IRPlace>> locals_;
        SemType returnType_;
        const StructItem* self_ = nullptr;
        bool failed_ = false;
        void error(const std::string& message);

        IRValue emitBinary(const std::string& op, const IRValue& left, const IRValue& right);
        struct LoopTarget {
            std::string continueLabel;
            std::string breakLabel;
            IRPlace result;
            bool hasResult = false;
        };
        std::vector<LoopTarget> loops_;
        void emitFunction(const FunctionItem& function, const StructItem* owner = nullptr);
        IRValue emitExpr(const Expr& expr, const SemType* expected = nullptr);
        IRPlace emitPlace(const Expr& expr);
        IRValue load(const IRPlace& place);
        void store(const IRPlace& place, const IRValue& value);
        IRValue emitBlock(const Block& block);
        void emitStmt(const Stmt& statement);
        std::unordered_map<std::string, const FunctionItem*> functions_;
        IRValue emitCall(const CallExpr& call);

        void emitRuntime();
    };
}
