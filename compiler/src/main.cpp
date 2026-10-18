#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include "antlr4-runtime.h"
#include "RxLexer.h"
#include "RxParser.h"
#include "ast_builder.h"

namespace
{
    class PrintVisitor : public rx::Visitor {
    public:
        explicit PrintVisitor(std::ostream& os) : out(os) {}

        void visitCrate(const rx::Crate& crate) override {
            out << "Crate\n";
            for (const auto& item : crate.items) item->accept(*this);
        }

        void visitBlock(const rx::Block& block) override {
            out << pad() << "Block\n";
            depth++;
            for (const auto& stmt : block.statements) stmt->accept(*this);
            if (block.tail) {
                out << pad() << "Tail\n";
                depth++;
                block.tail->accept(*this);
                out << "\n";
                depth--;
            }
            depth--;
        }

        void visitUnitType(const rx::UnitType&) override { out << "()"; }
        void visitPathType(const rx::PathType& type) override { printPath(type.path); }

        void visitReferenceType(const rx::ReferenceType& type) override {
            out << "&";
            if (!type.lifetime.empty()) out << type.lifetime << " ";
            if (type.isMutable) out << "mut ";
            type.inner->accept(*this);
        }

        void visitArrayType(const rx::ArrayType& type) override {
            out << "[";
            type.element->accept(*this);
            out << "; ";
            type.length->accept(*this);
            out << "]";
        }

        void visitIntegerLiteralExpr(const rx::IntegerLiteralExpr& expr) override { out << expr.text; }
        void visitBooleanLiteralExpr(const rx::BooleanLiteralExpr& expr) override { out << (expr.value ? "true" : "false"); }

        void visitPathExpr(const rx::PathExpr& expr) override { printPath(expr.path); }

        void visitUnaryExpr(const rx::UnaryExpr& expr) override {
            out << expr.op;
            if (expr.op == "&mut") out << " ";
            expr.operand->accept(*this);
        }

        void visitBinaryExpr(const rx::BinaryExpr& expr) override {
            out << "(";
            expr.lhs->accept(*this);
            out << " " << expr.op << " ";
            expr.rhs->accept(*this);
            out << ")";
        }

        void visitAssignmentExpr(const rx::AssignmentExpr& expr) override {
            out << "(";
            expr.lhs->accept(*this);
            out << " " << expr.op << " ";
            expr.rhs->accept(*this);
            out << ")";
        }

        void visitCastExpr(const rx::CastExpr& expr) override {
            expr.value->accept(*this);
            out << " as ";
            expr.type->accept(*this);
        }

        void visitCallExpr(const rx::CallExpr& expr) override {
            expr.callee->accept(*this);
            out << "(";
            for (size_t i = 0; i < expr.args.size(); ++i) {
                if (i) out << ", ";
                expr.args[i]->accept(*this);
            }
            out << ")";
        }

        void visitMethodCallExpr(const rx::MethodCallExpr& expr) override {
            expr.receiver->accept(*this);
            out << "." << expr.method << "(";
            for (size_t i = 0; i < expr.args.size(); ++i) {
                if (i) out << ", ";
                expr.args[i]->accept(*this);
            }
            out << ")";
        }

        void visitFieldAccessExpr(const rx::FieldAccessExpr& expr) override {
            expr.receiver->accept(*this);
            out << "." << expr.field;
        }

        void visitIndexExpr(const rx::IndexExpr& expr) override {
            expr.base->accept(*this);
            out << "[";
            expr.index->accept(*this);
            out << "]";
        }

        void visitArrayExpr(const rx::ArrayExpr& expr) override {
            out << "[";
            for (size_t i = 0; i < expr.elements.size(); ++i) {
                if (i) out << ", ";
                expr.elements[i]->accept(*this);
            }
            if (expr.repeat) {
                if (!expr.elements.empty()) out << "; ";
                expr.repeat->accept(*this);
            }
            out << "]";
        }

        void visitStructExpr(const rx::StructExpr& expr) override {
            printPath(expr.path);
            out << " { ";
            for (size_t i = 0; i < expr.fields.size(); ++i) {
                if (i) out << ", ";
                out << expr.fields[i] << ": ";
                expr.values[i]->accept(*this);
            }
            out << " }";
        }

        void visitBlockExpr(const rx::BlockExpr& expr) override { expr.block->accept(*this); }

        void visitIfExpr(const rx::IfExpr& expr) override {
            out << "if ";
            expr.condition->accept(*this);
            out << " ";
            expr.thenBranch->accept(*this);
            if (expr.elseBranch) {
                out << " else ";
                expr.elseBranch->accept(*this);
            }
        }

        void visitLoopExpr(const rx::LoopExpr& expr) override {
            out << "loop ";
            expr.body->accept(*this);
        }

        void visitWhileExpr(const rx::WhileExpr& expr) override {
            out << "while ";
            expr.condition->accept(*this);
            out << " ";
            expr.body->accept(*this);
        }

        void visitBreakExpr(const rx::BreakExpr& expr) override {
            out << "break";
            if (expr.value) {
                out << " ";
                expr.value->accept(*this);
            }
        }

        void visitReturnExpr(const rx::ReturnExpr& expr) override {
            out << "return";
            if (expr.value) {
                out << " ";
                expr.value->accept(*this);
            }
        }

        void visitContinueExpr(const rx::ContinueExpr&) override { out << "continue"; }
        void visitUnitExpr(const rx::UnitExpr&) override { out << "()"; }

        void visitLetStmt(const rx::LetStmt& stmt) override {
            out << pad() << "let " << (stmt.isMutable ? "mut " : "") << stmt.name;
            if (stmt.type) {
                out << ": ";
                stmt.type->accept(*this);
            }
            out << " = ";
            stmt.initializer->accept(*this);
            out << ";\n";
        }

        void visitExprStmt(const rx::ExprStmt& stmt) override {
            out << pad();
            stmt.expression->accept(*this);
            out << (stmt.hasSemicolon ? ";\n" : "\n");
        }

        void visitEmptyStmt(const rx::EmptyStmt&) override { out << pad() << ";\n"; }

        void visitUseItem(const rx::UseItem& item) override {
            out << pad() << "use ";
            printUseTree(item.useTree);
            out << ";\n";
        }

        void visitStructItem(const rx::StructItem& item) override {
            out << pad() << "struct " << item.name;
            printGenericParams(item.genericParams);
            printWhereClause(item.whereClause);
            if (!item.attributes.empty()) {
                out << " #[derive(";
                for (size_t i = 0; i < item.attributes.size(); ++i) {
                    if (i) out << ", ";
                    out << item.attributes[i];
                }
                out << ")]";
            }
            out << " {\n";
            depth++;
            for (const auto& field : item.fields) {
                out << pad() << field.name << ": ";
                field.type->accept(*this);
                out << ",\n";
            }
            depth--;
            out << pad() << "}\n";
        }

        void visitFunctionItem(const rx::FunctionItem& item) override {
            out << pad() << "fn " << item.name;
            printGenericParams(item.genericParams);
            out << "(";
            for (size_t i = 0; i < item.parameters.size(); ++i) {
                if (i) out << ", ";
                const auto& p = item.parameters[i];
                if (p.isSelf) {
                    if (p.isReference) out << "&";
                    if (!p.lifetime.empty()) out << p.lifetime << " ";
                    if (p.isMutable) out << "mut ";
                    out << "self";
                }
                else {
                    if (p.isMutable) out << "mut ";
                    out << p.name << ": ";
                    p.type->accept(*this);
                }
            }
            out << ")";
            if (item.resultType) {
                out << " -> ";
                item.resultType->accept(*this);
            }
            printWhereClause(item.whereClause);
            out << "\n";
            depth++;
            item.body->accept(*this);
            depth--;
        }

        void visitConstItem(const rx::ConstItem& item) override {
            out << pad() << "const " << item.name << ": ";
            item.type->accept(*this);
            out << " = ";
            item.value->accept(*this);
            out << ";\n";
        }

        void visitImplItem(const rx::ImplItem& item) override {
            out << pad() << "impl";
            printGenericParams(item.genericParams);
            out << " ";
            item.selfType->accept(*this);
            printWhereClause(item.whereClause);
            out << " {\n";
            depth++;
            for (const auto& child : item.items) child->accept(*this);
            depth--;
            out << pad() << "}\n";
        }

    private:
        std::ostream& out;
        int depth = 0;

        std::string pad() const { return std::string(depth * 2, ' '); }

        void printPath(const rx::Path& path) {
            for (size_t i = 0; i < path.segments.size(); ++i) {
                if (i) out << "::";
                out << path.segments[i].name;
                if (!path.segments[i].args.empty()) {
                    out << "<";
                    for (size_t j = 0; j < path.segments[i].args.size(); ++j) {
                        if (j) out << ", ";
                        const auto& arg = path.segments[i].args[j];
                        if (arg.isLifetime) out << arg.lifetime;
                        else if (arg.type) arg.type->accept(*this);
                    }
                    out << ">";
                }
            }
        }

        void printGenericParams(const std::vector<rx::LifetimeParam>& params) {
            if (params.empty()) return;
            out << "<";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i) out << ", ";
                out << params[i].name;
                if (!params[i].bounds.empty()) {
                    out << ": ";
                    for (size_t j = 0; j < params[i].bounds.size(); ++j) {
                        if (j) out << " + ";
                        out << params[i].bounds[j];
                    }
                }
            }
            out << ">";
        }

        void printWhereClause(const std::vector<rx::WherePredicate>& predicates) {
            if (predicates.empty()) return;
            out << " where ";
            for (size_t i = 0; i < predicates.size(); ++i) {
                if (i) out << ", ";
                const auto& p = predicates[i];
                if (p.isLifetime) {
                    out << p.lifetime << ": ";
                    for (size_t j = 0; j < p.bounds.size(); ++j) {
                        if (j) out << " + ";
                        out << p.bounds[j];
                    }
                }
                else {
                    p.type->accept(*this);
                    out << ": ";
                    for (size_t j = 0; j < p.bounds.size(); ++j) {
                        if (j) out << " + ";
                        out << p.bounds[j];
                    }
                }
            }
        }

        void printUseTree(const rx::UseTree& tree) {
            if (tree.isGlob) {
                if (!tree.path.empty()) out << tree.path << "::";
                out << "*";
                return;
            }
            if (tree.isGroup) {
                if (!tree.path.empty()) out << tree.path << "::";
                out << "{";
                for (size_t i = 0; i < tree.children.size(); ++i) {
                    if (i) out << ", ";
                    printUseTree(tree.children[i]);
                }
                out << "}";
                return;
            }
            out << tree.path;
            if (!tree.alias.empty()) out << " as " << tree.alias;
        }
    };
}

int main(int argc, char** argv) {
    std::string stage = "dump";
    std::string filename;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stage" && i + 1 < argc) {
            stage = argv[++i];
        }
        else if (!arg.empty() && arg[0] != '-') {
            filename = arg;
        }
    }

    std::string code;

    if (!filename.empty()) {
        std::ifstream in(filename);
        if (!in) {
            std::cerr << "Cannot open file: " << filename << "\n";
            return 1;
        }
        code.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    else {
        code = R"(
struct Pair<'a> {
    first: &'a i32,
    second: i32,
}
fn main() {
    let x = 1 + 2 * 3;
    let v: Vec<i32> = Vec::new();
    println_i32(x);
}
)";
    }

    antlr4::ANTLRInputStream input(code);
    rxgrammar::RxLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    rxgrammar::RxParser parser(&tokens);

    auto* tree = parser.crate();

    if (parser.getNumberOfSyntaxErrors() > 0) {
        std::cerr << "Syntax error.\n";
        return 1;
    }

    rx::ASTBuilder builder;
    auto ast = builder.buildCrate(tree);

    if (stage == "semantic") {
        // Semantic analysis starts here: walk the AST and reject invalid programs with exit code 1.
        return 0;
    }

    PrintVisitor printer(std::cout);
    ast->accept(printer);

    return 0;
}
