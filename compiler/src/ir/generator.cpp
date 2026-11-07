#include "generator.h"

#include <iostream>
#include <cassert>

namespace rx
{
    bool IRGenerator::run(const Crate& crate, const SemanticAnalyzer& semantic, std::ostream& output) {
        semantic_ = &semantic;
        output_ = &output;
        failed_ = false;
        locals_.clear();
        loops_.clear();
        functions_.clear();
        self_ = nullptr;

        if (!semantic.ok()) {
            error("不能为 semantic 检查失败的程序生成 IR");
            return false;
        }

        for (const auto& item : crate.items) {
            if (dynamic_cast<const UseItem*>(item.get())) {
                continue;
            }

            const auto* function = dynamic_cast<const FunctionItem*>(item.get());
            if (!function) {
                error("当前只支持顶层函数和 use");
                return false;
            }

            functions_[function->name] = function;
        }

        auto mainEntry = functions_.find("main");
        if (mainEntry == functions_.end()) {
            error("缺少 main 函数");
            return false;
        }

        const FunctionItem& mainFunction = *mainEntry->second;
        if (!mainFunction.parameters.empty()) {
            error("main 不能有参数");
            return false;
        }

        if (mainFunction.resultType) {
            const SemType* type = semantic.resolvedType(*mainFunction.resultType);
            if (!type || type->kind != TypeKind::Unit) {
                error("main 必须返回 unit");
                return false;
            }
        }

        emitRuntime();

        for (const auto& item : crate.items) {
            const auto* function = dynamic_cast<const FunctionItem*>(item.get());
            if (!function) {
                continue;
            }

            emitFunction(*function);

            if (failed_) {
                return false;
            }
        }

        builder_.beginFunction("define i32 @main()");
        builder_.emit("call void @rx.main()");
        builder_.endBlock("ret i32 0");
        builder_.finishFunction(output);
        return true;
    }

    void IRGenerator::error(const std::string& message) {
        std::cerr << "IR error: " << message << "\n";
        failed_ = true;
    }

    static bool isDayOneType(const SemType& type) {
        return isInteger(type) || type.kind == TypeKind::Bool || type.kind == TypeKind::Unit;
    }

    void IRGenerator::emitRuntime() {
        *output_ << R"(
@rx.fmt.read = private constant [3 x i8] c"%d\00"
@rx.fmt.print = private constant [3 x i8] c"%d\00"
@rx.fmt.println = private constant [4 x i8] c"%d\0A\00"

declare i32 @scanf(ptr, ...)
declare i32 @printf(ptr, ...)

define i32 @rx.get_i32() {
entry:
  %slot = alloca i32
  store i32 0, ptr %slot
  call i32 (ptr, ...) @scanf(ptr @rx.fmt.read, ptr %slot)
  %value = load i32, ptr %slot
  ret i32 %value
}

define void @rx.print_i32(i32 %value) {
entry:
  call i32 (ptr, ...) @printf(ptr @rx.fmt.print, i32 %value)
  ret void
}

define void @rx.println_i32(i32 %value) {
entry:
  call i32 (ptr, ...) @printf(ptr @rx.fmt.println, i32 %value)
  ret void
}

)";
    }

    IRValue IRGenerator::load(const IRPlace& place) {
        assert(place.type.kind != TypeKind::Never);
        assert(!place.address.empty());
        if (place.type.kind == TypeKind::Unit) return {place.type, "zeroinitializer"};

        std::string type = llvmType(place.type);
        assert(!type.empty());
        std::string result = builder_.newTemp();
        builder_.emit(result + " = load " + type + ", ptr " + place.address);
        return {place.type, result};
    }

    void IRGenerator::store(const IRPlace& place, const IRValue& value) {
        assert(sameType(place.type, value.type));
        assert(!place.address.empty());
        if (place.type.kind == TypeKind::Unit) return;

        std::string type = llvmType(place.type);
        assert(!type.empty());
        assert(!value.operand.empty());
        builder_.emit("store " + type + " " + value.operand +", ptr " + place.address);
    }

    IRPlace IRGenerator::emitPlace(const Expr& expr) {
        if (const auto* path = dynamic_cast<const PathExpr*>(&expr)) {
            if (path->path.size() == 1) {
                const std::string& name = path->path[0].name;
                for (std::size_t i = locals_.size(); i > 0; --i) {
                    auto found = locals_[i - 1].find(name);
                    if (found != locals_[i - 1].end()) {
                        return found->second;
                    }
                }
            }
        }
        error("暂不支持这个表达式的存储位置");
        return {neverType(), ""};
    }

    IRValue IRGenerator::emitExpr(const Expr& expr, const SemType* expected) {
        (void)expected;

        if (failed_ || builder_.isTerminated()) return {neverType(), ""};

        const SemType* type = semantic_->expressionType(expr);
        if (!type) {
            error("表达式缺少 semantic 类型记录");
            return {neverType(), ""};
        }

        if (const auto* integer = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
            unsigned long long value = 0;
            for (char c : integer->text) {
                if (c == '_') continue;
                if (c < '0' || c > '9') break;
                value = value * 10 + (c - '0');
            }

            return {*type, std::to_string(value)};
        }

        if (const auto* boolean = dynamic_cast<const BooleanLiteralExpr*>(&expr)) {
            if (boolean->value) return {*type, "1"};
            return {*type, "0"};
        }

        if (dynamic_cast<const UnitExpr*>(&expr)) return {unitType(), "zeroinitializer"};

        if (const auto* block = dynamic_cast<const Block*>(&expr)) return emitBlock(*block);

        if (dynamic_cast<const PathExpr*>(&expr)) {
            IRPlace place = emitPlace(expr);
            if (failed_) return {neverType(), ""};
            return load(place);
        }
        if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expr)) {
            if (binary->op == "&&" || binary->op == "||") {
                error("短路逻辑需要控制流，下一阶段实现");
                return {neverType(), ""};
            }

            IRValue left = emitExpr(*binary->lhs);
            if (failed_ || builder_.isTerminated()) {
                return {neverType(), ""};
            }

            IRValue right = emitExpr(*binary->rhs);
            if (failed_ || builder_.isTerminated()) {
                return {neverType(), ""};
            }

            return emitBinary(binary->op, left, right);
        }
        if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
            if (unary->op != "-" && unary->op != "!") {
                error("暂不支持一元运算符: " + unary->op);
                return {neverType(), ""};
            }

            IRValue operand = emitExpr(*unary->operand);
            if (failed_ || builder_.isTerminated()) {
                return {neverType(), ""};
            }

            bool integer = isInteger(operand.type);
            bool boolean = operand.type.kind == TypeKind::Bool;
            if (!integer && !boolean) {
                error("暂不支持这些类型的一元运算");
                return {neverType(), ""};
            }

            std::string result = builder_.newTemp();
            std::string typeText = llvmType(operand.type);
            if (unary->op == "-") {
                builder_.emit(result + " = sub " + typeText + " 0, " + operand.operand);
            }
            else {
                std::string mask;
                if (boolean) mask = "1";
                else mask = "-1";
                builder_.emit(result + " = xor " + typeText + " " + operand.operand + ", " + mask);
            }

            return {operand.type, result};
        }
        if (const auto* assignment = dynamic_cast<const AssignmentExpr*>(&expr)) {
            if (assignment->op == "=") {
                IRValue value = emitExpr(*assignment->rhs);
                if (failed_ || builder_.isTerminated()) {
                    return {neverType(), ""};
                }

                IRPlace place = emitPlace(*assignment->lhs);
                if (failed_ || builder_.isTerminated()) {
                    return {neverType(), ""};
                }

                if (!sameType(place.type, value.type)) {
                    error("暂不支持这个赋值中的类型转换");
                    return {neverType(), ""};
                }

                store(place, value);
            }
            else {
                IRPlace place = emitPlace(*assignment->lhs);
                if (failed_ || builder_.isTerminated()) {
                    return {neverType(), ""};
                }

                IRValue left = load(place);
                IRValue right = emitExpr(*assignment->rhs);
                if (failed_ || builder_.isTerminated()) {
                    return {neverType(), ""};
                }

                std::string op = assignment->op;
                op.pop_back();

                IRValue result = emitBinary(op, left, right);
                if (failed_) {
                    return {neverType(), ""};
                }

                store(place, result);
            }

            return {unitType(), "zeroinitializer"};
        }
        if (const auto* returned = dynamic_cast<const ReturnExpr*>(&expr)) {
            IRValue value{unitType(), "zeroinitializer"};

            if (returned->value) {
                value = emitExpr(*returned->value, &returnType_);

                if (failed_ || builder_.isTerminated()) {
                    return {neverType(), ""};
                }
            }

            if (!sameType(returnType_, value.type)) {
                error("暂不支持这个 return 中的类型转换");
                return {neverType(), ""};
            }

            if (returnType_.kind == TypeKind::Unit) {
                builder_.endBlock("ret void");
            }
            else {
                builder_.endBlock("ret " + llvmType(returnType_) + " " + value.operand);
            }

            return {neverType(), ""};
        }
        if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
            return emitCall(*call);
        }
        if (const auto* cast = dynamic_cast<const CastExpr*>(&expr)) {
            IRValue value = emitExpr(*cast->value);
            if (failed_ || builder_.isTerminated()) {
                return {neverType(), ""};
            }

            const SemType* target = semantic_->resolvedType(*cast->type);
            if (!target || !isInteger(*target)) {
                error("暂不支持这个转换目标类型");
                return {neverType(), ""};
            }

            if (isInteger(value.type)) {
                return {*target, value.operand};
            }

            if (value.type.kind == TypeKind::Bool) {
                std::string result = builder_.newTemp();
                builder_.emit(result + " = zext i8 " + value.operand + " to i32");
                return {*target, result};
            }

            error("暂不支持这个类型转换");
            return {neverType(), ""};
        }
        error("暂不支持这个表达式的 IR 生成");
        return {neverType(), ""};
    }

    void IRGenerator::emitStmt(const Stmt& statement) {
        if (failed_ || builder_.isTerminated()) return;

        if (const auto* let = dynamic_cast<const LetStmt*>(&statement)) {
            if (!let->initializer) {
                error("暂不支持没有初始化值的 let");
                return;
            }

            const SemType* declared = nullptr;
            if (let->type) {
                declared = semantic_->resolvedType(*let->type);
                if (!declared) {
                    error("let 类型标注缺少 semantic 记录");
                    return;
                }
            }

            IRValue value = emitExpr(*let->initializer, declared);
            if (failed_ || builder_.isTerminated()) return;

            SemType type;
            if (declared) type = *declared;
            else type = value.type;
            if (!sameType(type, value.type)) {
                error("暂不支持这个 let 初始化中的类型转换");
                return;
            }

            std::string address = builder_.newTemp();
            std::string typeText = llvmType(type);
            if (typeText.empty()) {
                error("无法生成局部变量类型");
                return;
            }

            builder_.emitAlloca(address + " = alloca " + typeText);

            IRPlace place{type, address};
            store(place, value);
            locals_.back()[let->name] = place;
            return;
        }

        if (const auto* expression = dynamic_cast<const ExprStmt*>(&statement)) {
            emitExpr(*expression->expression);
            return;
        }
        if (dynamic_cast<const EmptyStmt*>(&statement)) return;
        error("暂不支持这个语句的 IR 生成");
    }

    IRValue IRGenerator::emitBlock(const Block& block) {
        locals_.emplace_back();

        IRValue result{unitType(), "zeroinitializer"};

        for (const auto& statement : block.statements) {
            if (failed_ || builder_.isTerminated()) {
                break;
            }
            emitStmt(*statement);
        }

        if (!failed_ && !builder_.isTerminated() && block.tail) {
            result = emitExpr(*block.tail);
        }

        if (builder_.isTerminated()) {
            result = {neverType(), ""};
        }

        locals_.pop_back();
        return result;
    }

    IRValue IRGenerator::emitBinary(const std::string& op, const IRValue& left, const IRValue& right) {
        bool integer = isInteger(left.type);
        bool boolean = left.type.kind == TypeKind::Bool;
        bool shift = op == "<<" || op == ">>";
        if ((!integer && !boolean) || (!shift && !sameType(left.type, right.type)) || (shift && (!integer || !isInteger(right.type)))) {
            error("暂不支持这些类型的二元运算");
            return {neverType(), ""};
        }

        bool signedInteger = left.type.kind == TypeKind::Int || left.type.kind == TypeKind::Isize;
        std::string instruction;
        std::string predicate;
        if (op == "==") predicate = "eq";
        else if (op == "!=") predicate = "ne";
        else if (op == "<") {
            predicate = "ult";
            if (signedInteger) predicate = "slt";
        }
        else if (op == "<=") {
            predicate = "ule";
            if (signedInteger) predicate = "sle";
        }
        else if (op == ">") {
            predicate = "ugt";
            if (signedInteger) predicate = "sgt";
        }
        else if (op == ">=") {
            predicate = "uge";
            if (signedInteger) predicate = "sge";
        }

        std::string type = llvmType(left.type);
        if (!predicate.empty()) {
            std::string compared = builder_.newTemp();
            builder_.emit(compared + " = icmp " + predicate + " " + type + " " + left.operand + ", " + right.operand);

            std::string result = builder_.newTemp();
            builder_.emit(result + " = zext i1 " + compared + " to i8");
            return {boolType(), result};
        }

        if (op == "&") instruction = "and";
        else if (op == "|") instruction = "or";
        else if (op == "^") instruction = "xor";
        else if (integer) {
            if (op == "+") instruction = "add";
            else if (op == "-") instruction = "sub";
            else if (op == "*") instruction = "mul";
            else if (op == "/") {
                if (signedInteger) instruction = "sdiv";
                else instruction = "udiv";
            }
            else if (op == "%") {
                if (signedInteger) instruction = "srem";
                else instruction = "urem";
            }
            else if (op == "<<") instruction = "shl";
            else if (op == ">>") {
                if (signedInteger) instruction = "ashr";
                else instruction = "lshr";
            }
        }

        if (instruction.empty()) {
            error("暂不支持二元运算符: " + op);
            return {neverType(), ""};
        }

        std::string rightOperand = right.operand;
        if (shift) {
            rightOperand = builder_.newTemp();
            builder_.emit(rightOperand + " = and i32 " + right.operand + ", 31");
        }

        std::string result = builder_.newTemp();
        builder_.emit(result + " = " + instruction + " " + type + " " + left.operand + ", " + rightOperand);
        return {left.type, result};
    }

    void IRGenerator::emitFunction(const FunctionItem& function, const StructItem* owner) {
        if (owner) {
            error("暂不支持关联函数和方法");
            return;
        }

        if (!function.body) {
            error("函数缺少函数体");
            return;
        }

        locals_.clear();
        locals_.emplace_back();
        loops_.clear();
        self_ = owner;
        returnType_ = unitType();

        if (function.resultType) {
            const SemType* type = semantic_->resolvedType(*function.resultType);
            if (!type) {
                error("函数返回类型缺少 semantic 记录");
                return;
            }

            returnType_ = *type;
        }

        if (!isDayOneType(returnType_)) {
            error("暂不支持这个函数返回类型");
            return;
        }

        std::vector<SemType> parameterTypes;
        std::string resultType = "void";
        if (returnType_.kind != TypeKind::Unit) resultType = llvmType(returnType_);
        std::string header = "define " + resultType + " @rx." + function.name + "(";
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            const auto& parameter = function.parameters[i];
            if (parameter.isSelf || !parameter.type) {
                error("暂不支持 self 参数");
                return;
            }

            const SemType* type = semantic_->resolvedType(*parameter.type);
            if (!type || !isDayOneType(*type)) {
                error("暂不支持这个函数参数类型");
                return;
            }

            parameterTypes.push_back(*type);

            if (i != 0) {
                header += ", ";
            }

            header += llvmType(*type) + " %arg" + std::to_string(i);
        }

        header += ")";
        builder_.beginFunction(header);

        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            const auto& parameter = function.parameters[i];
            const SemType& type = parameterTypes[i];
            std::string address = builder_.newTemp();
            builder_.emitAlloca(address + " = alloca " + llvmType(type));

            IRPlace place{type, address};
            IRValue value{
                type, "%arg" + std::to_string(i)
            };
            store(place, value);
            locals_.back()[parameter.name] = place;
        }

        IRValue result = emitBlock(*function.body);
        if (failed_) {
            return;
        }

        if (!builder_.isTerminated()) {
            if (!sameType(returnType_, result.type)) {
                error("暂不支持函数尾表达式中的类型转换");
                return;
            }

            if (returnType_.kind == TypeKind::Unit) {
                builder_.endBlock("ret void");
            }
            else {
                builder_.endBlock("ret " + llvmType(returnType_) + " " + result.operand);
            }
        }

        builder_.finishFunction(*output_);
        locals_.clear();
    }

    IRValue IRGenerator::emitCall(const CallExpr& call) {
        const auto* path = dynamic_cast<const PathExpr*>(call.callee.get());
        if (!path || path->path.size() != 1) {
            error("暂不支持这个函数调用路径");
            return {neverType(), ""};
        }

        const std::string& name = path->path[0].name;
        std::vector<SemType> parameterTypes;
        SemType resultType = unitType();
        if (name == "get_i32") {
            resultType = intType();
        }
        else if (name == "print_i32" || name == "println_i32") {
            parameterTypes.push_back(intType());
        }
        else {
            auto found = functions_.find(name);
            if (found == functions_.end()) {
                error("暂不支持这个函数调用: " + name);
                return {neverType(), ""};
            }

            const FunctionItem& function = *found->second;
            for (const auto& parameter : function.parameters) {
                if (parameter.isSelf || !parameter.type) {
                    error("暂不支持这个调用参数");
                    return {neverType(), ""};
                }

                const SemType* type = semantic_->resolvedType(*parameter.type);
                if (!type || !isDayOneType(*type)) {
                    error("暂不支持这个调用参数类型");
                    return {neverType(), ""};
                }

                parameterTypes.push_back(*type);
            }

            if (function.resultType) {
                const SemType* type = semantic_->resolvedType(*function.resultType);
                if (!type || !isDayOneType(*type)) {
                    error("暂不支持这个调用返回类型");
                    return {neverType(), ""};
                }

                resultType = *type;
            }
        }

        if (call.args.size() != parameterTypes.size()) {
            error("调用参数数量不匹配");
            return {neverType(), ""};
        }

        std::string arguments;
        for (std::size_t i = 0; i < call.args.size(); ++i) {
            IRValue value = emitExpr(*call.args[i], &parameterTypes[i]);
            if (failed_ || builder_.isTerminated()) {
                return {neverType(), ""};
            }

            if (!sameType(parameterTypes[i], value.type)) {
                error("暂不支持调用参数中的类型转换");
                return {neverType(), ""};
            }

            if (i != 0) {
                arguments += ", ";
            }

            arguments += llvmType(parameterTypes[i]) + " " + value.operand;
        }

        std::string resultTypeText = "void";
        if (resultType.kind != TypeKind::Unit) resultTypeText = llvmType(resultType);
        std::string instruction = "call " + resultTypeText + " @rx." + name + "(" + arguments + ")";
        if (resultType.kind == TypeKind::Unit) {
            builder_.emit(instruction);
            return {unitType(), "zeroinitializer"};
        }

        std::string result = builder_.newTemp();
        builder_.emit(result + " = " + instruction);
        return {resultType, result};
    }
}
