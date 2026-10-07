#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include "antlr4-runtime.h"
#include "RxLexer.h"
#include "RxParser.h"
#include "ast_builder.h"

void printType(const rx::Type* type, int indent) {
    if (!type) {
        std::cout << std::string(indent, ' ') << "Type: <null>\n";
        return;
    }
    std::string pad(indent, ' ');
    switch (type->kind) {
    case rx::TypeKind::Unit:
        std::cout << pad << "Type: () [Unit]\n";
        break;
    case rx::TypeKind::Path:
        std::cout << pad << "Type: " << type->path << " [Path]\n";
        break;
    case rx::TypeKind::Reference:
        std::cout << pad << "Type: &" << (type->isMutable ? "mut " : "")
                  << "[Reference]\n";
        printType(type->inner.get(), indent + 2);
        break;
    case rx::TypeKind::Array:
        std::cout << pad << "Type: [" << type->arrayLength << "] [Array]\n";
        printType(type->element.get(), indent + 2);
        break;
    }
}

void printExpr(const rx::Expr* expr, int indent);

void printBlock(const rx::Block* block, int indent) {
    if (!block) return;
    for (const auto& stmt : block->statements) {
        if (stmt->kind == rx::StmtKind::Let) {
            std::cout << std::string(indent, ' ') << "Let: " << stmt->name
                      << (stmt->isMutable ? " (mut)" : "")
                      << " : " << stmt->typeName << "\n";
            printType(stmt->type.get(), indent + 2);
            printExpr(stmt->expression.get(), indent + 2);
        } else if (stmt->expression) {
            std::cout << std::string(indent, ' ') << "Statement\n";
            printExpr(stmt->expression.get(), indent + 2);
        }
    }
    if (block->tail) {
        std::cout << std::string(indent, ' ') << "Tail\n";
        printExpr(block->tail.get(), indent + 2);
    }
}

void printExpr(const rx::Expr* expr, int indent) {
    if (!expr) return;

    std::cout << std::string(indent, ' ');

    switch (expr->kind) {
    case rx::ExprKind::IntegerLiteral:
        std::cout << "IntegerLiteral: " << expr->text << "\n";
        break;
    case rx::ExprKind::BooleanLiteral:
        std::cout << "BooleanLiteral: " << expr->text << "\n";
        break;
    case rx::ExprKind::Variable:
        std::cout << "Variable: " << expr->text << "\n";
        break;
    case rx::ExprKind::Binary:
        std::cout << "Binary: " << expr->text << "\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Unary:
        std::cout << "Unary: " << expr->text << "\n";
        if (!expr->operands.empty()) printExpr(expr->operands[0].get(), indent + 2);
        break;
    case rx::ExprKind::Assignment:
        std::cout << "Assignment: " << expr->text << "\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Cast:
        std::cout << "Cast: " << expr->text << "\n";
        if (!expr->operands.empty()) printExpr(expr->operands[0].get(), indent + 2);
        break;
    case rx::ExprKind::Array:
        std::cout << "Array: " << expr->text << "\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Call:
        std::cout << "Call\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Index:
        std::cout << "Index: " << expr->text << "\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Method:
        std::cout << "Method: " << expr->text << "\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Field:
        std::cout << "Field: " << expr->text << "\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Break:
        std::cout << "Break\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Return:
        std::cout << "Return\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Continue:
        std::cout << "Continue\n";
        break;
    case rx::ExprKind::Struct:
        std::cout << "Struct: " << expr->text << "\n";
        for (size_t i = 0; i < expr->operands.size(); ++i) {
            std::cout << std::string(indent + 2, ' ') << "Field: "
                      << (i < expr->fieldNames.size() ? expr->fieldNames[i] : "?")
                      << "\n";
            printExpr(expr->operands[i].get(), indent + 4);
        }
        break;
    case rx::ExprKind::Block:
        std::cout << "Block\n";
        printBlock(expr->block.get(), indent + 2);
        break;
    case rx::ExprKind::If:
        std::cout << "If\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::Loop:
        std::cout << "Loop\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    case rx::ExprKind::While:
        std::cout << "While\n";
        for (const auto& operand : expr->operands) printExpr(operand.get(), indent + 2);
        break;
    default:
        std::cout << "Unknown\n";
        break;
    }
}

void printItem(const rx::Item* item, int indent) {
    if (!item) return;
    std::string pad(indent, ' ');

    switch (item->kind) {
    case rx::ItemKind::Use:
        std::cout << pad << "Use: " << item->name << "\n";
        break;

    case rx::ItemKind::Struct:
        std::cout << pad << "Struct: " << item->name;
        if (!item->genericParams.empty()) std::cout << " <" << item->genericParams << ">";
        std::cout << "\n";
        if (!item->whereClause.empty()) std::cout << pad << "  where " << item->whereClause << "\n";
        for (const auto& a : item->attributes)
            std::cout << pad << "  attribute: " << a << "\n";
        for (const auto& f : item->fields) {
            std::cout << pad << "  field: " << f.name << " : " << f.typeName << "\n";
            printType(f.type.get(), indent + 4);
        }
        break;

    case rx::ItemKind::Function: {
        const auto& fn = item->function;
        if (!fn) break;
        std::cout << pad << "Function: " << fn->name;
        if (!fn->genericParams.empty()) std::cout << " <" << fn->genericParams << ">";
        std::cout << "\n";
        if (!fn->whereClause.empty()) std::cout << pad << "  where " << fn->whereClause << "\n";
        for (const auto& p : fn->parameters) {
            std::cout << pad << "  param: " << p.name
                      << (p.isSelf ? " (self)" : "")
                      << " : " << p.typeName
                      << (p.isMutable ? " [mut]" : "") << "\n";
            printType(p.type.get(), indent + 4);
        }
        std::cout << pad << "  returns: " << fn->resultType << "\n";
        printType(fn->result.get(), indent + 4);
        std::cout << pad << "  body:\n";
        printBlock(fn->body.get(), indent + 2);
        break;
    }

    case rx::ItemKind::Constant:
        std::cout << pad << "Const: " << item->name << "\n";
        printType(item->type.get(), indent + 2);
        std::cout << pad << "  value:\n";
        printExpr(item->value.get(), indent + 4);
        break;

    case rx::ItemKind::Impl:
        std::cout << pad << "Impl: " << item->name << "\n";
        printType(item->type.get(), indent + 2);
        if (!item->genericParams.empty()) std::cout << pad << "  generic: " << item->genericParams << "\n";
        if (!item->whereClause.empty()) std::cout << pad << "  where " << item->whereClause << "\n";
        for (const auto& assoc : item->associatedItems)
            printItem(assoc.get(), indent + 2);
        break;
    }
}

int main(int argc, char** argv) {
    std::string code;

    if (argc >= 2) {
        std::ifstream in(argv[1]);

        if (!in) {
            std::cerr << "Cannot open file: " << argv[1] << "\n";
            return 1;
        }

        code.assign(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()
        );
    } else {
        code = R"(
fn main() {
    let x = 1 + 2 * 3;
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

    std::cout << "AST created successfully.\n";
    std::cout << "Top-level items: "
        << ast->items.size() << '\n';

    for (const auto& item : ast->items) {
        printItem(item.get(), 2);
    }

    return 0;
}
