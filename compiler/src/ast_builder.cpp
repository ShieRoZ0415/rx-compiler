#include "ast_builder.h"

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
        auto buildFunction = [&](rxgrammar::RxParser::FunctionDefinitionContext* f) -> std::unique_ptr<Item>
        {
            auto item = std::make_unique<FunctionItem>();
            item->name = f->identifier()->getText();
            if (f->genericParams()) item->genericParams = buildGenericParams(f->genericParams());
            if (f->whereClause()) item->whereClause = buildWhereClause(f->whereClause());

            if (f->functionParameters()) {
                auto* ps = f->functionParameters();
                if (ps->selfParam()) {
                    auto* sp = ps->selfParam();
                    FunctionParam p;
                    p.name = "self";
                    p.isSelf = true;
                    p.isReference = sp->AMP() != nullptr;
                    p.isMutable = sp->MUT() != nullptr;
                    if (sp->lifetime()) p.lifetime = sp->lifetime()->getText();
                    item->parameters.push_back(std::move(p));
                }
                for (auto* pc : ps->functionParam()) {
                    FunctionParam p;
                    p.name = pc->identifierBinding()->identifier()->getText();
                    p.isMutable = pc->identifierBinding()->MUT() != nullptr;
                    p.type = buildTypeRef(pc->typeRef());
                    item->parameters.push_back(std::move(p));
                }
            }

            if (f->typeRef()) {
                item->resultType = buildTypeRef(f->typeRef());
            }
            else {
                item->resultType = std::make_unique<UnitType>();
            }

            item->body = buildBlock(f->blockExpression());
            return item;
        };

        auto buildConstant = [&](rxgrammar::RxParser::ConstantItemContext* c) -> std::unique_ptr<Item>
        {
            auto item = std::make_unique<ConstItem>();
            item->name = c->identifier()->getText();
            item->type = buildTypeRef(c->typeRef());
            item->value = buildConstValue(c->constValue());
            return item;
        };

        if (ctx->useDeclaration()) {
            auto item = std::make_unique<UseItem>();
            item->useTree = buildUseTree(ctx->useDeclaration()->useTree());
            return item;
        }
        if (ctx->functionDefinition()) {
            return buildFunction(ctx->functionDefinition());
        }
        if (ctx->structDefinition()) {
            auto* s = ctx->structDefinition();
            auto item = std::make_unique<StructItem>();
            item->name = s->identifier()->getText();
            if (s->genericParams()) item->genericParams = buildGenericParams(s->genericParams());
            if (s->whereClause()) item->whereClause = buildWhereClause(s->whereClause());
            for (auto* a : s->outerAttribute()) {
                for (auto* d : a->deriveName()) {
                    item->attributes.push_back(d->getText());
                }
            }
            for (auto* f : s->structField()) {
                StructField field;
                field.name = f->identifier()->getText();
                field.type = buildTypeRef(f->typeRef());
                item->fields.push_back(std::move(field));
            }
            return item;
        }
        if (ctx->constantItem()) {
            return buildConstant(ctx->constantItem());
        }
        if (ctx->inherentImpl()) {
            auto* impl = ctx->inherentImpl();
            auto item = std::make_unique<ImplItem>();
            item->selfType = buildTypeRef(impl->typeRef());
            if (impl->genericParams()) item->genericParams = buildGenericParams(impl->genericParams());
            if (impl->whereClause()) item->whereClause = buildWhereClause(impl->whereClause());
            for (auto* a : impl->associatedItem()) {
                if (a->constantItem()) {
                    item->items.push_back(buildConstant(a->constantItem()));
                }
                else if (a->functionDefinition()) {
                    item->items.push_back(buildFunction(a->functionDefinition()));
                }
            }
            return item;
        }

        return nullptr;
    }

    std::unique_ptr<Type> ASTBuilder::buildTypeRef(rxgrammar::RxParser::TypeRefContext* ctx) {
        if (!ctx) return nullptr;
        if (ctx->LPAREN() && ctx->RPAREN() && !ctx->typeRef()) {
            return std::make_unique<UnitType>();
        }
        if (ctx->typeRef()) return buildTypeRef(ctx->typeRef());
        if (ctx->typePath()) {
            auto type = std::make_unique<PathType>();
            type->path = buildTypePath(ctx->typePath());
            return type;
        }
        if (ctx->referenceType()) {
            auto* r = ctx->referenceType();
            auto inner = buildTypeRef(r->typeRef());
            if (r->ANDAND()) {
                auto innerRef = std::make_unique<ReferenceType>();
                if (r->lifetime()) innerRef->lifetime = r->lifetime()->getText();
                innerRef->isMutable = r->MUT() != nullptr;
                innerRef->inner = std::move(inner);
                auto outerRef = std::make_unique<ReferenceType>();
                outerRef->inner = std::move(innerRef);
                return outerRef;
            }
            auto type = std::make_unique<ReferenceType>();
            if (r->lifetime()) type->lifetime = r->lifetime()->getText();
            type->isMutable = r->MUT() != nullptr;
            type->inner = std::move(inner);
            return type;
        }
        if (ctx->arrayType()) {
            return buildArrayType(ctx->arrayType());
        }
        return nullptr;
    }

    std::unique_ptr<Type> ASTBuilder::buildArrayType(rxgrammar::RxParser::ArrayTypeContext* ctx) {
        auto type = std::make_unique<ArrayType>();
        type->element = buildTypeRef(ctx->typeRef());
        type->length = buildConstValue(ctx->constValue());
        return type;
    }

    std::unique_ptr<Type> ASTBuilder::buildClosedCastType(rxgrammar::RxParser::ClosedCastTypeContext* ctx) {
        if (!ctx) return nullptr;
        if (ctx->LPAREN()) {
            if (ctx->typeRef()) return buildTypeRef(ctx->typeRef());
            return std::make_unique<UnitType>();
        }
        if (ctx->arrayType()) {
            return buildArrayType(ctx->arrayType());
        }
        if (ctx->AMP() || ctx->ANDAND()) {
            auto inner = buildClosedCastType(ctx->closedCastType());
            if (ctx->ANDAND()) {
                auto innerRef = std::make_unique<ReferenceType>();
                if (ctx->lifetime()) innerRef->lifetime = ctx->lifetime()->getText();
                innerRef->isMutable = ctx->MUT() != nullptr;
                innerRef->inner = std::move(inner);
                auto outerRef = std::make_unique<ReferenceType>();
                outerRef->inner = std::move(innerRef);
                return outerRef;
            }
            auto type = std::make_unique<ReferenceType>();
            if (ctx->lifetime()) type->lifetime = ctx->lifetime()->getText();
            type->isMutable = ctx->MUT() != nullptr;
            type->inner = std::move(inner);
            return type;
        }
        if (ctx->pathIdentSegment()) {
            auto type = std::make_unique<PathType>();
            for (auto* segment : ctx->typePathSegment()) {
                PathSegment seg;
                seg.name = segment->pathIdentSegment()->getText();
                if (segment->genericArgs()) seg.args = buildGenericArgs(segment->genericArgs());
                type->path.segments.push_back(std::move(seg));
            }
            PathSegment last;
            last.name = ctx->pathIdentSegment()->getText();
            if (ctx->genericArgs()) last.args = buildGenericArgs(ctx->genericArgs());
            type->path.segments.push_back(std::move(last));
            return type;
        }
        return nullptr;
    }

    Path ASTBuilder::buildPath(rxgrammar::RxParser::PathInExpressionContext* ctx) {
        Path path;
        for (auto* segment : ctx->pathExprSegment()) {
            PathSegment seg;
            seg.name = segment->pathIdentSegment()->getText();
            if (segment->genericArgs()) seg.args = buildGenericArgs(segment->genericArgs());
            path.segments.push_back(std::move(seg));
        }
        return path;
    }

    Path ASTBuilder::buildTypePath(rxgrammar::RxParser::TypePathContext* ctx) {
        Path path;
        for (auto* segment : ctx->typePathSegment()) {
            PathSegment seg;
            seg.name = segment->pathIdentSegment()->getText();
            if (segment->genericArgs()) seg.args = buildGenericArgs(segment->genericArgs());
            path.segments.push_back(std::move(seg));
        }
        return path;
    }

    std::vector<GenericArg> ASTBuilder::buildGenericArgs(rxgrammar::RxParser::GenericArgsContext* ctx) {
        std::vector<GenericArg> args;
        for (auto* arg : ctx->genericArg()) {
            GenericArg g;
            if (arg->lifetime()) {
                g.isLifetime = true;
                g.lifetime = arg->lifetime()->getText();
            }
            else if (arg->typeRef()) {
                g.type = buildTypeRef(arg->typeRef());
            }
            args.push_back(std::move(g));
        }
        return args;
    }

    std::vector<LifetimeParam> ASTBuilder::buildGenericParams(rxgrammar::RxParser::GenericParamsContext* ctx) {
        std::vector<LifetimeParam> params;
        for (auto* p : ctx->lifetimeParam()) {
            LifetimeParam param;
            param.name = p->lifetime()->getText();
            if (p->lifetimeBounds()) {
                for (auto* l : p->lifetimeBounds()->lifetime()) {
                    param.bounds.push_back(l->getText());
                }
            }
            params.push_back(std::move(param));
        }
        return params;
    }

    std::vector<WherePredicate> ASTBuilder::buildWhereClause(rxgrammar::RxParser::WhereClauseContext* ctx) {
        std::vector<WherePredicate> predicates;
        for (auto* item : ctx->whereClauseItem()) {
            WherePredicate pred;
            if (item->lifetime()) {
                pred.isLifetime = true;
                pred.lifetime = item->lifetime()->getText();
                if (item->lifetimeBounds()) {
                    for (auto* l : item->lifetimeBounds()->lifetime()) {
                        pred.bounds.push_back(l->getText());
                    }
                }
            }
            else if (item->typeRef()) {
                pred.type = buildTypeRef(item->typeRef());
                if (item->typeParamBounds()) {
                    for (auto* l : item->typeParamBounds()->lifetime()) {
                        pred.bounds.push_back(l->getText());
                    }
                }
            }
            predicates.push_back(std::move(pred));
        }
        return predicates;
    }

    UseTree ASTBuilder::buildUseTree(rxgrammar::RxParser::UseTreeContext* ctx) {
        UseTree tree;
        std::string prefix;
        if (ctx->usePath()) prefix = ctx->usePath()->getText();

        if (ctx->STAR()) {
            tree.path = prefix;
            tree.isGlob = true;
            return tree;
        }
        if (ctx->LBRACE()) {
            tree.path = prefix;
            tree.isGroup = true;
            for (auto* child : ctx->useTree()) {
                tree.children.push_back(buildUseTree(child));
            }
            return tree;
        }

        tree.path = prefix;
        if (ctx->AS()) {
            if (ctx->identifier()) tree.alias = ctx->identifier()->getText();
            else if (ctx->UNDERSCORE()) tree.alias = "_";
        }
        return tree;
    }

    std::unique_ptr<Expr> ASTBuilder::buildMagnitude(rxgrammar::RxParser::MagnitudeContext* ctx) {
        if (!ctx) return nullptr;
        if (ctx->INTEGER_LITERAL()) {
            auto expr = std::make_unique<IntegerLiteralExpr>();
            expr->text = ctx->INTEGER_LITERAL()->getText();
            return expr;
        }
        if (ctx->pathInExpression()) return buildPathExpr(ctx->pathInExpression());
        if (ctx->magnitude()) return buildMagnitude(ctx->magnitude());
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConstValue(rxgrammar::RxParser::ConstValueContext* ctx) {
        if (!ctx) return nullptr;
        if (ctx->INTEGER_LITERAL()) {
            auto expr = std::make_unique<IntegerLiteralExpr>();
            expr->text = ctx->INTEGER_LITERAL()->getText();
            return expr;
        }
        if (ctx->TRUE()) {
            auto expr = std::make_unique<BooleanLiteralExpr>();
            expr->value = true;
            return expr;
        }
        if (ctx->FALSE()) {
            auto expr = std::make_unique<BooleanLiteralExpr>();
            expr->value = false;
            return expr;
        }
        if (ctx->pathInExpression()) return buildPathExpr(ctx->pathInExpression());
        if (ctx->MINUS()) {
            auto expr = std::make_unique<UnaryExpr>();
            expr->op = "-";
            expr->operand = buildMagnitude(ctx->magnitude());
            return expr;
        }
        if (ctx->constValue()) return buildConstValue(ctx->constValue());
        return nullptr;
    }

    std::unique_ptr<Block> ASTBuilder::buildBlock(rxgrammar::RxParser::BlockExpressionContext* ctx) {
        auto block = std::make_unique<Block>();
        for (auto* statementCtx : ctx->statement()) block->statements.push_back(buildStatement(statementCtx));
        if (ctx->statementExpression()) block->tail = buildStatementAssignmentExpression(
            ctx->statementExpression()->statementAssignmentExpression());
        return block;
    }

    std::unique_ptr<Stmt> ASTBuilder::buildStatement(rxgrammar::RxParser::StatementContext* ctx) {
        if (ctx->letStatement()) {
            return buildLetStatement(ctx->letStatement());
        }
        if (ctx->expressionWithBlock()) {
            auto stmt = std::make_unique<ExprStmt>();
            stmt->expression = buildExpressionWithBlock(ctx->expressionWithBlock());
            stmt->hasSemicolon = ctx->SEMI() != nullptr;
            return stmt;
        }
        if (ctx->statementExpression()) {
            auto stmt = std::make_unique<ExprStmt>();
            stmt->expression = buildStatementAssignmentExpression(
                ctx->statementExpression()->statementAssignmentExpression());
            stmt->hasSemicolon = ctx->SEMI() != nullptr;
            return stmt;
        }
        return std::make_unique<EmptyStmt>();
    }

    std::unique_ptr<Stmt> ASTBuilder::buildLetStatement(rxgrammar::RxParser::LetStatementContext* ctx) {
        auto stmt = std::make_unique<LetStmt>();
        auto* binding = ctx->identifierBinding();
        stmt->name = binding->identifier()->getText();
        stmt->isMutable = binding->MUT() != nullptr;
        if (ctx->typeRef()) stmt->type = buildTypeRef(ctx->typeRef());
        if (ctx->expression()) stmt->initializer = buildExpression(ctx->expression());
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildCastExpression(rxgrammar::RxParser::CastExpressionContext* ctx) {
        auto lhs = buildUnaryExpression(ctx->unaryExpression());
        if (!lhs) return nullptr;
        for (auto* typeCtx : ctx->typeRef()) {
            auto cast = std::make_unique<CastExpr>();
            cast->type = buildTypeRef(typeCtx);
            cast->value = std::move(lhs);
            lhs = std::move(cast);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLiteralExpression(rxgrammar::RxParser::LiteralExpressionContext* ctx) {
        if (ctx->INTEGER_LITERAL()) {
            auto expr = std::make_unique<IntegerLiteralExpr>();
            expr->text = ctx->INTEGER_LITERAL()->getText();
            return expr;
        }
        if (ctx->TRUE()) {
            auto expr = std::make_unique<BooleanLiteralExpr>();
            expr->value = true;
            return expr;
        }
        if (ctx->FALSE()) {
            auto expr = std::make_unique<BooleanLiteralExpr>();
            expr->value = false;
            return expr;
        }
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildNonBlockPrimary(rxgrammar::RxParser::NonBlockPrimaryContext* ctx) {
        if (ctx->literalExpression()) return buildLiteralExpression(ctx->literalExpression());

        if (ctx->pathInExpression()) {
            auto path = buildPath(ctx->pathInExpression());
            if (!ctx->LBRACE()) {
                auto expr = std::make_unique<PathExpr>();
                expr->path = std::move(path);
                return expr;
            }
            auto expr = std::make_unique<StructExpr>();
            expr->path = std::move(path);
            if (ctx->structExprFields()) {
                for (auto* field : ctx->structExprFields()->structExprField()) {
                    auto value = buildExpression(field->expression());
                    if (!value) return nullptr;
                    expr->fields.push_back(field->identifier()->getText());
                    expr->values.push_back(std::move(value));
                }
            }
            return expr;
        }

        if (ctx->arrayExpression()) {
            auto* array = ctx->arrayExpression();
            auto expr = std::make_unique<ArrayExpr>();
            if (array->SEMI()) {
                if (!array->expression().empty()) {
                    expr->elements.push_back(buildExpression(array->expression(0)));
                }
                expr->repeat = buildConstValue(array->constValue());
            }
            else {
                for (auto* element : array->expression()) {
                    auto value = buildExpression(element);
                    if (!value) return nullptr;
                    expr->elements.push_back(std::move(value));
                }
            }
            return expr;
        }

        if (ctx->LPAREN()) {
            if (ctx->expression()) return buildExpression(ctx->expression());
            return std::make_unique<UnitExpr>();
        }

        if (ctx->BREAK()) {
            auto expr = std::make_unique<BreakExpr>();
            if (ctx->expression()) expr->value = buildExpression(ctx->expression());
            return expr;
        }

        if (ctx->RETURN()) {
            auto expr = std::make_unique<ReturnExpr>();
            if (ctx->expression()) expr->value = buildExpression(ctx->expression());
            return expr;
        }

        if (ctx->CONTINUE()) {
            return std::make_unique<ContinueExpr>();
        }
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildPathExpr(rxgrammar::RxParser::PathInExpressionContext* ctx) {
        auto expr = std::make_unique<PathExpr>();
        expr->path = buildPath(ctx);
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
                auto call = std::make_unique<CallExpr>();
                call->callee = std::move(base);
                for (auto* arg : suffix->callArguments()->expression()) {
                    auto argument = buildExpression(arg);
                    if (!argument) return nullptr;
                    call->args.push_back(std::move(argument));
                }
                base = std::move(call);
                continue;
            }
            if (suffix->LBRACKET()) {
                auto index = buildExpression(suffix->expression());
                if (!index) return nullptr;
                auto expr = std::make_unique<IndexExpr>();
                expr->base = std::move(base);
                expr->index = std::move(index);
                base = std::move(expr);
                continue;
            }
            if (suffix->dotSuffix()) {
                auto* dot = suffix->dotSuffix();
                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<MethodCallExpr>();
                    method->receiver = std::move(base);
                    method->method = dot->pathExprSegment()->pathIdentSegment()->getText();
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->args.push_back(std::move(argument));
                    }
                    base = std::move(method);
                    continue;
                }
                if (dot->identifier()) {
                    auto field = std::make_unique<FieldAccessExpr>();
                    field->receiver = std::move(base);
                    field->field = dot->identifier()->getText();
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
            auto operand = buildUnaryExpression(ctx->unaryExpression());
            if (!operand) return nullptr;
            auto expr = std::make_unique<UnaryExpr>();
            expr->op = ctx->unaryOperator()->getText();
            expr->operand = std::move(operand);
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

        auto expr = std::make_unique<AssignmentExpr>();
        expr->op = assignmentOperator->getText();
        expr->lhs = std::move(lhs);
        expr->rhs = std::move(rhs);
        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLogicalOrExpression(rxgrammar::RxParser::LogicalOrExpressionContext* ctx) {
        auto lhs = buildLogicalAndExpression(ctx->logicalAndExpression(0));
        if (!lhs) return nullptr;
        auto terms = ctx->logicalAndExpression();
        auto operators = ctx->OROR();
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildLogicalAndExpression(terms[i + 1]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = "<";
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
            return binary;
        }

        auto lhs = buildBitOrExpression(ctx->bitOrExpression(0));
        if (!lhs) return nullptr;
        if (ctx->comparisonExceptLt()) {
            auto rhs = buildBitOrExpression(ctx->bitOrExpression(1));
            if (!rhs) return nullptr;
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = ctx->comparisonExceptLt()->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildShiftExpression(rxgrammar::RxParser::ShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* closed = dynamic_cast<rxgrammar::RxParser::ClosedAdditiveExpressionContext*>(child)) {
                auto value = buildClosedAdditiveExpression(closed);
                if (!value) return nullptr;
                if (!lhs) {
                    lhs = std::move(value);
                    continue;
                }
                if (pendingOperator.empty()) return nullptr;
                auto binary = std::make_unique<BinaryExpr>();
                binary->op = pendingOperator;
                binary->lhs = std::move(lhs);
                binary->rhs = std::move(value);
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
                auto binary = std::make_unique<BinaryExpr>();
                binary->op = pendingOperator;
                binary->lhs = std::move(lhs);
                binary->rhs = std::move(value);
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

    std::unique_ptr<Expr> ASTBuilder::buildStatementAssignmentExpression(
        rxgrammar::RxParser::StatementAssignmentExpressionContext* ctx) {
        auto lhs = buildStatementLogicalOrExpression(ctx->statementLogicalOrExpression());
        if (!lhs) return nullptr;
        auto* assignmentOperator = ctx->assignmentOperator();
        if (!assignmentOperator) return lhs;

        auto rhs = buildExpression(ctx->expression());
        if (!rhs) return nullptr;

        auto expr = std::make_unique<AssignmentExpr>();
        expr->op = assignmentOperator->getText();
        expr->lhs = std::move(lhs);
        expr->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildClosedCastExpression(rxgrammar::RxParser::ClosedCastExpressionContext* ctx) {
        if (ctx->unaryExpression()) return buildUnaryExpression(ctx->unaryExpression());

        auto lhs = buildCastExpression(ctx->castExpression());
        if (!lhs) return nullptr;

        auto cast = std::make_unique<CastExpr>();
        cast->type = buildClosedCastType(ctx->closedCastType());
        cast->value = std::move(lhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr>
    ASTBuilder::buildClosedShiftExpression(rxgrammar::RxParser::ClosedShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* closed = dynamic_cast<rxgrammar::RxParser::ClosedAdditiveExpressionContext*>(child)) {
                auto value = buildClosedAdditiveExpression(closed);
                if (!value) return nullptr;
                if (!lhs) {
                    lhs = std::move(value);
                }
                else {
                    if (pendingOperator.empty()) return nullptr;
                    auto binary = std::make_unique<BinaryExpr>();
                    binary->op = pendingOperator;
                    binary->lhs = std::move(lhs);
                    binary->rhs = std::move(value);
                    lhs = std::move(binary);
                    pendingOperator.clear();
                }
                continue;
            }
            if (auto* additive = dynamic_cast<rxgrammar::RxParser::AdditiveExpressionContext*>(child)) {
                auto value = buildAdditiveExpression(additive);
                if (!value) return nullptr;
                if (!lhs) {
                    lhs = std::move(value);
                }
                else {
                    if (pendingOperator.empty()) return nullptr;
                    auto binary = std::make_unique<BinaryExpr>();
                    binary->op = pendingOperator;
                    binary->lhs = std::move(lhs);
                    binary->rhs = std::move(value);
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

        for (size_t i = 0; i < operators.size(); ++i) {
            std::unique_ptr<Expr> rhs;
            if (i + 1 < terms.size()) rhs = buildShiftExpression(terms[i + 1]);
            else rhs = buildClosedShiftExpression(ctx->closedShiftExpression());
            if (!rhs) return nullptr;
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildBitAndExpression(terms[i + 1]);
            else rhs = buildClosedBitAndExpression(ctx->closedBitAndExpression());
            if (!rhs) return nullptr;
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildBitXorExpression(terms[i + 1]);
            else rhs = buildClosedBitXorExpression(ctx->closedBitXorExpression());
            if (!rhs) return nullptr;
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = ctx->comparisonExceptLt()->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
            lhs = std::move(binary);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementShiftExpression(
        rxgrammar::RxParser::StatementShiftExpressionContext* ctx) {
        std::unique_ptr<Expr> lhs;
        std::string pendingOperator;

        for (auto* child : ctx->children) {
            if (auto* statementAdditive = dynamic_cast<rxgrammar::RxParser::StatementAdditiveExpressionContext*>(
                child)) {
                auto value = buildStatementAdditiveExpression(statementAdditive);
                if (!value) return nullptr;
                if (!lhs) {
                    lhs = std::move(value);
                    continue;
                }
                if (pendingOperator.empty()) return nullptr;
                auto binary = std::make_unique<BinaryExpr>();
                binary->op = pendingOperator;
                binary->lhs = std::move(lhs);
                binary->rhs = std::move(value);
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
                auto binary = std::make_unique<BinaryExpr>();
                binary->op = pendingOperator;
                binary->lhs = std::move(lhs);
                binary->rhs = std::move(value);
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

    std::unique_ptr<Expr> ASTBuilder::buildStatementAdditiveExpression(
        rxgrammar::RxParser::StatementAdditiveExpressionContext* ctx) {
        auto lhs = buildStatementMultiplicativeExpression(ctx->statementMultiplicativeExpression());
        if (!lhs) return nullptr;

        auto terms = ctx->multiplicativeExpression();
        auto operators = ctx->additiveOperator();
        for (size_t i = 0; i < operators.size(); ++i) {
            auto rhs = buildMultiplicativeExpression(terms[i]);
            if (!rhs) return nullptr;
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto operand = buildUnaryExpression(ctx->unaryExpression());
            if (!operand) return nullptr;
            auto expr = std::make_unique<UnaryExpr>();
            expr->op = ctx->unaryOperator()->getText();
            expr->operand = std::move(operand);
            return expr;
        }
        if (ctx->statementPostfixExpression()) {
            return buildStatementPostfixExpression(ctx->statementPostfixExpression());
        }
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildStatementPostfixExpression(
        rxgrammar::RxParser::StatementPostfixExpressionContext* ctx) {
        std::unique_ptr<Expr> base;

        if (ctx->nonBlockPrimary()) {
            base = buildNonBlockPrimary(ctx->nonBlockPrimary());
        }
        else {
            base = buildExpressionWithBlock(ctx->expressionWithBlock());
            if (!base) return nullptr;

            if (ctx->dotSuffix()) {
                auto* dot = ctx->dotSuffix();
                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<MethodCallExpr>();
                    method->receiver = std::move(base);
                    method->method = dot->pathExprSegment()->pathIdentSegment()->getText();
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->args.push_back(std::move(argument));
                    }
                    base = std::move(method);
                }
                else if (dot->identifier()) {
                    auto field = std::make_unique<FieldAccessExpr>();
                    field->receiver = std::move(base);
                    field->field = dot->identifier()->getText();
                    base = std::move(field);
                }
                else return nullptr;
            }
        }
        if (!base) return nullptr;

        for (auto* suffix : ctx->postfixSuffix()) {
            if (suffix->callArguments()) {
                auto call = std::make_unique<CallExpr>();
                call->callee = std::move(base);
                for (auto* arg : suffix->callArguments()->expression()) {
                    auto argument = buildExpression(arg);
                    if (!argument) return nullptr;
                    call->args.push_back(std::move(argument));
                }
                base = std::move(call);
                continue;
            }
            if (suffix->LBRACKET()) {
                auto index = buildExpression(suffix->expression());
                if (!index) return nullptr;
                auto expr = std::make_unique<IndexExpr>();
                expr->base = std::move(base);
                expr->index = std::move(index);
                base = std::move(expr);
                continue;
            }
            if (suffix->dotSuffix()) {
                auto* dot = suffix->dotSuffix();
                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<MethodCallExpr>();
                    method->receiver = std::move(base);
                    method->method = dot->pathExprSegment()->pathIdentSegment()->getText();
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->args.push_back(std::move(argument));
                    }
                    base = std::move(method);
                    continue;
                }
                if (dot->identifier()) {
                    auto field = std::make_unique<FieldAccessExpr>();
                    field->receiver = std::move(base);
                    field->field = dot->identifier()->getText();
                    base = std::move(field);
                    continue;
                }
                return nullptr;
            }
            return nullptr;
        }
        return base;
    }

    std::unique_ptr<Expr> ASTBuilder::buildBlockExpr(rxgrammar::RxParser::BlockExpressionContext* ctx) {
        auto expr = std::make_unique<BlockExpr>();
        expr->block = buildBlock(ctx);
        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildExpressionWithBlock(rxgrammar::RxParser::ExpressionWithBlockContext* ctx) {
        if (ctx->ifExpression()) return buildIfExpression(ctx->ifExpression());
        if (ctx->LOOP()) return buildLoopExpression(ctx->blockExpression());
        if (ctx->WHILE()) return buildWhileExpression(ctx->conditionExpression(), ctx->blockExpression());
        if (ctx->blockExpression()) return buildBlockExpr(ctx->blockExpression());
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildLoopExpression(rxgrammar::RxParser::BlockExpressionContext* ctx) {
        auto expr = std::make_unique<LoopExpr>();
        expr->body = buildBlockExpr(ctx);
        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildWhileExpression(rxgrammar::RxParser::ConditionExpressionContext* condition,
                                                           rxgrammar::RxParser::BlockExpressionContext* body) {
        auto cond = buildConditionAssignmentExpression(condition->conditionAssignmentExpression());
        if (!cond) return nullptr;
        auto expr = std::make_unique<WhileExpr>();
        expr->condition = std::move(cond);
        expr->body = buildBlockExpr(body);
        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildIfExpression(rxgrammar::RxParser::IfExpressionContext* ctx) {
        auto condition =
            buildConditionAssignmentExpression(ctx->conditionExpression()->conditionAssignmentExpression());
        if (!condition) return nullptr;

        auto blocks = ctx->blockExpression();
        if (blocks.empty()) return nullptr;

        auto expr = std::make_unique<IfExpr>();
        expr->condition = std::move(condition);
        expr->thenBranch = buildBlockExpr(blocks[0]);

        if (ctx->ifExpression()) {
            expr->elseBranch = buildIfExpression(ctx->ifExpression());
        }
        else if (blocks.size() >= 2) {
            expr->elseBranch = buildBlockExpr(blocks[1]);
        }
        return expr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionAssignmentExpression(
        rxgrammar::RxParser::ConditionAssignmentExpressionContext* ctx) {
        auto lhs = buildConditionLogicalOrExpression(ctx->conditionLogicalOrExpression());
        if (!lhs) return nullptr;

        auto* op = ctx->assignmentOperator();
        if (!op) return lhs;

        auto rhs = buildConditionAssignmentExpression(ctx->conditionExpression()->conditionAssignmentExpression());
        if (!rhs) return nullptr;

        auto expr = std::make_unique<AssignmentExpr>();
        expr->op = op->getText();
        expr->lhs = std::move(lhs);
        expr->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto binary = std::make_unique<BinaryExpr>();
            binary->op = operators[i]->getText();
            binary->lhs = std::move(lhs);
            binary->rhs = std::move(rhs);
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
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = "<";
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
            return expr;
        }

        auto lhs = buildConditionBitOrExpression(ctx->conditionBitOrExpression(0));
        if (!lhs) return nullptr;

        if (ctx->comparisonExceptLt()) {
            auto rhs = buildConditionBitOrExpression(ctx->conditionBitOrExpression(1));
            if (!rhs) return nullptr;
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = ctx->comparisonExceptLt()->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildConditionBitXorExpression(terms[i + 1]);
            else rhs = buildConditionClosedBitXorExpression(ctx->conditionClosedBitXorExpression());
            if (!rhs) return nullptr;
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildConditionBitAndExpression(terms[i + 1]);
            else rhs = buildConditionClosedBitAndExpression(ctx->conditionClosedBitAndExpression());
            if (!rhs) return nullptr;
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildConditionShiftExpression(terms[i + 1]);
            else rhs = buildConditionClosedShiftExpression(ctx->conditionClosedShiftExpression());
            if (!rhs) return nullptr;
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
                    auto expr = std::make_unique<BinaryExpr>();
                    expr->op = pendingOperator;
                    expr->lhs = std::move(lhs);
                    expr->rhs = std::move(value);
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
                    auto expr = std::make_unique<BinaryExpr>();
                    expr->op = pendingOperator;
                    expr->lhs = std::move(lhs);
                    expr->rhs = std::move(value);
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
                if (terminal->getText() == "<<") pendingOperator = "<<";
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
                    auto expr = std::make_unique<BinaryExpr>();
                    expr->op = pendingOperator;
                    expr->lhs = std::move(lhs);
                    expr->rhs = std::move(value);
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
                    auto expr = std::make_unique<BinaryExpr>();
                    expr->op = pendingOperator;
                    expr->lhs = std::move(lhs);
                    expr->rhs = std::move(value);
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
                if (terminal->getText() == "<<") pendingOperator = "<<";
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
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildConditionMultiplicativeExpression(terms[i + 1]);
            else rhs = buildConditionClosedMultiplicativeExpression(ctx->conditionClosedMultiplicativeExpression());
            if (!rhs) return nullptr;
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
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
            if (i + 1 < terms.size()) rhs = buildConditionCastExpression(terms[i + 1]);
            else rhs = buildConditionClosedCastExpression(ctx->conditionClosedCastExpression());
            if (!rhs) return nullptr;
            auto expr = std::make_unique<BinaryExpr>();
            expr->op = operators[i]->getText();
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
            lhs = std::move(expr);
        }
        return lhs;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionCastExpression(
        rxgrammar::RxParser::ConditionCastExpressionContext* ctx) {
        auto lhs = buildConditionUnaryExpression(ctx->conditionUnaryExpression());
        if (!lhs) return nullptr;

        for (auto* typeCtx : ctx->typeRef()) {
            auto cast = std::make_unique<CastExpr>();
            cast->type = buildTypeRef(typeCtx);
            cast->value = std::move(lhs);
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

        auto cast = std::make_unique<CastExpr>();
        cast->type = buildClosedCastType(ctx->closedCastType());
        cast->value = std::move(lhs);
        return cast;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionUnaryExpression(
        rxgrammar::RxParser::ConditionUnaryExpressionContext* ctx) {
        if (ctx->unaryOperator()) {
            auto operand = buildConditionUnaryExpression(ctx->conditionUnaryExpression());
            if (!operand) return nullptr;
            auto expr = std::make_unique<UnaryExpr>();
            expr->op = ctx->unaryOperator()->getText();
            expr->operand = std::move(operand);
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
                auto call = std::make_unique<CallExpr>();
                call->callee = std::move(base);
                for (auto* arg : suffix->callArguments()->expression()) {
                    auto argument = buildExpression(arg);
                    if (!argument) return nullptr;
                    call->args.push_back(std::move(argument));
                }
                base = std::move(call);
                continue;
            }
            if (suffix->LBRACKET()) {
                auto index = buildExpression(suffix->expression());
                if (!index) return nullptr;
                auto expr = std::make_unique<IndexExpr>();
                expr->base = std::move(base);
                expr->index = std::move(index);
                base = std::move(expr);
                continue;
            }
            if (suffix->dotSuffix()) {
                auto* dot = suffix->dotSuffix();
                if (dot->pathExprSegment() && dot->callArguments()) {
                    auto method = std::make_unique<MethodCallExpr>();
                    method->receiver = std::move(base);
                    method->method = dot->pathExprSegment()->pathIdentSegment()->getText();
                    for (auto* arg : dot->callArguments()->expression()) {
                        auto argument = buildExpression(arg);
                        if (!argument) return nullptr;
                        method->args.push_back(std::move(argument));
                    }
                    base = std::move(method);
                    continue;
                }
                if (dot->identifier()) {
                    auto field = std::make_unique<FieldAccessExpr>();
                    field->receiver = std::move(base);
                    field->field = dot->identifier()->getText();
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
            return buildBlockExpr(ctx->blockExpression());
        }
        return nullptr;
    }

    std::unique_ptr<Expr> ASTBuilder::buildConditionPrimaryWithoutBareBlock(
        rxgrammar::RxParser::ConditionPrimaryWithoutBareBlockContext* ctx) {
        if (ctx->literalExpression()) {
            return buildLiteralExpression(ctx->literalExpression());
        }

        if (ctx->pathInExpression()) {
            return buildPathExpr(ctx->pathInExpression());
        }

        if (ctx->arrayExpression()) {
            auto* array = ctx->arrayExpression();
            auto expr = std::make_unique<ArrayExpr>();
            if (array->SEMI()) {
                if (!array->expression().empty()) {
                    expr->elements.push_back(buildExpression(array->expression(0)));
                }
                expr->repeat = buildConstValue(array->constValue());
            }
            else {
                for (auto* element : array->expression()) {
                    auto value = buildExpression(element);
                    if (!value) return nullptr;
                    expr->elements.push_back(std::move(value));
                }
            }
            return expr;
        }

        if (ctx->LPAREN()) {
            if (ctx->expression()) return buildExpression(ctx->expression());
            return std::make_unique<UnitExpr>();
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
            return std::make_unique<BreakExpr>();
        }

        if (ctx->RETURN()) {
            auto expr = std::make_unique<ReturnExpr>();
            if (ctx->conditionExpression()) {
                expr->value = buildConditionAssignmentExpression(
                    ctx->conditionExpression()->conditionAssignmentExpression());
            }
            return expr;
        }

        if (ctx->CONTINUE()) {
            return std::make_unique<ContinueExpr>();
        }

        return nullptr;
    }
}
