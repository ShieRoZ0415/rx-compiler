#include "ast_builder.h"

#include "RxParser.h"
#include "RxParser.h"

namespace rx
{
    std::unique_ptr<Crate> ASTBuilder::buildCrate(rxgrammar::RxParser::CrateContext* ctx) {
        auto crate = std::make_unique<Crate>();

        for (auto* itemCtx : ctx->item()) {
            crate->items.push_back(buildItem(itemCtx));
        }

        return crate;
    }

    std::unique_ptr<Item> ASTBuilder::buildItem(rxgrammar::RxParser::ItemContext* ctx) {
        auto item = std::make_unique<Item>();

        if (ctx->useDeclaration()) {
            item->kind = ItemKind::Use;
        }
        else if (ctx->functionDefinition()) {
            item->kind = ItemKind::Function;

            auto* functionCtx = ctx->functionDefinition();

            item->name = functionCtx
                         ->identifier()
                         ->getText();

            item->function = std::make_unique<Function>();
            item->function->name = item->name;

            item->function->body = buildBlock(functionCtx->blockExpression());
        }
        else if (ctx->structDefinition()) {
            item->kind = ItemKind::Struct;

            item->name =
                ctx->structDefinition()
                   ->identifier()
                   ->getText();
        }
        else if (ctx->constantItem()) {
            item->kind = ItemKind::Constant;

            item->name =
                ctx->constantItem()
                   ->identifier()
                   ->getText();
        }
        else if (ctx->inherentImpl()) {
            item->kind = ItemKind::Impl;
        }

        return item;
    }

    std::unique_ptr<Block> ASTBuilder::buildBlock(rxgrammar::RxParser::BlockExpressionContext* ctx) {
        auto block = std::make_unique<Block>();
        for (auto* statementCtx : ctx->statement()) {
            block->statements.push_back(buildStatement(statementCtx));
        }
        if (ctx->statementExpression()) {
            block->tail = buildStatementExpression(ctx->statementExpression());
        }
        return block;
    }

    std::unique_ptr<Stmt> ASTBuilder::buildStatement(rxgrammar::RxParser::StatementContext* ctx) {
        auto stmt = std::make_unique<Stmt>();
        if (ctx->letStatement()) {
            stmt->kind = StmtKind::Let;

            auto* letCtx = ctx->letStatement();
            stmt->name = letCtx->identifierBinding()->identifier()->getText();
            stmt->isMutable = letCtx->identifierBinding()->MUT() != nullptr;
            stmt->expression = buildExpression(letCtx->expression());
            return stmt;
        }
        if (ctx->expressionWithBlock()) {
            stmt->kind = StmtKind::Expr;
            stmt->expression = buildExpressionWithBlock(ctx->expressionWithBlock());
            return stmt;
        }
        if (ctx->statementExpression()) {
            stmt->kind = StmtKind::Expr;
            stmt->expression = buildStatementExpression(ctx->statementExpression());
            return stmt;
        }
        if (ctx->SEMI()) {
            stmt->kind = StmtKind::Empty;
            return stmt;
        }

        return stmt;
    }

    std::unique_ptr<Stmt> ASTBuilder::buildLetStatement(rxgrammar::RxParser::LetStatementContext* ctx) {
        auto stmt = std::make_unique<Stmt>();

        stmt->kind = StmtKind::Let;

        auto* binding = ctx->identifierBinding();

        stmt->name =
            binding->identifier()
                   ->getText();

        stmt->isMutable =
            binding->MUT() != nullptr;

        stmt->expression = buildExpression(ctx->expression());

        if (ctx->typeRef()) {
            stmt->typeName =
                ctx->typeRef()->getText();
        }

        return stmt;
    }

    std::unique_ptr<Expr> ASTBuilder::buildExpression(rxgrammar::RxParser::ExpressionContext* ctx) {
        return buildAssignmentExpression(ctx->assignmentExpression());
    }

    std::unique_ptr<Expr> ASTBuilder::buildAdditiveExpression(rxgrammar::RxParser::AdditiveExpressionContext* ctx) {
        auto terms = ctx->multiplicativeExpression();
        auto operators = ctx->additiveOperator();

        auto lhs = buildMultiplicativeExpression(terms[0]);

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildMultiplicativeExpression(terms[i + 1]);

            auto binary = std::make_unique<Expr>();

            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();

            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));

            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildMultiplicativeExpression(
        rxgrammar::RxParser::MultiplicativeExpressionContext* ctx) {
        auto terms = ctx->castExpression();
        auto operators = ctx->multiplicativeOperator();

        auto lhs = buildCastExpression(terms[0]);

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildCastExpression(terms[i + 1]);

            auto binary = std::make_unique<Expr>();

            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();

            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));

            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildCastExpression(rxgrammar::RxParser::CastExpressionContext* ctx) {
        auto lhs = buildUnaryExpression(ctx->unaryExpression());
        if (!lhs) return nullptr;

        auto types = ctx->typeRef();
        for (auto* typeCtx : types) {
            auto cast = std::make_unique<Expr>();
            cast->kind = ExprKind::Cast;
            cast->text = typeCtx->getText();
            cast->operands.push_back(std::move(lhs));
            lhs = std::move(cast);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLiteralExpression(rxgrammar::RxParser::LiteralExpressionContext* ctx) {
        auto expr = std::make_unique<Expr>();

        if (ctx->INTEGER_LITERAL()) {
            expr->kind = ExprKind::IntegerLiteral;
            expr->text = ctx->INTEGER_LITERAL()->getText();
            return expr;
        }

        if (ctx->TRUE()) {
            expr->kind = ExprKind::BooleanLiteral;
            expr->text = "true";
            return expr;
        }

        if (ctx->FALSE()) {
            expr->kind = ExprKind::BooleanLiteral;
            expr->text = "false";
            return expr;
        }

        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildNonBlockPrimary(rxgrammar::RxParser::NonBlockPrimaryContext* ctx) {
        if (ctx->literalExpression()) return buildLiteralExpression(ctx->literalExpression());

        if (ctx->pathInExpression()) {
            auto path = buildPathInExpression(ctx->pathInExpression());
            if (!ctx->LBRACE()) return path;
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Struct;
            expr->text = path->text;
            if (ctx->structExprFields()) {
                for (auto* field : ctx->structExprFields()->structExprField()) {
                    expr->fieldNames.push_back(field->identifier()->getText());
                    auto value = buildExpression(field->expression());
                    if (!value) return nullptr;
                    expr->operands.push_back(std::move(value));
                }
            }

            return expr;
        }

        if (ctx->arrayExpression()) {
            auto expr = std::make_unique<Expr>();
            auto* array = ctx->arrayExpression();

            expr->kind = ExprKind::Array;
            expr->text = array->getText();

            for (auto* element : array->expression()) {
                auto value = buildExpression(element);
                if (!value) return nullptr;
                expr->operands.push_back(std::move(value));
            }

            return expr;
        }

        if (ctx->LPAREN()) {
            if (ctx->expression()) {
                return buildExpression(ctx->expression());
            }

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Unit;
            expr->text = "()";

            return expr;
        }

        if (ctx->BREAK()) {
            auto expr = std::make_unique<Expr>();

            expr->kind = ExprKind::Break;
            expr->text = "break";

            if (ctx->expression()) {
                auto value = buildExpression(ctx->expression());

                if (!value) return nullptr;

                expr->operands.push_back(std::move(value));
            }

            return expr;
        }

        if (ctx->RETURN()) {
            auto expr = std::make_unique<Expr>();

            expr->kind = ExprKind::Return;
            expr->text = "return";

            if (ctx->expression()) {
                auto value = buildExpression(ctx->expression());

                if (!value) return nullptr;

                expr->operands.push_back(std::move(value));
            }

            return expr;
        }

        if (ctx->CONTINUE()) {
            auto expr = std::make_unique<Expr>();

            expr->kind = ExprKind::Continue;
            expr->text = "continue";

            return expr;
        }
        return nullptr;
    }


    std::unique_ptr<Expr> ASTBuilder::buildPathInExpression(rxgrammar::RxParser::PathInExpressionContext* ctx) {
        auto expr = std::make_unique<Expr>();

        expr->kind = ExprKind::Variable;
        expr->text = ctx->getText();

        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildPrimaryExpression(rxgrammar::RxParser::PrimaryExpressionContext* ctx) {
        if (ctx->nonBlockPrimary()) {
            return buildNonBlockPrimary(ctx->nonBlockPrimary());
        }

        if (ctx->expressionWithBlock()) {
            return buildExpressionWithBlock(ctx->expressionWithBlock());
        }

        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildPostfixExpression(rxgrammar::RxParser::PostfixExpressionContext* ctx) {
        auto base = buildPrimaryExpression(ctx->primaryExpression());
        if (!base) return nullptr;
        for (auto* suffix : ctx->postfixSuffix()) {
            if (suffix->callArguments()) {
                auto call = std::make_unique<Expr>();
                call->kind = ExprKind::Call;
                call->text = "call";
                call->operands.push_back(std::move(base));
                for (auto* arg : suffix->callArguments()->expression()) {
                    auto argument = buildExpression(arg);
                    if (!argument) return nullptr;
                    call->operands.push_back(std::move(argument));
                }
                base = std::move(call);
                continue;
            }
            if (suffix->LBRACKET()) {
                auto index = buildExpression(suffix->expression());
                if (!index) return nullptr;
                auto expr = std::make_unique<Expr>();
                expr->kind = ExprKind::Index;
                expr->text = "[]";
                expr->operands.push_back(std::move(base));
                expr->operands.push_back(std::move(index));

                base = std::move(expr);

                continue;
            }
            if (suffix->dotSuffix()) {
                auto* dot = suffix->dotSuffix();
                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<Expr>();
                    method->kind = ExprKind::Method;
                    method->text = dot->pathExprSegment()->getText();
                    method->operands.push_back(std::move(base));
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->operands.push_back(std::move(argument));
                    }
                    base = std::move(method);
                    continue;
                }
                if (dot->identifier()) {
                    auto field = std::make_unique<Expr>();
                    field->kind = ExprKind::Field;
                    field->text = dot->identifier()->getText();
                    field->operands.push_back(std::move(base));
                    base = std::move(field);
                    continue;
                }
                return nullptr;
            }
            return nullptr;
        }

        return base;
    }

    std::unique_ptr<Expr> ASTBuilder::buildUnaryExpression(rxgrammar::RxParser::UnaryExpressionContext* ctx) {
        if (ctx->postfixExpression()) {
            return buildPostfixExpression(ctx->postfixExpression());
        }
        if (ctx->unaryOperator()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Unary;
            expr->text = ctx->unaryOperator()->getText();
            auto operand = buildUnaryExpression(ctx->unaryExpression());
            if (!operand) return nullptr;
            expr->operands.push_back(std::move(operand));
            return expr;
        }

        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildAssignmentExpression(rxgrammar::RxParser::AssignmentExpressionContext* ctx) {
        auto lhs = buildLogicalOrExpression(ctx->logicalOrExpression());
        if (!lhs) return nullptr;
        auto* assignmentOperator = ctx->assignmentOperator();
        if (!assignmentOperator) return lhs;

        auto rhs = buildExpression(ctx->expression());
        if (!rhs) return nullptr;

        auto binary = std::make_unique<Expr>();
        binary->kind = ExprKind::Assignment;
        binary->text = ctx->assignmentOperator()->getText();
        binary->operands.push_back(std::move(lhs));
        binary->operands.push_back(std::move(rhs));
        return binary;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLogicalOrExpression(rxgrammar::RxParser::LogicalOrExpressionContext* ctx) {
        auto lhs = buildLogicalAndExpression(ctx->logicalAndExpression(0));
        if (!lhs) return nullptr;
        auto terms = ctx->logicalAndExpression();
        auto operators = ctx->OROR();


        for (size_t i = 0; i < operators.size(); i++) {
            auto rhs = buildLogicalAndExpression(terms[i + 1]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLogicalAndExpression(rxgrammar::RxParser::LogicalAndExpressionContext* ctx) {
        auto lhs = buildComparisonExpression(ctx->comparisonExpression(0));
        if (!lhs) return nullptr;
        auto terms = ctx->comparisonExpression();
        auto operators = ctx->ANDAND();

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildComparisonExpression(terms[i + 1]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildComparisonExpression(rxgrammar::RxParser::ComparisonExpressionContext* ctx) {
        if (ctx->LT()) {
            auto lhs = buildClosedBitOrExpression(ctx->closedBitOrExpression());
            if (!lhs) return nullptr;

            auto rhs = buildBitOrExpression(ctx->bitOrExpression(0));
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = "<";
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));

            return binary;
        }

        auto lhs = buildBitOrExpression(ctx->bitOrExpression(0));
        if (!lhs) return nullptr;

        if (ctx->comparisonExceptLt()) {
            auto rhs = buildBitOrExpression(ctx->bitOrExpression(1));
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = ctx->comparisonExceptLt()->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            return binary;
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildBitOrExpression(rxgrammar::RxParser::BitOrExpressionContext* ctx) {
        auto terms = ctx->bitXorExpression();
        auto operators = ctx->PIPE();

        auto lhs = buildBitXorExpression(terms[0]);
        if (!lhs) return nullptr;
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildBitXorExpression(terms[i + 1]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildBitXorExpression(rxgrammar::RxParser::BitXorExpressionContext* ctx) {
        auto terms = ctx->bitAndExpression();
        auto operators = ctx->CARET();

        auto lhs = buildBitAndExpression(terms[0]);
        if (!lhs) return nullptr;
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildBitAndExpression(terms[i + 1]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildBitAndExpression(rxgrammar::RxParser::BitAndExpressionContext* ctx) {
        auto terms = ctx->shiftExpression();
        auto operators = ctx->AMP();

        auto lhs = buildShiftExpression(terms[0]);
        if (!lhs) return nullptr;
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildShiftExpression(terms[i + 1]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildShiftExpression(rxgrammar::RxParser::ShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* additive = dynamic_cast<rxgrammar::RxParser::ClosedAdditiveExpressionContext*>(child)) {
                auto value = buildClosedAdditiveExpression(additive);
                if (!value) return nullptr;
                if (!lhs) {
                    lhs = std::move(value);
                    continue;
                }
                if (pendingOperator.empty()) return nullptr;
                auto binary = std::make_unique<Expr>();
                binary->kind = ExprKind::Binary;
                binary->text = pendingOperator;
                binary->operands.push_back(std::move(lhs));
                binary->operands.push_back(std::move(value));
                lhs = std::move(binary);
                pendingOperator.clear();
                continue;
            }
            if (auto* additive = dynamic_cast<rxgrammar::RxParser::AdditiveExpressionContext*>(child)) {
                auto value = buildAdditiveExpression(additive);
                if (!value) return nullptr;
                if (!lhs) {
                    lhs = std::move(value);
                    continue;
                }
                if (pendingOperator.empty()) return nullptr;
                auto binary = std::make_unique<Expr>();
                binary->kind = ExprKind::Binary;
                binary->text = pendingOperator;
                binary->operands.push_back(std::move(lhs));
                binary->operands.push_back(std::move(value));
                lhs = std::move(binary);
                pendingOperator.clear();
                continue;
            }
            if (auto* shiftRight = dynamic_cast<rxgrammar::RxParser::ShiftRightContext*>(child)) {
                pendingOperator = shiftRight->getText();
                continue;
            }
            if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
                if (terminal->getText() == "<<") pendingOperator = "<<";
            }
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementExpression(rxgrammar::RxParser::StatementExpressionContext* ctx) {
        return buildStatementAssignmentExpression(ctx->statementAssignmentExpression());
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementAssignmentExpression(
        rxgrammar::RxParser::StatementAssignmentExpressionContext* ctx) {
        auto lhs = buildStatementLogicalOrExpression(ctx->statementLogicalOrExpression());
        if (!lhs) return nullptr;
        auto* assignmentOperator = ctx->assignmentOperator();
        if (!assignmentOperator) return lhs;
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::Assignment;
        expr->text = assignmentOperator->getText();
        expr->operands.push_back(std::move(lhs));
        auto rhs = buildExpression(ctx->expression());

        if (!rhs) return nullptr;
        expr->operands.push_back(std::move(rhs));

        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementLogicalOrExpression(
        rxgrammar::RxParser::StatementLogicalOrExpressionContext* ctx) {
        auto lhs = buildStatementLogicalAndExpression(ctx->statementLogicalAndExpression());
        if (!lhs) return nullptr;
        auto terms = ctx->logicalAndExpression();
        auto operators = ctx->OROR();
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildLogicalAndExpression(terms[i]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementLogicalAndExpression(
        rxgrammar::RxParser::StatementLogicalAndExpressionContext* ctx) {
        auto lhs = buildStatementComparisonExpression(ctx->statementComparisonExpression());
        if (!lhs) return nullptr;
        auto terms = ctx->comparisonExpression();
        auto operators = ctx->ANDAND();
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildComparisonExpression(terms[i]);

            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildClosedCastExpression(rxgrammar::RxParser::ClosedCastExpressionContext* ctx) {
        if (ctx->unaryExpression()) return buildUnaryExpression(ctx->unaryExpression());

        auto lhs = buildCastExpression(ctx->castExpression());
        if (!lhs) return nullptr;

        auto cast = std::make_unique<Expr>();
        cast->kind = ExprKind::Cast;
        cast->text = ctx->closedCastType()->getText();
        cast->operands.push_back(std::move(lhs));

        return cast;
    }

    std::unique_ptr<Expr> ASTBuilder::buildClosedMultiplicativeExpression(
        rxgrammar::RxParser::ClosedMultiplicativeExpressionContext* ctx) {
        auto terms = ctx->castExpression();
        auto operators = ctx->multiplicativeOperator();
        if (operators.empty()) return buildClosedCastExpression(ctx->closedCastExpression());
        auto lhs = buildCastExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) rhs = buildCastExpression(terms[i + 1]);
            else rhs = buildClosedCastExpression(ctx->closedCastExpression());
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildClosedAdditiveExpression(
        rxgrammar::RxParser::ClosedAdditiveExpressionContext* ctx) {
        auto terms = ctx->multiplicativeExpression();
        auto operators = ctx->additiveOperator();
        if (operators.empty()) return buildClosedMultiplicativeExpression(ctx->closedMultiplicativeExpression());

        auto lhs = buildMultiplicativeExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) rhs = buildMultiplicativeExpression(terms[i + 1]);
            else rhs = buildClosedMultiplicativeExpression(ctx->closedMultiplicativeExpression());
            if (!rhs) return nullptr;
            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr>
    ASTBuilder::buildClosedShiftExpression(rxgrammar::RxParser::ClosedShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* closedAdditive = dynamic_cast<rxgrammar::RxParser::ClosedAdditiveExpressionContext*>(child)) {
                auto value = buildClosedAdditiveExpression(closedAdditive);
                if (!value) return nullptr;

                if (!lhs) lhs = std::move(value);
                else {
                    if (pendingOperator.empty()) return nullptr;

                    auto binary = std::make_unique<Expr>();
                    binary->kind = ExprKind::Binary;
                    binary->text = pendingOperator;
                    binary->operands.push_back(std::move(lhs));
                    binary->operands.push_back(std::move(value));

                    lhs = std::move(binary);

                    pendingOperator.clear();
                }

                continue;
            }

            if (auto* additive = dynamic_cast<rxgrammar::RxParser::AdditiveExpressionContext*>(child)) {
                auto value = buildAdditiveExpression(additive);
                if (!value)return nullptr;

                if (!lhs) lhs = std::move(value);
                else {
                    if (pendingOperator.empty()) return nullptr;
                    auto binary = std::make_unique<Expr>();
                    binary->kind = ExprKind::Binary;
                    binary->text = pendingOperator;
                    binary->operands.push_back(std::move(lhs));
                    binary->operands.push_back(std::move(value));
                    lhs = std::move(binary);
                    pendingOperator.clear();
                }
                continue;
            }

            if (auto* shiftRight = dynamic_cast<rxgrammar::RxParser::ShiftRightContext*>(child)) {
                pendingOperator = shiftRight->getText();
                continue;
            }
            if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
                if (terminal->getText() == "<<") pendingOperator = "<<";
            }
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildClosedBitAndExpression(
        rxgrammar::RxParser::ClosedBitAndExpressionContext* ctx) {
        auto terms = ctx->shiftExpression();
        auto operators = ctx->AMP();
        if (operators.empty()) return buildClosedShiftExpression(ctx->closedShiftExpression());

        auto lhs = buildShiftExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); i++) {
            std::unique_ptr<Expr> rhs;

            if (i + 1 < terms.size()) rhs = buildShiftExpression(terms[i + 1]);
            else {
                rhs = buildClosedShiftExpression(ctx->closedShiftExpression());
            }
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildClosedBitXorExpression(
        rxgrammar::RxParser::ClosedBitXorExpressionContext* ctx) {
        auto terms = ctx->bitAndExpression();
        auto operators = ctx->CARET();
        if (operators.empty()) return buildClosedBitAndExpression(ctx->closedBitAndExpression());

        auto lhs = buildBitAndExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;

            if (i + 1 < terms.size()) {
                rhs = buildBitAndExpression(terms[i + 1]);
            }
            else {
                rhs = buildClosedBitAndExpression(ctx->closedBitAndExpression());
            }
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr>
    ASTBuilder::buildClosedBitOrExpression(rxgrammar::RxParser::ClosedBitOrExpressionContext* ctx) {
        auto terms = ctx->bitXorExpression();
        auto operators = ctx->PIPE();
        if (operators.empty()) return buildClosedBitXorExpression(ctx->closedBitXorExpression());

        auto lhs = buildBitXorExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;

            if (i + 1 < terms.size()) {
                rhs = buildBitXorExpression(terms[i + 1]);
            }
            else {
                rhs = buildClosedBitXorExpression(ctx->closedBitXorExpression());
            }
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementComparisonExpression(
        rxgrammar::RxParser::StatementComparisonExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        if (ctx->statementBitOrExpression()) lhs = buildStatementBitOrExpression(ctx->statementBitOrExpression());
        if (!lhs) return nullptr;

        if (ctx->comparisonExceptLt()) {
            auto* rhsCtx = ctx->bitOrExpression();
            if (!rhsCtx) return nullptr;

            auto rhs = buildBitOrExpression(rhsCtx);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = ctx->comparisonExceptLt()->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            return binary;
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementBitOrExpression(
        rxgrammar::RxParser::StatementBitOrExpressionContext* ctx) {
        auto lhs = buildStatementBitXorExpression(ctx->statementBitXorExpression());
        if (!lhs) return nullptr;

        auto terms = ctx->bitXorExpression();
        auto operators = ctx->PIPE();

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildBitXorExpression(terms[i]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementBitXorExpression(
        rxgrammar::RxParser::StatementBitXorExpressionContext* ctx) {
        auto lhs = buildStatementBitAndExpression(ctx->statementBitAndExpression());
        if (!lhs) return nullptr;

        auto terms = ctx->bitAndExpression();
        auto operators = ctx->CARET();

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildBitAndExpression(terms[i]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementBitAndExpression(
        rxgrammar::RxParser::StatementBitAndExpressionContext* ctx) {
        auto lhs = buildStatementShiftExpression(ctx->statementShiftExpression());
        if (!lhs) return nullptr;

        auto terms = ctx->shiftExpression();
        auto operators = ctx->AMP();

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildShiftExpression(terms[i]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementShiftExpression(
        rxgrammar::RxParser::StatementShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (size_t i = 0; i < ctx->children.size(); ++i) {
            auto* child = ctx->children[i];

            if (auto* statementAdditive = dynamic_cast<rxgrammar::RxParser::StatementAdditiveExpressionContext*>(
                child)) {
                auto value = buildStatementAdditiveExpression(statementAdditive);
                if (!value) return nullptr;

                if (!lhs) {
                    lhs = std::move(value);
                    continue;
                }

                if (pendingOperator.empty()) return nullptr;

                auto binary = std::make_unique<Expr>();
                binary->kind = ExprKind::Binary;
                binary->text = pendingOperator;
                binary->operands.push_back(std::move(lhs));
                binary->operands.push_back(std::move(value));
                lhs = std::move(binary);
                pendingOperator.clear();
                continue;
            }

            if (auto* additive = dynamic_cast<rxgrammar::RxParser::AdditiveExpressionContext*>(child)) {
                auto value = buildAdditiveExpression(additive);
                if (!value) return nullptr;

                if (!lhs) {
                    lhs = std::move(value);
                    continue;
                }

                if (pendingOperator.empty()) return nullptr;

                auto binary = std::make_unique<Expr>();
                binary->kind = ExprKind::Binary;
                binary->text = pendingOperator;
                binary->operands.push_back(std::move(lhs));
                binary->operands.push_back(std::move(value));
                lhs = std::move(binary);
                pendingOperator.clear();
                continue;
            }

            if (auto* shiftRight = dynamic_cast<rxgrammar::RxParser::ShiftRightContext*>(child)) {
                pendingOperator = shiftRight->getText();
                continue;
            }

            if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
                if (terminal->getText() == "<<") {
                    pendingOperator = "<<";
                }
            }
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementAdditiveExpression(
        rxgrammar::RxParser::StatementAdditiveExpressionContext* ctx) {
        auto lhs = buildStatementMultiplicativeExpression(ctx->statementMultiplicativeExpression());
        if (!lhs) return nullptr;

        auto terms = ctx->multiplicativeExpression();
        auto operators = ctx->additiveOperator();

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildMultiplicativeExpression(terms[i]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementMultiplicativeExpression(
        rxgrammar::RxParser::StatementMultiplicativeExpressionContext* ctx) {
        auto lhs = buildStatementCastExpression(ctx->statementCastExpression());
        if (!lhs) return nullptr;

        auto terms = ctx->castExpression();
        auto operators = ctx->multiplicativeOperator();

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildCastExpression(terms[i]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementCastExpression(
        rxgrammar::RxParser::StatementCastExpressionContext* ctx) {
        if (ctx->statementUnaryExpression()) return buildStatementUnaryExpression(ctx->statementUnaryExpression());
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementUnaryExpression(
        rxgrammar::RxParser::StatementUnaryExpressionContext* ctx) {
        if (ctx->unaryOperator()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Unary;
            expr->text = ctx->unaryOperator()->getText();

            auto operand = buildUnaryExpression(ctx->unaryExpression());
            if (!operand) return nullptr;

            expr->operands.push_back(std::move(operand));
            return expr;
        }
        if (ctx->statementPostfixExpression())
            return
                buildStatementPostfixExpression(ctx->statementPostfixExpression());

        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementPostfixExpression(
        rxgrammar::RxParser::StatementPostfixExpressionContext* ctx) {
        std::unique_ptr<Expr> base;

        if (ctx->nonBlockPrimary()) {
            base = buildNonBlockPrimary(ctx->nonBlockPrimary());
        } else {
            base = buildExpressionWithBlock(ctx->expressionWithBlock());   // {p} 这类
            if (!base) return nullptr;

            if (ctx->dotSuffix()) {
                auto* dot = ctx->dotSuffix();

                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<Expr>();
                    method->kind = ExprKind::Method;
                    method->text = dot->pathExprSegment()->getText();
                    method->operands.push_back(std::move(base));
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->operands.push_back(std::move(argument));
                    }
                    base = std::move(method);
                }
                else if (dot->identifier()) {
                    auto field = std::make_unique<Expr>();
                    field->kind = ExprKind::Field;
                    field->text = dot->identifier()->getText();
                    field->operands.push_back(std::move(base));
                    base = std::move(field);
                } else return nullptr;
            }
        }
        if (!base) return nullptr;

        for (auto* suffix : ctx->postfixSuffix()) {
            if (suffix->callArguments()) {
                auto call = std::make_unique<Expr>();
                call->kind = ExprKind::Call;
                call->text = "call";
                call->operands.push_back(std::move(base));
                for (auto* arg : suffix->callArguments()->expression()) {
                    auto argument = buildExpression(arg);
                    if (!argument) return nullptr;
                    call->operands.push_back(std::move(argument));
                }

                base = std::move(call);

                continue;
            }

            if (suffix->LBRACKET()) {
                auto index = buildExpression(suffix->expression());
                if (!index) return nullptr;

                auto expr = std::make_unique<Expr>();
                expr->kind = ExprKind::Index;
                expr->text = "[]";
                expr->operands.push_back(std::move(base));
                expr->operands.push_back(std::move(index));
                base = std::move(expr);

                continue;
            }
            if (suffix->dotSuffix()) {
                auto* dot = suffix->dotSuffix();

                if (dot->pathExprSegment() &&
                    dot->callArguments()) {
                    auto method = std::make_unique<Expr>();
                    method->kind = ExprKind::Method;
                    method->text = dot->pathExprSegment()->getText();

                    method->operands.push_back(std::move(base));
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->operands.push_back(std::move(argument));
                    }

                    base = std::move(method);
                    continue;
                }
                if (dot->identifier()) {
                    auto field = std::make_unique<Expr>();
                    field->kind = ExprKind::Field;
                    field->text = dot->identifier()->getText();
                    field->operands.push_back(std::move(base));
                    base = std::move(field);

                    continue;
                }

                return nullptr;
            }

            return nullptr;
        }

        return base;
    }

    std::unique_ptr<Expr> ASTBuilder::buildBlockExpressionAsExpr(rxgrammar::RxParser::BlockExpressionContext* ctx) {
        auto expr = std::make_unique<Expr>();

        expr->kind = ExprKind::Block;
        expr->text = "block";
        expr->block = buildBlock(ctx);

        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildExpressionWithBlock(rxgrammar::RxParser::ExpressionWithBlockContext* ctx) {
        if (ctx->ifExpression()) return buildIfExpression(ctx->ifExpression());

        if (ctx->LOOP()) return buildLoopExpression(ctx->blockExpression());

        if (ctx->WHILE()) return buildWhileExpression(ctx->conditionExpression(), ctx->blockExpression());

        if (ctx->blockExpression()) {
            return buildBlockExpressionAsExpr(ctx->blockExpression());
        }

        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLoopExpression(rxgrammar::RxParser::BlockExpressionContext* ctx) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::Loop;
        expr->text = "loop";
        expr->operands.push_back(buildBlockExpressionAsExpr(ctx));
        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildWhileExpression(rxgrammar::RxParser::ConditionExpressionContext* condition,
                                                           rxgrammar::RxParser::BlockExpressionContext* body) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::While;
        expr->text = "while";

        auto cond = buildConditionExpression(condition);
        if (!cond) return nullptr;

        expr->operands.push_back(std::move(cond));
        expr->operands.push_back(buildBlockExpressionAsExpr(body));

        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildIfExpression(rxgrammar::RxParser::IfExpressionContext* ctx) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::If;
        expr->text = "if";

        auto condition = buildConditionExpression(ctx->conditionExpression());
        if (!condition) return nullptr;
        expr->operands.push_back(std::move(condition));

        auto blocks = ctx->blockExpression();
        if (blocks.empty()) return nullptr;
        expr->operands.push_back(buildBlockExpressionAsExpr(blocks[0]));

        if (ctx->ifExpression()) {
            auto elseIf = buildIfExpression(ctx->ifExpression());
            if (!elseIf) return nullptr;
            expr->operands.push_back(std::move(elseIf));
        }
        else if (blocks.size() >= 2) {
            expr->operands.push_back(buildBlockExpressionAsExpr(blocks[1]));
        }

        return expr;
    }


    std::unique_ptr<Expr> ASTBuilder::buildConditionExpression(rxgrammar::RxParser::ConditionExpressionContext* ctx) {
        return buildConditionAssignmentExpression(ctx->conditionAssignmentExpression());
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionAssignmentExpression(
        rxgrammar::RxParser::ConditionAssignmentExpressionContext* ctx) {
        auto lhs = buildConditionLogicalOrExpression(ctx->conditionLogicalOrExpression());
        if (!lhs) return nullptr;

        auto* op = ctx->assignmentOperator();
        if (!op) return lhs;

        auto rhs = buildConditionExpression(ctx->conditionExpression());
        if (!rhs) return nullptr;

        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::Assignment;
        expr->text = op->getText();
        expr->operands.push_back(std::move(lhs));
        expr->operands.push_back(std::move(rhs));

        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionLogicalOrExpression(
        rxgrammar::RxParser::ConditionLogicalOrExpressionContext* ctx) {
        auto terms = ctx->conditionLogicalAndExpression();
        auto operators = ctx->OROR();

        auto lhs = buildConditionLogicalAndExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionLogicalAndExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));

            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionLogicalAndExpression(
        rxgrammar::RxParser::ConditionLogicalAndExpressionContext* ctx) {
        auto terms = ctx->conditionComparisonExpression();
        auto operators = ctx->ANDAND();

        auto lhs = buildConditionComparisonExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionComparisonExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto binary = std::make_unique<Expr>();
            binary->kind = ExprKind::Binary;
            binary->text = operators[i]->getText();
            binary->operands.push_back(std::move(lhs));
            binary->operands.push_back(std::move(rhs));
            lhs = std::move(binary);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionComparisonExpression(
        rxgrammar::RxParser::ConditionComparisonExpressionContext* ctx) {
        if (ctx->LT()) {
            auto lhs = buildConditionClosedBitOrExpression(ctx->conditionClosedBitOrExpression());
            if (!lhs) return nullptr;

            auto rhs = buildConditionBitOrExpression(ctx->conditionBitOrExpression(0));
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = "<";
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            return expr;
        }

        auto lhs = buildConditionBitOrExpression(ctx->conditionBitOrExpression(0));
        if (!lhs) return nullptr;

        if (ctx->comparisonExceptLt()) {
            auto rhs = buildConditionBitOrExpression(ctx->conditionBitOrExpression(1));
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = ctx->comparisonExceptLt()->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            return expr;
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionBitOrExpression(
        rxgrammar::RxParser::ConditionBitOrExpressionContext* ctx) {
        auto terms = ctx->conditionBitXorExpression();
        auto operators = ctx->PIPE();

        auto lhs = buildConditionBitXorExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionBitXorExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedBitOrExpression(
        rxgrammar::RxParser::ConditionClosedBitOrExpressionContext* ctx) {
        auto terms = ctx->conditionBitXorExpression();
        auto operators = ctx->PIPE();

        auto lhs = terms.empty()
                       ? buildConditionClosedBitXorExpression(ctx->conditionClosedBitXorExpression())
                       : buildConditionBitXorExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) {
                rhs = buildConditionBitXorExpression(terms[i + 1]);
            }
            else {
                rhs = buildConditionClosedBitXorExpression(ctx->conditionClosedBitXorExpression());
            }
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionBitXorExpression(
        rxgrammar::RxParser::ConditionBitXorExpressionContext* ctx) {
        auto terms = ctx->conditionBitAndExpression();
        auto operators = ctx->CARET();

        auto lhs = buildConditionBitAndExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionBitAndExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedBitXorExpression(
        rxgrammar::RxParser::ConditionClosedBitXorExpressionContext* ctx) {
        auto terms = ctx->conditionBitAndExpression();
        auto operators = ctx->CARET();

        auto lhs = terms.empty()
                       ? buildConditionClosedBitAndExpression(ctx->conditionClosedBitAndExpression())
                       : buildConditionBitAndExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) {
                rhs = buildConditionBitAndExpression(terms[i + 1]);
            }
            else {
                rhs = buildConditionClosedBitAndExpression(ctx->conditionClosedBitAndExpression());
            }
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionBitAndExpression(
        rxgrammar::RxParser::ConditionBitAndExpressionContext* ctx) {
        auto terms = ctx->conditionShiftExpression();
        auto operators = ctx->AMP();

        auto lhs = buildConditionShiftExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionShiftExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedBitAndExpression(
        rxgrammar::RxParser::ConditionClosedBitAndExpressionContext* ctx) {
        auto terms = ctx->conditionShiftExpression();
        auto operators = ctx->AMP();

        auto lhs = terms.empty()
                       ? buildConditionClosedShiftExpression(ctx->conditionClosedShiftExpression())
                       : buildConditionShiftExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) {
                rhs = buildConditionShiftExpression(terms[i + 1]);
            }
            else {
                rhs = buildConditionClosedShiftExpression(ctx->conditionClosedShiftExpression());
            }
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionShiftExpression(
        rxgrammar::RxParser::ConditionShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* closed = dynamic_cast<rxgrammar::RxParser::ConditionClosedAdditiveExpressionContext*>(child)) {
                auto value = buildConditionClosedAdditiveExpression(closed);
                if (!value) return nullptr;

                if (!lhs) {
                    lhs = std::move(value);
                }
                else {
                    if (pendingOperator.empty()) return nullptr;
                    auto expr = std::make_unique<Expr>();
                    expr->kind = ExprKind::Binary;
                    expr->text = pendingOperator;
                    expr->operands.push_back(std::move(lhs));
                    expr->operands.push_back(std::move(value));
                    lhs = std::move(expr);
                    pendingOperator.clear();
                }

                continue;
            }

            if (auto* additive = dynamic_cast<rxgrammar::RxParser::ConditionAdditiveExpressionContext*>(child)) {
                auto value = buildConditionAdditiveExpression(additive);
                if (!value) return nullptr;

                if (!lhs) {
                    lhs = std::move(value);
                }
                else {
                    if (pendingOperator.empty()) return nullptr;
                    auto expr = std::make_unique<Expr>();
                    expr->kind = ExprKind::Binary;
                    expr->text = pendingOperator;
                    expr->operands.push_back(std::move(lhs));
                    expr->operands.push_back(std::move(value));
                    lhs = std::move(expr);
                    pendingOperator.clear();
                }

                continue;
            }

            if (auto* shiftRight = dynamic_cast<rxgrammar::RxParser::ShiftRightContext*>(child)) {
                pendingOperator = shiftRight->getText();
                continue;
            }

            if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
                if (terminal->getText() == "<<") {
                    pendingOperator = "<<";
                }
            }
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedShiftExpression(
        rxgrammar::RxParser::ConditionClosedShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* closed = dynamic_cast<rxgrammar::RxParser::ConditionClosedAdditiveExpressionContext*>(child)) {
                auto value = buildConditionClosedAdditiveExpression(closed);
                if (!value) return nullptr;

                if (!lhs) {
                    lhs = std::move(value);
                }
                else {
                    if (pendingOperator.empty()) return nullptr;

                    auto expr = std::make_unique<Expr>();
                    expr->kind = ExprKind::Binary;
                    expr->text = pendingOperator;
                    expr->operands.push_back(std::move(lhs));
                    expr->operands.push_back(std::move(value));
                    lhs = std::move(expr);
                    pendingOperator.clear();
                }

                continue;
            }

            if (auto* additive = dynamic_cast<rxgrammar::RxParser::ConditionAdditiveExpressionContext*>(child)) {
                auto value = buildConditionAdditiveExpression(additive);
                if (!value) return nullptr;

                if (!lhs) {
                    lhs = std::move(value);
                }
                else {
                    if (pendingOperator.empty()) return nullptr;

                    auto expr = std::make_unique<Expr>();
                    expr->kind = ExprKind::Binary;
                    expr->text = pendingOperator;
                    expr->operands.push_back(std::move(lhs));
                    expr->operands.push_back(std::move(value));
                    lhs = std::move(expr);
                    pendingOperator.clear();
                }

                continue;
            }

            if (auto* shiftRight = dynamic_cast<rxgrammar::RxParser::ShiftRightContext*>(child)) {
                pendingOperator = shiftRight->getText();
                continue;
            }

            if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
                if (terminal->getText() == "<<") {
                    pendingOperator = "<<";
                }
            }
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionAdditiveExpression(
        rxgrammar::RxParser::ConditionAdditiveExpressionContext* ctx) {
        auto terms = ctx->conditionMultiplicativeExpression();
        auto operators = ctx->additiveOperator();

        auto lhs = buildConditionMultiplicativeExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionMultiplicativeExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedAdditiveExpression(
        rxgrammar::RxParser::ConditionClosedAdditiveExpressionContext* ctx) {
        auto terms = ctx->conditionMultiplicativeExpression();
        auto operators = ctx->additiveOperator();

        auto lhs = terms.empty()
                       ? buildConditionClosedMultiplicativeExpression(ctx->conditionClosedMultiplicativeExpression())
                       : buildConditionMultiplicativeExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;

            if (i + 1 < terms.size()) {
                rhs = buildConditionMultiplicativeExpression(terms[i + 1]);
            }
            else {
                rhs = buildConditionClosedMultiplicativeExpression(ctx->conditionClosedMultiplicativeExpression());
            }
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionMultiplicativeExpression(
        rxgrammar::RxParser::ConditionMultiplicativeExpressionContext* ctx) {
        auto terms = ctx->conditionCastExpression();
        auto operators = ctx->multiplicativeOperator();

        auto lhs = buildConditionCastExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildConditionCastExpression(terms[i + 1]);
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedMultiplicativeExpression(
        rxgrammar::RxParser::ConditionClosedMultiplicativeExpressionContext* ctx) {
        auto terms = ctx->conditionCastExpression();
        auto operators = ctx->multiplicativeOperator();

        auto lhs = terms.empty()
                       ? buildConditionClosedCastExpression(ctx->conditionClosedCastExpression())
                       : buildConditionCastExpression(terms[0]);
        if (!lhs) return nullptr;

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) {
                rhs = buildConditionCastExpression(terms[i + 1]);
            }
            else {
                rhs = buildConditionClosedCastExpression(ctx->conditionClosedCastExpression());
            }
            if (!rhs) return nullptr;

            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->text = operators[i]->getText();
            expr->operands.push_back(std::move(lhs));
            expr->operands.push_back(std::move(rhs));
            lhs = std::move(expr);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionCastExpression(
        rxgrammar::RxParser::ConditionCastExpressionContext* ctx) {
        auto lhs = buildConditionUnaryExpression(ctx->conditionUnaryExpression());
        if (!lhs) return nullptr;

        auto types = ctx->typeRef();
        for (auto* typeCtx : types) {
            auto cast = std::make_unique<Expr>();
            cast->kind = ExprKind::Cast;
            cast->text = typeCtx->getText();
            cast->operands.push_back(std::move(lhs));
            lhs = std::move(cast);
        }

        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionClosedCastExpression(
        rxgrammar::RxParser::ConditionClosedCastExpressionContext* ctx) {
        if (ctx->conditionUnaryExpression()) {
            return buildConditionUnaryExpression(ctx->conditionUnaryExpression());
        }

        auto lhs = buildConditionCastExpression(ctx->conditionCastExpression());
        if (!lhs) return nullptr;

        auto cast = std::make_unique<Expr>();
        cast->kind = ExprKind::Cast;
        cast->text = ctx->closedCastType()->getText();
        cast->operands.push_back(std::move(lhs));

        return cast;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionUnaryExpression(
        rxgrammar::RxParser::ConditionUnaryExpressionContext* ctx) {
        if (ctx->unaryOperator()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Unary;
            expr->text = ctx->unaryOperator()->getText();
            auto operand = buildConditionUnaryExpression(ctx->conditionUnaryExpression());
            if (!operand) return nullptr;
            expr->operands.push_back(std::move(operand));
            return expr;
        }

        return buildConditionPostfixExpression(ctx->conditionPostfixExpression());
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionPostfixExpression(
        rxgrammar::RxParser::ConditionPostfixExpressionContext* ctx) {
        auto base = buildConditionPrimary(ctx->conditionPrimary());
        if (!base) return nullptr;

        for (auto* suffix : ctx->postfixSuffix()) {
            if (suffix->callArguments()) {
                auto call = std::make_unique<Expr>();
                call->kind = ExprKind::Call;
                call->text = "call";
                call->operands.push_back(std::move(base));

                for (auto* arg : suffix->callArguments()->expression()) {
                    auto argument = buildExpression(arg);
                    if (!argument) return nullptr;
                    call->operands.push_back(std::move(argument));
                }
                base = std::move(call);
                continue;
            }

            if (suffix->LBRACKET()) {
                auto index = buildExpression(suffix->expression());
                if (!index) return nullptr;

                auto expr = std::make_unique<Expr>();
                expr->kind = ExprKind::Index;
                expr->text = "[]";
                expr->operands.push_back(std::move(base));
                expr->operands.push_back(std::move(index));
                base = std::move(expr);
                continue;
            }

            if (suffix->dotSuffix()) {
                auto* dot = suffix->dotSuffix();

                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<Expr>();
                    method->kind = ExprKind::Method;
                    method->text = dot->pathExprSegment()->getText();
                    method->operands.push_back(std::move(base));
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->operands.push_back(std::move(argument));
                    }
                    base = std::move(method);
                    continue;
                }

                if (dot->identifier()) {
                    auto field = std::make_unique<Expr>();
                    field->kind = ExprKind::Field;
                    field->text = dot->identifier()->getText();
                    field->operands.push_back(std::move(base));
                    base = std::move(field);
                    continue;
                }

                return nullptr;
            }
        }

        return base;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionPrimary(rxgrammar::RxParser::ConditionPrimaryContext* ctx) {
        if (ctx->conditionPrimaryWithoutBareBlock()) {
            return buildConditionPrimaryWithoutBareBlock(ctx->conditionPrimaryWithoutBareBlock());
        }

        if (ctx->blockExpression()) {
            return buildBlockExpressionAsExpr(ctx->blockExpression());
        }

        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionPrimaryWithoutBareBlock(
        rxgrammar::RxParser::ConditionPrimaryWithoutBareBlockContext* ctx) {
        if (ctx->literalExpression()) {
            return buildLiteralExpression(ctx->literalExpression());
        }

        if (ctx->pathInExpression()) {
            return buildPathInExpression(ctx->pathInExpression());
        }

        if (ctx->arrayExpression()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Array;
            expr->text = ctx->arrayExpression()->getText();
            for (auto* element : ctx->arrayExpression()->expression()) {
                auto value = buildExpression(element);
                if (!value) return nullptr;
                expr->operands.push_back(std::move(value));
            }
            return expr;
        }

        if (ctx->LPAREN()) {
            if (ctx->expression()) {
                return buildExpression(ctx->expression());
            }
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Unit;
            expr->text = "()";

            return expr;
        }

        if (ctx->ifExpression()) {
            return buildIfExpression(ctx->ifExpression());
        }

        if (ctx->LOOP()) {
            return buildLoopExpression(ctx->blockExpression());
        }

        if (ctx->WHILE()) {
            return buildWhileExpression(ctx->conditionExpression(), ctx->blockExpression());
        }

        if (ctx->BREAK()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Break;
            expr->text = "break";

            return expr;
        }

        if (ctx->RETURN()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Return;
            expr->text = "return";
            if (ctx->conditionExpression()) {
                auto value = buildConditionExpression(ctx->conditionExpression());
                if (!value) return nullptr;
                expr->operands.push_back(std::move(value));
            }

            return expr;
        }

        if (ctx->CONTINUE()) {
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Continue;
            expr->text = "continue";
            return expr;
        }

        return nullptr;
    }
}
