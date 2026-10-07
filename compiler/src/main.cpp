#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include "antlr4-runtime.h"
#include "RxLexer.h"
#include "RxParser.h"
#include "ast_builder.h"

void printExpr(const rx::Expr* expr, int indent) {
    if (!expr) return;

    std::cout << std::string(indent, ' ');

    if (expr->kind == rx::ExprKind::IntegerLiteral) {
        std::cout << "IntegerLiteral: " << expr->text << "\n";
    }
    else if (expr->kind == rx::ExprKind::BooleanLiteral) {
        std::cout << "BooleanLiteral: " << expr->text << "\n";
    }
    else if (expr->kind == rx::ExprKind::Variable) {
        std::cout << "Variable: " << expr->text << "\n";
    }
    else if (expr->kind == rx::ExprKind::Binary) {
        std::cout << "Binary: " << expr->text << "\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Unary) {
        std::cout << "Unary: " << expr->text << "\n";
        if (!expr->operands.empty()) {
            printExpr(expr->operands[0].get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Assignment) {
        std::cout << "Assignment: " << expr->text << "\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Cast) {
        std::cout << "Cast: " << expr->text << "\n";
        if (!expr->operands.empty()) {
            printExpr(expr->operands[0].get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Array) {
        std::cout << "Array: " << expr->text << "\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Call) {
        std::cout << "Call\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Index) {
        std::cout << "Index: " << expr->text << "\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Method) {
        std::cout << "Method: " << expr->text << "\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Field) {
        std::cout << "Field: " << expr->text << "\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Break) {
        std::cout << "Break\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Return) {
        std::cout << "Return\n";
        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Continue) {
        std::cout << "Continue\n";
    }
    else if (expr->kind == rx::ExprKind::Array) {
        std::cout << "Array: " << expr->text << "\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Struct) {
        std::cout << "Struct: " << expr->text << "\n";

        for (size_t i = 0; i < expr->operands.size(); ++i) {
            std::cout << std::string(indent + 2, ' ')
                << "Field: "
                << expr->fieldNames[i]
                << "\n";

            printExpr(expr->operands[i].get(), indent + 4);
        }
    }
    else if (expr->kind == rx::ExprKind::Block) {
        std::cout << "Block\n";

        if (expr->block) {
            for (const auto& stmt : expr->block->statements) {
                if (stmt->kind == rx::StmtKind::Let) {
                    std::cout << std::string(indent + 2, ' ')
                        << "Let: "
                        << stmt->name
                        << "\n";

                    printExpr(stmt->expression.get(), indent + 4);
                }
                else if (stmt->expression) {
                    std::cout << std::string(indent + 2, ' ')
                        << "Statement\n";

                    printExpr(stmt->expression.get(), indent + 4);
                }
            }

            if (expr->block->tail) {
                std::cout << std::string(indent + 2, ' ')
                    << "Tail\n";

                printExpr(expr->block->tail.get(), indent + 4);
            }
        }
    }
    else if (expr->kind == rx::ExprKind::If) {
        std::cout << "If\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Loop) {
        std::cout << "Loop\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::While) {
        std::cout << "While\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Call) {
        std::cout << "Call\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Method) {
        std::cout << "Method: " << expr->text << "\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Field) {
        std::cout << "Field: " << expr->text << "\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else if (expr->kind == rx::ExprKind::Index) {
        std::cout << "Index\n";

        for (const auto& operand : expr->operands) {
            printExpr(operand.get(), indent + 2);
        }
    }
    else {
        std::cout << "Unknown\n";
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
        std::cout << "Item: " << item->name << '\n';

        if (item->function) {
            std::cout << "  Function: "
                << item->function->name << '\n';

            if (item->function->body) {
                std::cout << "  Body statements: "
                    << item->function->body->statements.size()
                    << '\n';
                for (const auto& stmt : item->function->body->statements) {
                    if (stmt->kind == rx::StmtKind::Empty) {
                        std::cout << "    Statement: Empty\n";
                    }
                    else if (stmt->kind == rx::StmtKind::Let) {
                        std::cout << "    Statement: Let\n";
                        std::cout << "      Name: " << stmt->name << '\n';
                        std::cout << "      Mutable: "
                            << (stmt->isMutable ? "true" : "false")
                            << '\n';
                    }
                    if (!stmt->expression) {
                        std::cout << "    Expression: null\n";
                    }
                    else {
                        std::cout << "    Expression tree:\n";
                        printExpr(stmt->expression.get(), 6);
                    }
                }
            }
        }
    }

    return 0;
}
