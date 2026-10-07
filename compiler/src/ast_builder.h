#pragma once

#include "ast.h"
#include "RxParser.h"

namespace rx
{
    class ASTBuilder {
    public:
        std::unique_ptr<Crate> buildCrate(rxgrammar::RxParser::CrateContext* ctx);

    private:
        std::unique_ptr<Item> buildItem(rxgrammar::RxParser::ItemContext* ctx);
        std::unique_ptr<Block> buildBlock(rxgrammar::RxParser::BlockExpressionContext* ctx);
        std::unique_ptr<Stmt> buildStatement(rxgrammar::RxParser::StatementContext* ctx);
        std::unique_ptr<Stmt> buildLetStatement(rxgrammar::RxParser::LetStatementContext* ctx);
        std::unique_ptr<Expr> buildExpression(rxgrammar::RxParser::ExpressionContext* ctx);
        std::unique_ptr<Expr> buildAdditiveExpression(rxgrammar::RxParser::AdditiveExpressionContext* ctx);
        std::unique_ptr<Expr> buildMultiplicativeExpression(rxgrammar::RxParser::MultiplicativeExpressionContext* ctx);
        std::unique_ptr<Expr> buildCastExpression(rxgrammar::RxParser::CastExpressionContext* ctx);
        std::unique_ptr<Expr> buildPathInExpression(rxgrammar::RxParser::PathInExpressionContext* ctx);
        std::unique_ptr<Expr> buildLiteralExpression(rxgrammar::RxParser::LiteralExpressionContext* ctx);
        std::unique_ptr<Expr> buildNonBlockPrimary(rxgrammar::RxParser::NonBlockPrimaryContext* ctx);
        std::unique_ptr<Expr> buildPrimaryExpression(rxgrammar::RxParser::PrimaryExpressionContext* ctx);
        std::unique_ptr<Expr> buildPostfixExpression(rxgrammar::RxParser::PostfixExpressionContext* ctx);
        std::unique_ptr<Expr> buildUnaryExpression(rxgrammar::RxParser::UnaryExpressionContext* ctx);
        std::unique_ptr<Expr> buildAssignmentExpression(rxgrammar::RxParser::AssignmentExpressionContext* ctx);
        std::unique_ptr<Expr> buildLogicalOrExpression(rxgrammar::RxParser::LogicalOrExpressionContext* ctx);
        std::unique_ptr<Expr> buildLogicalAndExpression(rxgrammar::RxParser::LogicalAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildComparisonExpression(rxgrammar::RxParser::ComparisonExpressionContext* ctx);
        std::unique_ptr<Expr> buildBitOrExpression(rxgrammar::RxParser::BitOrExpressionContext* ctx);
        std::unique_ptr<Expr> buildBitAndExpression(rxgrammar::RxParser::BitAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildBitXorExpression(rxgrammar::RxParser::BitXorExpressionContext* ctx);
        std::unique_ptr<Expr> buildShiftExpression(rxgrammar::RxParser::ShiftExpressionContext* ctx);

        std::unique_ptr<Expr> buildClosedCastExpression(rxgrammar::RxParser::ClosedCastExpressionContext* ctx);
        std::unique_ptr<Expr> buildClosedMultiplicativeExpression(
            rxgrammar::RxParser::ClosedMultiplicativeExpressionContext* ctx);
        std::unique_ptr<Expr> buildClosedAdditiveExpression(rxgrammar::RxParser::ClosedAdditiveExpressionContext* ctx);
        std::unique_ptr<Expr> buildClosedShiftExpression(rxgrammar::RxParser::ClosedShiftExpressionContext* ctx);
        std::unique_ptr<Expr> buildClosedBitAndExpression(rxgrammar::RxParser::ClosedBitAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildClosedBitXorExpression(rxgrammar::RxParser::ClosedBitXorExpressionContext* ctx);
        std::unique_ptr<Expr> buildClosedBitOrExpression(rxgrammar::RxParser::ClosedBitOrExpressionContext* ctx);

        std::unique_ptr<Expr> buildStatementAssignmentExpression(
            rxgrammar::RxParser::StatementAssignmentExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementExpression(rxgrammar::RxParser::StatementExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementLogicalOrExpression(
            rxgrammar::RxParser::StatementLogicalOrExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementLogicalAndExpression(
            rxgrammar::RxParser::StatementLogicalAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementComparisonExpression(
            rxgrammar::RxParser::StatementComparisonExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementBitOrExpression(rxgrammar::RxParser::StatementBitOrExpressionContext* ctx);
        std::unique_ptr<Expr>
        buildStatementBitXorExpression(rxgrammar::RxParser::StatementBitXorExpressionContext* ctx);
        std::unique_ptr<Expr>
        buildStatementBitAndExpression(rxgrammar::RxParser::StatementBitAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementShiftExpression(rxgrammar::RxParser::StatementShiftExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementAdditiveExpression(
            rxgrammar::RxParser::StatementAdditiveExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementMultiplicativeExpression(
            rxgrammar::RxParser::StatementMultiplicativeExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementCastExpression(rxgrammar::RxParser::StatementCastExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementUnaryExpression(rxgrammar::RxParser::StatementUnaryExpressionContext* ctx);
        std::unique_ptr<Expr> buildStatementPostfixExpression(
            rxgrammar::RxParser::StatementPostfixExpressionContext* ctx);

        std::unique_ptr<Expr> buildExpressionWithBlock(
            rxgrammar::RxParser::ExpressionWithBlockContext* ctx);
        std::unique_ptr<Expr> buildIfExpression(
            rxgrammar::RxParser::IfExpressionContext* ctx);
        std::unique_ptr<Expr> buildLoopExpression(
            rxgrammar::RxParser::BlockExpressionContext* ctx);
        std::unique_ptr<Expr> buildWhileExpression(
            rxgrammar::RxParser::ConditionExpressionContext* condition,
            rxgrammar::RxParser::BlockExpressionContext* body);
        std::unique_ptr<Expr> buildBlockExpressionAsExpr(
            rxgrammar::RxParser::BlockExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionExpression(
            rxgrammar::RxParser::ConditionExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionAssignmentExpression(
            rxgrammar::RxParser::ConditionAssignmentExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionLogicalOrExpression(
            rxgrammar::RxParser::ConditionLogicalOrExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionLogicalAndExpression(
            rxgrammar::RxParser::ConditionLogicalAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionComparisonExpression(
            rxgrammar::RxParser::ConditionComparisonExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionBitOrExpression(
            rxgrammar::RxParser::ConditionBitOrExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedBitOrExpression(
            rxgrammar::RxParser::ConditionClosedBitOrExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionBitXorExpression(
            rxgrammar::RxParser::ConditionBitXorExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedBitXorExpression(
            rxgrammar::RxParser::ConditionClosedBitXorExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionBitAndExpression(
            rxgrammar::RxParser::ConditionBitAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedBitAndExpression(
            rxgrammar::RxParser::ConditionClosedBitAndExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionShiftExpression(
            rxgrammar::RxParser::ConditionShiftExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedShiftExpression(
            rxgrammar::RxParser::ConditionClosedShiftExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionAdditiveExpression(
            rxgrammar::RxParser::ConditionAdditiveExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedAdditiveExpression(
            rxgrammar::RxParser::ConditionClosedAdditiveExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionMultiplicativeExpression(
            rxgrammar::RxParser::ConditionMultiplicativeExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedMultiplicativeExpression(
            rxgrammar::RxParser::ConditionClosedMultiplicativeExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionCastExpression(
            rxgrammar::RxParser::ConditionCastExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionClosedCastExpression(
            rxgrammar::RxParser::ConditionClosedCastExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionUnaryExpression(
            rxgrammar::RxParser::ConditionUnaryExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionPostfixExpression(
            rxgrammar::RxParser::ConditionPostfixExpressionContext* ctx);
        std::unique_ptr<Expr> buildConditionPrimary(
            rxgrammar::RxParser::ConditionPrimaryContext* ctx);
        std::unique_ptr<Expr> buildConditionPrimaryWithoutBareBlock(
            rxgrammar::RxParser::ConditionPrimaryWithoutBareBlockContext* ctx);
    };
}
