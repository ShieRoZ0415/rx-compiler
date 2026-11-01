#include "analyzer.h"

#include <iostream>
#include <unordered_set>

namespace rx
{
    static void registerBuiltin(Scope& scope, const std::string& name, std::vector<SemType> params, SemType ret) {
        Symbol sym;
        sym.kind = Symbol::Kind::Func;
        sym.paramTypes = std::move(params);
        sym.returnType = std::move(ret);
        scope.insert(name, std::move(sym));
    }

    bool SemanticAnalyzer::run(Crate& crate) {
        errorCount_ = 0;
        scopes_.clear();
        scopes_.emplace_back();
        selfType_ = neverType();
        loops_.clear();
        methods_.clear();
        layoutState_.clear();

        structTypes_.clear();

        exprInfo_.clear();

        for (const auto& item : crate.items) {
            const auto* structure = dynamic_cast<const StructItem*>(item.get());
            if (!structure) continue;
            const std::string& name = structure->name;
            if (name == "i32" || name == "u32" || name == "isize" || name == "usize" ||
                name == "bool" || name == "Box" || name == "Vec" || name == "Self") {
                error("不能重新定义内建类型名: " + name);
                continue;
            }
            if (structTypes_.find(name) != structTypes_.end()) {
                error("重复定义结构体: " + name);
                continue;
            }
            Symbol symbol;
            symbol.kind = Symbol::Kind::Struct;
            symbol.type.kind = TypeKind::Struct;
            symbol.type.structDef = structure;
            structTypes_.emplace(name, std::move(symbol));
        }

        for (const auto& item : crate.items) {
            const auto* constant = dynamic_cast<const ConstItem*>(item.get());
            if (!constant) continue;
            ConstantInfo info;
            info.definition = constant;
            if (!constants_.emplace(constant->name, std::move(info)).second) {
                error("重复定义常量: " + constant->name);
            }
        }

        for (const auto& item : crate.items) {
            const auto* implementation = dynamic_cast<const ImplItem*>(item.get());
            if (!implementation) continue;

            const auto* target = dynamic_cast<const PathType*>(implementation->selfType.get());
            if (!target || target->path.size() != 1) continue;

            auto structure = structTypes_.find(target->path.front().name);
            if (structure == structTypes_.end()) continue;

            const StructItem* owner = structure->second.type.structDef;
            for (const auto& item : implementation->items) {
                const auto* constant = dynamic_cast<const ConstItem*>(item.get());
                if (!constant) continue;
                ConstantInfo info;
                info.definition = constant;
                info.owner = owner;
                std::string key = owner->name + "::" + constant->name;
                if (!constants_.emplace(key, std::move(info)).second) {
                    error("重复定义关联常量: " + key);
                }
            }
        }
        for (auto& entry : constants_) {
            evaluateConstant(entry.second);
        }
        for (const auto& item : crate.items) {
            const auto* structure = dynamic_cast<const StructItem*>(item.get());
            if (!structure) continue;
            auto found = structTypes_.find(structure->name);
            if (found == structTypes_.end()) continue;

            Symbol& symbol = found->second;
            if (symbol.type.structDef != structure) continue;

            SemType savedSelf = selfType_;
            selfType_ = symbol.type;
            for (const auto& field : structure->fields) {
                symbol.fields.emplace_back(field.name, resolveType(*field.type));
            }
            selfType_ = savedSelf;
        }
        std::unordered_set<std::string> globalValues;
        for (const auto& item : crate.items) {
            if (const auto* f = dynamic_cast<FunctionItem*>(item.get())) {
                if (!globalValues.insert(f->name).second) {
                    error("重复定义顶层值: " + f->name);
                    continue;
                }
                Symbol sym;
                sym.kind = Symbol::Kind::Func;
                for (const auto& p : f->parameters) {
                    if (p.isSelf) {
                        error("顶层函数不能有 self 参数");
                        continue;
                    }
                    sym.paramTypes.push_back(resolveType(*p.type));
                }
                sym.returnType = f->resultType ? resolveType(*f->resultType) : unitType();
                currentScope().insert(f->name, std::move(sym));
            }
        }

        for (const auto& item : crate.items) {
            if (const auto* c = dynamic_cast<ConstItem*>(item.get())) {
                if (!globalValues.insert(c->name).second) {
                    error("重复定义顶层值: " + c->name);
                    continue;
                }
                Symbol sym;
                sym.kind = Symbol::Kind::Const;
                auto constant = constants_.find(c->name);
                sym.type = constant == constants_.end() ? neverType() : constant->second.type;
                currentScope().insert(c->name, std::move(sym));
            }
        }

        std::unordered_map<const StructItem*, std::unordered_set<std::string>> associatedNames;
        for (const auto& item : crate.items) {
            const auto* implementation =
                dynamic_cast<const ImplItem*>(item.get());

            if (!implementation) {
                continue;
            }

            SemType target = resolveType(*implementation->selfType);

            if (target.kind != TypeKind::Struct || !target.structDef) {
                continue;
            }

            auto& names = associatedNames[target.structDef];
            auto& methods = methods_[target.structDef->name];

            for (const auto& item : implementation->items) {
                const auto* function = dynamic_cast<const FunctionItem*>(item.get());
                const auto* constant = dynamic_cast<const ConstItem*>(item.get());

                std::string name;
                if (function) name = function->name;
                else if (constant) name = constant->name;
                else continue;

                if (!names.insert(name).second) {
                    error("重复定义关联项: " + target.structDef->name + "::" + name);
                    continue;
                }
                if (function) methods.push_back(function);
            }
        }

        registerBuiltin(currentScope(), "get_i32", {}, intType());
        registerBuiltin(currentScope(), "get_u32", {}, uintType());
        registerBuiltin(currentScope(), "get_isize", {}, isizeType());
        registerBuiltin(currentScope(), "get_usize", {}, usizeType());
        registerBuiltin(currentScope(), "get_bool", {}, boolType());
        registerBuiltin(currentScope(), "println_i32", {intType()}, unitType());
        registerBuiltin(currentScope(), "println_u32", {uintType()}, unitType());
        registerBuiltin(currentScope(), "println_isize", {isizeType()}, unitType());
        registerBuiltin(currentScope(), "println_usize", {usizeType()}, unitType());
        registerBuiltin(currentScope(), "println_bool", {boolType()}, unitType());
        registerBuiltin(currentScope(), "print_i32", {intType()}, unitType());
        registerBuiltin(currentScope(), "print_u32", {uintType()}, unitType());
        registerBuiltin(currentScope(), "print_isize", {isizeType()}, unitType());
        registerBuiltin(currentScope(), "print_usize", {usizeType()}, unitType());
        registerBuiltin(currentScope(), "print_bool", {boolType()}, unitType());
        const FunctionItem* mainFunction = nullptr;

        for (const auto& item : crate.items) {
            const auto* function =
                dynamic_cast<const FunctionItem*>(item.get());

            if (!function || function->name != "main") continue;

            if (mainFunction) error("重复定义入口函数 main");
            else mainFunction = function;
        }

        if (!mainFunction) error("缺少顶层入口函数 main");
        else {
            if (!mainFunction->parameters.empty()) {
                error("main 不能有参数");
            }
            if (!mainFunction->genericParams.empty()) {
                error("main 不能有泛型参数");
            }
            SemType result = unitType();
            if (mainFunction->resultType) result = resolveType(*mainFunction->resultType);
            if (result.kind != TypeKind::Unit) {
                error("main 必须返回 ()");
            }
        }
        crate.accept(*this);
        return errorCount_ == 0;
    }

    void SemanticAnalyzer::error(const std::string& message) {
        std::cerr << "semantic error: " << message << "\n";
        errorCount_++;
    }

    SemType SemanticAnalyzer::resolveType(const Type& type) {
        if (dynamic_cast<const UnitType*>(&type)) return unitType();
        if (const auto* path = dynamic_cast<const PathType*>(&type)) return typeFromSegment(path->path.back());
        if (const auto* ref = dynamic_cast<const ReferenceType*>(&type)) {
            SemType t;
            t.kind = TypeKind::Ref;
            t.isMutable = ref->isMutable;
            t.inner = std::make_shared<SemType>(resolveType(*ref->inner));
            return t;
        }
        if (const auto* arr = dynamic_cast<const ArrayType*>(&type)) {
            SemType t;
            t.kind = TypeKind::Array;
            t.inner = std::make_shared<SemType>(resolveType(*arr->element));

            t.arrayLength = readArrayLength(*arr->length);
            return t;
        }
        error("未知类型");
        return neverType();
    }

    SemType SemanticAnalyzer::typeFromSegment(const PathSegment& segment) {
        const std::string& name = segment.name;
        const Type* typeArg = nullptr;
        std::size_t typeArgCnt = 0;
        for (const auto& argument : segment.args) {
            if (argument.isLifetime) continue;
            if (!argument.type) {
                error("无效的类型实参: " + name);
                return neverType();
            }
            ++typeArgCnt;
            typeArg = argument.type.get();
        }
        if (name == "Box" || name == "Vec") {
            if (typeArgCnt != 1) {
                error("类型 " + name + " 必须恰好有一个类型实参");
                return neverType();
            }
            SemType t;
            t.kind = name == "Box" ? TypeKind::Box : TypeKind::Vec;
            t.inner = std::make_shared<SemType>(
                resolveType(*typeArg)
            );
            return t;
        }
        if (typeArgCnt != 0) {
            error("类型 " + name + " 不接受类型实参");
            return neverType();
        }
        if (name == "i32") return intType();
        if (name == "u32") return uintType();
        if (name == "isize") return isizeType();
        if (name == "usize") return usizeType();
        if (name == "bool") return boolType();
        if (name == "Self") {
            if (selfType_.kind == TypeKind::Struct) return selfType_;
            error("Self 只能用在 impl 块内");
            return neverType();
        }

        auto structure = structTypes_.find(name);
        if (structure != structTypes_.end()) {
            return structure->second.type;
        }
        error("未知类型: " + name);
        return neverType();
    }

    SemType SemanticAnalyzer::resolveStruct(const Path& path) {
        if (path.empty()) {
            error("结构体路径不能为空");
            return neverType();
        }
        SemType type = typeFromSegment(path.back());
        if (type.kind == TypeKind::Struct && type.structDef) {
            return type;
        }
        error("该路径不是结构体类型");
        return neverType();
    }

    const FunctionItem* SemanticAnalyzer::findMethod(const SemType& receiver, const std::string& name) {
        const SemType* t = &receiver;
        while (t->kind == TypeKind::Ref) t = t->inner.get();
        if (t->kind != TypeKind::Struct || !t->structDef) return nullptr;
        auto it = methods_.find(t->structDef->name);
        if (it == methods_.end()) return nullptr;
        for (const auto* fn : it->second) {
            if (fn->name == name) return fn;
        }
        return nullptr;
    }

    SemType SemanticAnalyzer::findField(const SemType& receiver, const std::string& name, bool* found) {
        *found = false;
        const SemType* type = &receiver;
        while ((type->kind == TypeKind::Ref || type->kind == TypeKind::Box) && type->inner) {
            type = type->inner.get();
        }
        if (type->kind != TypeKind::Struct || !type->structDef) {
            return neverType();
        }

        auto structure = structTypes_.find(type->structDef->name);
        if (structure == structTypes_.end()) return neverType();

        for (const auto& field : structure->second.fields) {
            if (field.first == name) {
                *found = true;
                return field.second;
            }
        }
        return neverType();
    }


    bool SemanticAnalyzer::checkArgs(const FunctionItem& fn, const std::vector<SemType>& args) {
        bool hasSelf = !fn.parameters.empty() && fn.parameters[0].isSelf;
        size_t start = hasSelf ? 1 : 0;
        size_t expected = fn.parameters.size() - start;

        if (args.size() != expected) {
            error("函数 " + fn.name + " 参数数量不匹配，期望 " + std::to_string(expected)
                + " 个，得到 " + std::to_string(args.size()) + " 个");
            return false;
        }
        for (size_t i = 0; i < args.size(); ++i) {
            const auto& p = fn.parameters[start + i];
            SemType want = resolveType(*p.type);
            if (!sameType(args[i], want)) {
                error("函数 " + fn.name + " 第 " + std::to_string(i + 1) + " 个参数类型不匹配");
                return false;
            }
        }
        return true;
    }

    bool SemanticAnalyzer::readInteger(const std::string& text, long long& value, SemType& type, bool& hasSuffix) {
        std::string number = text;
        type = intType();
        hasSuffix = false;

        for (const std::string suffix :
             {"i32", "u32", "isize", "usize"}) {
            if (number.size() >= suffix.size() &&
                number.compare(number.size() - suffix.size(), suffix.size(), suffix) == 0) {
                hasSuffix = true;
                number.resize(number.size() - suffix.size());

                if (suffix == "u32") {
                    type = uintType();
                }
                else if (suffix == "isize") {
                    type = isizeType();
                }
                else if (suffix == "usize") {
                    type = usizeType();
                }
                break;
            }
        }

        std::string digits;

        for (char ch : number) {
            if (ch != '_') {
                digits.push_back(ch);
            }
        }

        int base = 10;
        std::size_t start = 0;

        if (digits.size() >= 2 && digits[0] == '0') {
            if (digits[1] == 'x') {
                base = 16;
                start = 2;
            }
            else if (digits[1] == 'o') {
                base = 8;
                start = 2;
            }
            else if (digits[1] == 'b') {
                base = 2;
                start = 2;
            }
        }

        if (start == digits.size()) {
            error("整数字面量缺少数字");
            return false;
        }

        value = 0;
        const long long maximum = 4294967295LL;

        for (std::size_t i = start; i < digits.size(); ++i) {
            char ch = digits[i];
            int digit = -1;

            if (ch >= '0' && ch <= '9') {
                digit = ch - '0';
            }
            else if (ch >= 'a' && ch <= 'f') {
                digit = ch - 'a' + 10;
            }
            else if (ch >= 'A' && ch <= 'F') {
                digit = ch - 'A' + 10;
            }

            if (digit < 0 || digit >= base) {
                error("整数字面量含有非法数字");
                return false;
            }

            if (value > (maximum - digit) / base) {
                error("整数字面量数值过大");
                return false;
            }

            value = value * base + digit;
        }

        return true;
    }

    SemanticAnalyzer::ConstantInfo* SemanticAnalyzer::findConstant(const Path& path) {
        if (path.empty() || path.size() > 2) {
            error("无效的常量路径");
            return nullptr;
        }
        for (const auto& segment : path) {
            for (const auto& argument : segment.args) {
                if (!argument.isLifetime) {
                    error("常量路径不能携带类型实参");
                    return nullptr;
                }
            }
        }
        std::string key;
        if (path.size() == 1) {
            key = path.front().name;
            if (!evaluatingConstant_) {
                Symbol* symbol = currentScope().lookup(key);
                if (symbol && symbol->kind != Symbol::Kind::Const) {
                    error("该名字不是常量: " + key);
                    return nullptr;
                }
            }
        }
        else {
            std::string ownerName = path.front().name;
            if (ownerName == "Self") {
                if (selfType_.kind != TypeKind::Struct || !selfType_.structDef) {
                    error("Self 缺少有效的结构体上下文");
                    return nullptr;
                }

                ownerName = selfType_.structDef->name;
            }
            else if (structTypes_.find(ownerName) == structTypes_.end()) {
                error("未知的结构体类型: " + ownerName);
                return nullptr;
            }
            key = ownerName + "::" + path.back().name;
        }

        auto found = constants_.find(key);
        if (found == constants_.end()) {
            error("未知常量: " + key);
            return nullptr;
        }
        return &found->second;
    }

    bool SemanticAnalyzer::evaluateConstantExpr(const Expr& expr, const SemType& expected, SemType& actual,
                                                long long& value) {
        if (const auto* literal = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
            bool hasSuffix = false;
            if (!readInteger(literal->text, value, actual, hasSuffix)) return false;
            if (!hasSuffix && isInteger(expected)) actual = expected;
            return true;
        }
        if (const auto* boolean = dynamic_cast<const BooleanLiteralExpr*>(&expr)) {
            actual = boolType();
            value = boolean->value ? 1 : 0;
            return true;
        }

        if (const auto* path = dynamic_cast<const PathExpr*>(&expr)) {
            ConstantInfo* constant = findConstant(path->path);
            if (!constant || !evaluateConstant(*constant)) return false;
            actual = constant->type;
            value = constant->value;
            return true;
        }

        if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expr)) {
            if (unary->op != "-") {
                error("不支持的常量一元运算");
                return false;
            }

            if (!evaluateConstantExpr(*unary->operand, expected, actual, value)) return false;

            if (actual.kind != TypeKind::Int && actual.kind != TypeKind::Isize) {
                error("常量负号只能用于有符号整数");
                return false;
            }
            value = -value;
            return true;
        }
        error("该表达式不能用于常量初始化");
        return false;
    }

    bool SemanticAnalyzer::evaluateConstant(ConstantInfo& constant) {
        if (constant.state == 2) return true;
        if (constant.state == 3) return false;

        if (constant.state == 1) {
            error(
                "常量存在循环依赖: " +
                constant.definition->name
            );
            return false;
        }

        constant.state = 1;
        SemType savedSelf = selfType_;
        bool savedEvaluation = evaluatingConstant_;
        evaluatingConstant_ = true;
        selfType_ = neverType();
        if (constant.owner) {
            selfType_.kind = TypeKind::Struct;
            selfType_.structDef = constant.owner;
        }
        constant.type = resolveType(*constant.definition->type);
        bool supported = isInteger(constant.type) || constant.type.kind == TypeKind::Bool;

        bool valid = supported;
        if (!supported) {
            error("当前常量初始化只支持整数和 bool");
        }
        SemType actual;
        long long value = 0;
        if (valid) valid = evaluateConstantExpr(*constant.definition->value, constant.type, actual, value);

        if (valid && !sameType(actual, constant.type)) {
            error("常量 " + constant.definition->name + " 需要 " + typeName(constant.type) + "，实际为 " + typeName(actual));
            valid = false;
        }

        if (valid) {
            if (constant.type.kind == TypeKind::Int || constant.type.kind == TypeKind::Isize) {
                if (value < -2147483648LL || value > 2147483647LL) {
                    error("有符号常量超出 32 位范围");
                    valid = false;
                }
            }
            else if (constant.type.kind == TypeKind::Uint || constant.type.kind == TypeKind::Usize) {
                if (value < 0 || value > 4294967295LL) {
                    error("无符号常量超出 32 位范围");
                    valid = false;
                }
            }
        }
        if (valid) constant.value = value;
        constant.state = valid ? 2 : 3;
        selfType_ = savedSelf;
        evaluatingConstant_ = savedEvaluation;
        return valid;
    }

    SemanticAnalyzer::ExprInfo SemanticAnalyzer::dereference(ExprInfo info) {
        if (info.type.kind == TypeKind::Never) {
            info.isPlace = false;
            info.canWrite = false;
            return info;
        }

        if ((info.type.kind != TypeKind::Ref && info.type.kind != TypeKind::Box) || !info.type.inner) {
            error("只能解引用引用或 Box");
            info.type = neverType();
            info.isPlace = false;
            info.canWrite = false;
            return info;
        }

        if (info.type.kind == TypeKind::Ref) {
            bool mutableReference = info.type.isMutable;
            if (!mutableReference) info.blocked = true;
            info.canWrite = mutableReference && !info.blocked;
        }
        else {
            info.canWrite = info.canWrite && !info.blocked;
        }

        SemType inner = *info.type.inner;
        info.type = inner;
        info.isPlace = true;
        return info;
    }

    SemType SemanticAnalyzer::checkArguments(const std::vector<SemType>& parameters,
                                             const std::vector<std::unique_ptr<Expr>>& arguments,
                                             const SemType& result) {
        if (parameters.size() != arguments.size()) {
            error("调用参数数量不匹配");
        }
        bool diverges = false;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            const SemType* expected = i < parameters.size() ? &parameters[i] : nullptr;
            SemType actual = checkExpr(*arguments[i], expected);
            if (actual.kind == TypeKind::Never) {
                diverges = true;
            }
            if (expected && !canCoerce(actual, *expected)) {
                error("第 " + std::to_string(i + 1) + " 个参数需要 " + typeName(*expected) + "，实际为 " + typeName(actual));
            }
        }
        currentType_ = diverges ? neverType() : result;
        return currentType_;
    }

    SemType SemanticAnalyzer::checkFunctionCall(const FunctionItem& function, const SemType& owner,
                                                const std::vector<std::unique_ptr<Expr>>& arguments,
                                                std::size_t firstParameter) {
        SemType savedSelf = selfType_;
        selfType_ = owner;
        std::vector<SemType> parameters;

        for (std::size_t i = firstParameter; i < function.parameters.size(); ++i) {
            const auto& parameter = function.parameters[i];
            SemType type;
            if (parameter.isSelf) {
                type = owner;
                if (parameter.isReference) {
                    SemType reference;
                    reference.kind = TypeKind::Ref;
                    reference.isMutable = parameter.isMutable;
                    reference.inner = std::make_shared<SemType>(owner);
                    type = reference;
                }
            }
            else type = resolveType(*parameter.type);
            parameters.push_back(type);
        }

        SemType result = function.resultType ? resolveType(*function.resultType) : unitType();
        selfType_ = savedSelf;

        return checkArguments(parameters, arguments, result);
    }

    bool SemanticAnalyzer::supportsTrait(const SemType& type, const std::string& trait) {
        std::unordered_set<const StructItem*> visiting;
        return supportsTrait(type, trait, visiting);
    }

    bool SemanticAnalyzer::supportsTrait(const SemType& type, const std::string& trait,
                                         std::unordered_set<const StructItem*>& visiting) {
        if (isInteger(type) || type.kind == TypeKind::Bool ||
            type.kind == TypeKind::Unit || type.kind == TypeKind::Never) {
            return true;
        }
        if (type.kind == TypeKind::Ref) {
            if (trait == "Copy" || trait == "Clone") {
                return !type.isMutable;
            }
            return type.inner && supportsTrait(*type.inner, trait, visiting);
        }
        if (type.kind == TypeKind::Array) {
            return type.inner && supportsTrait(*type.inner, trait, visiting);
        }
        if (type.kind == TypeKind::Box || type.kind == TypeKind::Vec) {
            if (trait == "Copy") {
                return false;
            }
            return type.inner && supportsTrait(*type.inner, trait, visiting);
        }
        if (type.kind != TypeKind::Struct || !type.structDef) {
            return false;
        }
        const StructItem* definition = type.structDef;
        bool requested = false;
        bool hasClone = false;
        bool hasPartialEq = false;
        for (const auto& attribute : definition->attributes) {
            if (attribute == trait) {
                requested = true;
            }
            if (attribute == "Clone") {
                hasClone = true;
            }
            if (attribute == "PartialEq") {
                hasPartialEq = true;
            }
        }
        if (!requested) {
            return false;
        }
        if (trait == "Copy" && !hasClone) {
            return false;
        }
        if (trait == "Eq" && !hasPartialEq) {
            return false;
        }
        if (visiting.find(definition) != visiting.end()) {
            return true;
        }
        auto structure = structTypes_.find(definition->name);
        if (structure == structTypes_.end()) {
            return false;
        }
        visiting.insert(definition);
        bool valid = true;
        for (const auto& field : structure->second.fields) {
            if (!supportsTrait(field.second, trait, visiting)) {
                valid = false;
            }
        }
        visiting.erase(definition);
        return valid;
    }

    void SemanticAnalyzer::visitCrate(const Crate& crate) {
        for (const auto& item : crate.items) item->accept(*this);
    }

    void SemanticAnalyzer::visitBlock(const Block& block) {
        currentType_ = checkExpr(block);
    }

    void SemanticAnalyzer::visitUnitType(const UnitType&) {}
    void SemanticAnalyzer::visitPathType(const PathType&) {}
    void SemanticAnalyzer::visitReferenceType(const ReferenceType&) {}
    void SemanticAnalyzer::visitArrayType(const ArrayType&) {}

    void SemanticAnalyzer::visitIntegerLiteralExpr(const IntegerLiteralExpr& expr) {
        const std::string& text = expr.text;
        if (text.size() >= 3 && text.compare(text.size() - 3, 3, "u32") == 0) currentType_ = uintType();
        else if (text.size() >= 5 && text.compare(text.size() - 5, 5, "isize") == 0) currentType_ = isizeType();
        else if (text.size() >= 5 && text.compare(text.size() - 5, 5, "usize") == 0) currentType_ = usizeType();
        else currentType_ = intType();
    }

    void SemanticAnalyzer::visitBooleanLiteralExpr(const BooleanLiteralExpr&) {
        currentType_ = boolType();
    }

    void SemanticAnalyzer::visitPathExpr(const PathExpr& expr) {
        if (expr.path.empty()) {
            error("值路径不能为空");
            currentType_ = neverType();
            return;
        }
        if (expr.path.size() == 1) {
            const std::string& name = expr.path.front().name;
            Symbol* symbol = currentScope().lookup(name);
            if (symbol && symbol->kind == Symbol::Kind::Var) {
                if (symbol->moved) error("使用了已移动的值: " + name);
                ExprInfo info;
                info.type = symbol->type;
                info.isPlace = true;
                info.canWrite = symbol->isMutable;
                exprInfo_[&expr] = info;

                currentType_ = symbol->type;
                return;
            }
        }
        ConstantInfo* constant = findConstant(expr.path);
        if (!constant || !evaluateConstant(*constant)) {
            currentType_ = neverType();
            return;
        }
        currentType_ = constant->type;
    }

    void SemanticAnalyzer::visitUnaryExpr(const UnaryExpr& expr) {
        SemType t = checkExpr(*expr.operand);
        ExprInfo operand = exprInfo_[expr.operand.get()];
        const std::string& op = expr.op;
        SemType scalar = t;

        if (t.kind == TypeKind::Ref && !t.isMutable && t.inner && (isInteger(*t.inner) || t.inner->kind ==
            TypeKind::Bool)) {
            scalar = *t.inner;
        }

        if (op == "-") {
            if (scalar.kind == TypeKind::Int || scalar.kind == TypeKind::Isize) {
                currentType_ = scalar;
            }
            else {
                error("一元负号只能用于有符号整数");
                currentType_ = neverType();
            }
        }
        else if (op == "!") {
            if (scalar.kind == TypeKind::Bool || isInteger(scalar)) {
                currentType_ = scalar;
            }
            else {
                error("! 只能用于 bool 或整数");
                currentType_ = neverType();
            }
        }
        else if (op == "*") {
            ExprInfo result = dereference(operand);
            exprInfo_[&expr] = result;
            currentType_ = result.type;
        }
        else if (op == "&" || op == "&mut" || op == "&&" || op == "&&mut") {
            if (t.kind == TypeKind::Never) {
                currentType_ = neverType();
                return;
            }

            bool mutableBorrow = (op == "&mut" || op == "&&mut");
            if (mutableBorrow && !operand.canWrite) {
                error("不能对该位置进行可变借用");
                currentType_ = neverType();
                return;
            }

            SemType reference;
            reference.kind = TypeKind::Ref;
            reference.isMutable = mutableBorrow;
            reference.inner = std::make_shared<SemType>(t);

            if (op == "&&" || op == "&&mut") {
                SemType outer;
                outer.kind = TypeKind::Ref;
                outer.isMutable = false;
                outer.inner = std::make_shared<SemType>(reference);
                currentType_ = outer;
            }
            else {
                currentType_ = reference;
            }
        }
        else {
            error("未知一元运算符: " + op);
            currentType_ = neverType();
        }
    }

    void SemanticAnalyzer::visitBinaryExpr(const BinaryExpr& expr) {
        SemType l = checkExpr(*expr.lhs);
        SemType r = checkExpr(*expr.rhs);
        const std::string& op = expr.op;

        if (op == "&&" || op == "||") {
            bool leftValid = l.kind == TypeKind::Bool || l.kind == TypeKind::Never;
            bool rightValid = r.kind == TypeKind::Bool || r.kind == TypeKind::Never;
            if (!leftValid || !rightValid) {
                error(op + " 两边必须是 bool");
                currentType_ = neverType();
                return;
            }
            currentType_ = l.kind == TypeKind::Never ? neverType() : boolType();
            return;
        }

        if (l.kind == TypeKind::Never || r.kind == TypeKind::Never) {
            currentType_ = neverType();
            return;
        }

        if (op == "==" || op == "!=") {
            if (!sameType(l, r)) {
                error(op + " 两边类型不一致");
                currentType_ = neverType();
            }
            else if (!supportsTrait(l, "PartialEq")) {
                error("该类型不支持相等比较: " + typeName(l));
                currentType_ = neverType();
            }
            else {
                currentType_ = boolType();
            }
            return;
        }

        if (op == "<" || op == "<=" || op == ">" || op == ">=") {
            bool compatible = sameType(l, r);
            const SemType* target = &l;
            if (l.kind == TypeKind::Ref & r.kind == TypeKind::Ref) {
                compatible = compatible || canCoerce(r, l);
                while (target->kind == TypeKind::Ref && target->inner) {
                    target = target->inner.get();
                }
            }

            bool supported = isInteger(*target) || target->kind == TypeKind::Bool;
            if (compatible && supported) {
                currentType_ = boolType();
            }
            else {
                error(op + " 不支持这些操作数类型");
                currentType_ = neverType();
            }
            return;
        }

        const SemType* left = &l;
        const SemType* right = &r;

        if (l.kind == TypeKind::Ref && !l.isMutable && l.inner && (isInteger(*l.inner) || l.inner->kind ==
            TypeKind::Bool)) {
            left = l.inner.get();
        }

        if (r.kind == TypeKind::Ref && !r.isMutable && r.inner && (isInteger(*r.inner) || r.inner->kind ==
            TypeKind::Bool)) {
            right = r.inner.get();
        }

        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
            if (isInteger(*left) && isInteger(*right) && sameType(*left, *right)) {
                currentType_ = *left;
            }
            else {
                error(op + " 两边必须是同类型整数");
                currentType_ = neverType();
            }
            return;
        }

        if (op == "&" || op == "|" || op == "^") {
            bool supported = isInteger(*left) || left->kind == TypeKind::Bool;
            if (supported && sameType(*left, *right)) {
                currentType_ = *left;
            }
            else {
                error(op + " 两边必须是同类型整数或 bool");
                currentType_ = neverType();
            }
            return;
        }

        if (op == "<<" || op == ">>") {
            if (isInteger(*left) && isInteger(*right)) {
                currentType_ = *left;
            }
            else {
                error(op + " 两边必须是整数");
                currentType_ = neverType();
            }
            return;
        }
        error("未知二元运算符: " + op);
        currentType_ = neverType();
    }

    void SemanticAnalyzer::visitAssignmentExpr(const AssignmentExpr& expr) {
        SemType l = checkExpr(*expr.lhs);
        ExprInfo left = exprInfo_[expr.lhs.get()];
        const SemType* expected = nullptr;
        if (expr.op == "=") expected = &l;
        SemType r = checkExpr(*expr.rhs, expected);

        if (l.kind != TypeKind::Never) {
            if (!left.isPlace) {
                error("赋值左侧必须是存储位置");
            }
            else if (!left.canWrite) {
                error("赋值目标不可写");
            }
        }
        else {
            const auto* field = dynamic_cast<const FieldAccessExpr*>(expr.lhs.get());
            const auto* index = dynamic_cast<const IndexExpr*>(expr.lhs.get());
            const auto* unary = dynamic_cast<const UnaryExpr*>(expr.lhs.get());
            bool isDereference = unary && unary->op == "*";
            if (!field && !index && !isDereference) error("赋值左侧必须是存储位置");
        }

        const std::string& op = expr.op;

        if (op == "=") {
            if (!canCoerce(l, r)) {
                error("赋值左右类型不一致");
            }
        }
        else {
            const SemType* right = &r;
            while (right->kind == TypeKind::Ref && !right->isMutable && right->inner) {
                right = right->inner.get();
            }
            bool valid = false;

            if (op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=") {
                valid = isInteger(l) && isInteger(*right) && sameType(l, *right);
            }
            else if (op == "&=" || op == "|=" || op == "^=") {
                bool supported = isInteger(l) || l.kind == TypeKind::Bool;
                valid = supported && sameType(l, *right);
            }
            else if (op == "<<=" || op == ">>=") {
                valid = isInteger(l) && isInteger(*right);
            }
            if (!valid) error("复合赋值 " + op + " 不支持 " + typeName(l) + " 和 " + typeName(r));
        }
        currentType_ = unitType();
    }

    void SemanticAnalyzer::visitCastExpr(const CastExpr& expr) {
        expr.value->accept(*this);
        SemType from = currentType_;
        SemType to = resolveType(*expr.type);
        if (isInteger(from) && isInteger(to)) currentType_ = to;
        else if (from.kind == TypeKind::Bool && isInteger(to)) currentType_ = to;
        else {
            error("不支持的 cast: " + typeName(from) + " as " + typeName(to));
            currentType_ = neverType();
        }
    }

    void SemanticAnalyzer::visitCallExpr(const CallExpr& expr) {
        const auto* callee = dynamic_cast<const PathExpr*>(expr.callee.get());
        if (!callee || callee->path.empty()) {
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }

            error("调用目标必须是函数路径");
            currentType_ = neverType();
            return;
        }

        const Path& path = callee->path;

        for (const auto& argument : path.back().args) {
            if (!argument.isLifetime) {
                error("函数名不能携带类型实参");
            }
        }

        if (path.size() == 1) {
            const std::string& name = path.front().name;
            Symbol* function = currentScope().lookup(name);

            if (!function ||
                function->kind != Symbol::Kind::Func) {
                for (const auto& argument : expr.args) {
                    checkExpr(*argument);
                }

                error("未定义的函数: " + name);
                currentType_ = neverType();
                return;
            }

            std::vector<SemType> parameters = function->paramTypes;
            SemType result = function->returnType;

            if (expr.args.size() != parameters.size()) {
                error("函数 " + name + " 参数数量不匹配");
            }

            bool diverges = false;

            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                const SemType* expected = i < parameters.size() ? &parameters[i] : nullptr;

                SemType actual = checkExpr(*expr.args[i], expected);
                if (actual.kind == TypeKind::Never) {
                    diverges = true;
                }

                if (expected && !canCoerce(actual, *expected)) {
                    error("函数 " + name + " 第 " + std::to_string(i + 1) +
                        " 个参数需要 " + typeName(*expected) + "，实际为 " + typeName(actual));
                }
            }

            currentType_ = diverges ? neverType() : result;
            return;
        }

        if (path.size() != 2) {
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }
            error("当前不支持这种关联函数路径");
            currentType_ = neverType();
            return;
        }

        SemType receiver = typeFromSegment(path.front());
        const std::string& name = path.back().name;

        if (name == "new" && (receiver.kind == TypeKind::Box || receiver.kind == TypeKind::Vec)) {
            std::size_t count = receiver.kind == TypeKind::Box ? 1 : 0;
            if (expr.args.size() != count) {
                error("容器构造函数参数数量不匹配");
            }

            bool diverges = false;
            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                const SemType* expected = nullptr;
                if (receiver.kind == TypeKind::Box && i == 0) {
                    expected = receiver.inner.get();
                }

                SemType actual = checkExpr(*expr.args[i], expected);
                if (actual.kind == TypeKind::Never) {
                    diverges = true;
                }
                if (expected && !canCoerce(actual, *expected)) {
                    error("Box 构造参数类型不匹配");
                }
            }
            currentType_ = diverges ? neverType() : receiver;
            return;
        }

        if (receiver.kind == TypeKind::Vec && (name == "len" || name == "is_empty" || name == "push" || name ==
            "remove")) {
            bool mutableReceiver = name == "push" || name == "remove";

            SemType reference;
            reference.kind = TypeKind::Ref;
            reference.isMutable = mutableReceiver;
            reference.inner = std::make_shared<SemType>(receiver);

            std::vector<SemType> parameters;
            parameters.push_back(reference);

            SemType result = unitType();

            if (name == "len") {
                result = usizeType();
            }
            else if (name == "is_empty") {
                result = boolType();
            }
            else {
                if (!receiver.inner) {
                    error("Vec 缺少元素类型");
                    currentType_ = neverType();
                    return;
                }

                if (name == "push") {
                    parameters.push_back(*receiver.inner);
                }
                else {
                    parameters.push_back(usizeType());
                    result = *receiver.inner;
                }
            }
            currentType_ = checkArguments(parameters, expr.args, result);
            return;
        }
        if (name == "clone") {
            const FunctionItem* inherent = nullptr;

            if (receiver.kind == TypeKind::Struct) {
                inherent = findMethod(receiver, name);
            }

            if (!inherent) {
                if (!supportsTrait(receiver, "Clone")) {
                    for (const auto& argument : expr.args) {
                        checkExpr(*argument);
                    }
                    error("该类型不支持 Clone: " + typeName(receiver));
                    currentType_ = neverType();
                    return;
                }

                SemType reference;
                reference.kind = TypeKind::Ref;
                reference.isMutable = false;
                reference.inner = std::make_shared<SemType>(receiver);
                std::vector<SemType> parameters;
                parameters.push_back(reference);
                currentType_ = checkArguments(parameters, expr.args, receiver);
                return;
            }
        }
        const FunctionItem* function = findMethod(receiver, name);
        if (!function) {
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }
            error("未找到关联函数: " + name);
            currentType_ = neverType();
            return;
        }

        currentType_ = checkFunctionCall(*function, receiver, expr.args, 0);
    }


    void SemanticAnalyzer::visitMethodCallExpr(const MethodCallExpr& expr) {
        checkExpr(*expr.receiver);
        ExprInfo receiver = exprInfo_[expr.receiver.get()];
        for (const auto& argument : expr.typeArgs) {
            if (!argument.isLifetime) {
                error("方法名不能携带类型实参");
            }
        }
        if (expr.method == "clone") {
            ExprInfo candidate = receiver;
            SemType referenceFallback = neverType();

            while (candidate.type.kind != TypeKind::Never) {
                if (candidate.type.kind == TypeKind::Ref && candidate.type.inner) {
                    if (!candidate.type.isMutable) {
                        referenceFallback = candidate.type;
                    }
                    candidate = dereference(candidate);
                    continue;
                }

                if (candidate.type.kind == TypeKind::Struct) {
                    const FunctionItem* function = findMethod(candidate.type, "clone");
                    if (function) {
                        if (function->parameters.empty() || !function->parameters.front().isSelf) {
                            for (const auto& argument : expr.args) {
                                checkExpr(*argument);
                            }

                            error("没有 self 参数的关联函数不能使用点调用");
                            currentType_ = neverType();
                            return;
                        }

                        const auto& self = function->parameters.front();
                        if (self.isReference && self.isMutable && !candidate.canWrite) {
                            error("该方法需要可变接收者");
                        }
                        currentType_ = checkFunctionCall(*function, candidate.type, expr.args, 1);
                        return;
                    }
                }

                if (supportsTrait(candidate.type, "Clone")) {
                    std::vector<SemType> parameters;
                    currentType_ = checkArguments(parameters, expr.args, candidate.type);
                    return;
                }

                if (candidate.type.kind == TypeKind::Box && candidate.type.inner) {
                    candidate = dereference(candidate);
                    continue;
                }
                break;
            }
            if (referenceFallback.kind == TypeKind::Ref) {
                std::vector<SemType> parameters;
                currentType_ = checkArguments(parameters, expr.args, referenceFallback);
                return;
            }
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }
            error("该接收者没有可用的 clone");
            currentType_ = neverType();
            return;
        }
        while ((receiver.type.kind == TypeKind::Ref || receiver.type.kind == TypeKind::Box) && receiver.type.inner) {
            receiver = dereference(receiver);
        }

        const SemType& target = receiver.type;
        if (target.kind == TypeKind::Never) {
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }
            currentType_ = neverType();
            return;
        }

        bool length = expr.method == "len" && (target.kind == TypeKind::Array || target.kind == TypeKind::Vec);
        bool empty = expr.method == "is_empty" && target.kind == TypeKind::Vec;
        bool push = expr.method == "push" && target.kind == TypeKind::Vec;
        bool remove = expr.method == "remove" && target.kind == TypeKind::Vec;

        if (length || empty || push || remove) {
            std::vector<SemType> parameters;
            SemType result = unitType();

            if ((push || remove) && !receiver.canWrite) {
                error("该容器方法需要可变访问");
            }

            if (push || remove) {
                if (!target.inner) {
                    error("Vec 缺少元素类型");
                    currentType_ = neverType();
                    return;
                }
            }

            if (length) {
                result = usizeType();
            }
            else if (empty) {
                result = boolType();
            }
            else if (push) {
                parameters.push_back(*target.inner);
            }
            else {
                parameters.push_back(usizeType());
                result = *target.inner;
            }
            currentType_ = checkArguments(parameters, expr.args, result);
            return;
        }

        const FunctionItem* function = findMethod(target, expr.method);
        if (!function) {
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }
            error("未找到方法: " + expr.method);
            currentType_ = neverType();
            return;
        }

        if (function->parameters.empty() || !function->parameters.front().isSelf) {
            for (const auto& argument : expr.args) {
                checkExpr(*argument);
            }
            error("没有 self 参数的关联函数不能使用点调用");
            currentType_ = neverType();
            return;
        }

        const auto& self = function->parameters.front();
        if (self.isReference && self.isMutable && !receiver.canWrite) {
            error("该方法需要可变接收者");
        }

        currentType_ = checkFunctionCall(*function, target, expr.args, 1);
    }

    void SemanticAnalyzer::visitFieldAccessExpr(const FieldAccessExpr& expr) {
        checkExpr(*expr.receiver);
        ExprInfo receiver = exprInfo_[expr.receiver.get()];

        while ((receiver.type.kind == TypeKind::Ref || receiver.type.kind == TypeKind::Box) && receiver.type.inner) {
            receiver = dereference(receiver);
        }

        if (receiver.type.kind == TypeKind::Never) {
            currentType_ = neverType();
            return;
        }

        bool found = false;
        SemType fieldType = findField(receiver.type, expr.field, &found);
        if (!found) {
            error("未找到字段: " + expr.field);
            currentType_ = neverType();
            return;
        }
        ExprInfo result = receiver;
        result.type = fieldType;
        result.isPlace = true;
        exprInfo_[&expr] = result;
        currentType_ = fieldType;
    }

    void SemanticAnalyzer::visitIndexExpr(const IndexExpr& expr) {
        checkExpr(*expr.base);
        ExprInfo base = exprInfo_[expr.base.get()];
        while ((base.type.kind == TypeKind::Ref || base.type.kind == TypeKind::Box) && base.type.inner) {
            base = dereference(base);
        }

        SemType expected = usizeType();
        SemType index = checkExpr(*expr.index, &expected);

        if (base.type.kind == TypeKind::Never || index.kind == TypeKind::Never) {
            currentType_ = neverType();
            return;
        }
        if (base.type.kind != TypeKind::Array && base.type.kind != TypeKind::Vec) {
            error("只能对数组或 Vec 取下标");
            currentType_ = neverType();
            return;
        }
        if (index.kind != TypeKind::Usize) {
            error("数组或 Vec 下标必须是 usize");
        }
        if (!base.type.inner) {
            error("容器缺少元素类型");
            currentType_ = neverType();
            return;
        }

        ExprInfo result = base;
        if (base.type.kind == TypeKind::Vec && !base.canWrite) {
            result.blocked = true;
        }
        result.type = *base.type.inner;
        result.isPlace = true;
        result.canWrite = base.canWrite && !result.blocked;
        exprInfo_[&expr] = result;
        currentType_ = result.type;
    }


    int SemanticAnalyzer::readArrayLength(const Expr& expr) {
        SemType expected = usizeType();
        SemType actual;
        long long value = 0;
        if (!evaluateConstantExpr(expr, expected, actual, value)) {
            return -1;
        }
        if (actual.kind != TypeKind::Usize) {
            error("数组长度必须是 usize");
            return -1;
        }
        if (value < 0 || value > 2147483647LL) {
            error("数组长度超出当前支持的范围");
            return -1;
        }
        return static_cast<int>(value);
    }


    SemType SemanticAnalyzer::checkExprType(const Expr& expr, const SemType* expected) {
        if (const auto* literal = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
            bool hasSuffix = false;

            for (const std::string suffix : {"i32", "u32", "isize", "usize"}) {
                if (literal->text.size() >= suffix.size() &&
                    literal->text.compare(literal->text.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    hasSuffix = true;
                    break;
                }
            }

            if (!hasSuffix && expected && isInteger(*expected)) {
                currentType_ = *expected;
                return currentType_;
            }
        }

        if (const auto* array = dynamic_cast<const ArrayExpr*>(&expr)) {
            SemType result;
            bool fixedElementType = expected && expected->kind == TypeKind::Array && expected->inner;
            result.kind = TypeKind::Array;

            if (fixedElementType) {
                result.inner = expected->inner;
            }

            int count = static_cast<int>(array->elements.size());

            if (array->repeat) {
                count = readArrayLength(*array->repeat);
                if (array->elements.size() != 1) {
                    error("重复数组必须有且只有一个元素表达式");
                }
            }

            for (const auto& element : array->elements) {
                SemType elementType = checkExpr(*element, result.inner.get());

                if (!result.inner) {
                    result.inner = std::make_shared<SemType>(elementType);
                }
                else if (fixedElementType) {
                    if (!canCoerce(elementType, *result.inner)) {
                        error("数组元素不符合声明的元素类型");
                    }
                }
                else {
                    SemType merged;
                    if (!mergeTypes(*result.inner, elementType, merged)) {
                        error("数组元素类型不一致");
                    }
                    else {
                        result.inner = std::make_shared<SemType>(merged);
                    }
                }
            }

            if (!result.inner) {
                error("空数组缺少元素类型信息");
                currentType_ = neverType();
                return currentType_;
            }
            if (array->repeat && count > 1 && !supportsTrait(*result.inner, "Copy")) {
                error("重复多份数组元素要求元素类型支持 Copy");
            }
            result.arrayLength = count;
            currentType_ = result;
            return currentType_;
        }
        if (const auto* block = dynamic_cast<const Block*>(&expr)) {
            scopes_.emplace_back(&currentScope());
            SemType result = checkBlockBody(*block, expected);
            scopes_.pop_back();
            currentType_ = result;
            return result;
        }
        if (const auto* conditional = dynamic_cast<const IfExpr*>(&expr)) {
            SemType condition = checkExpr(*conditional->condition);
            if (condition.kind != TypeKind::Bool && condition.kind != TypeKind::Never) {
                error("if 条件必须是 bool");
            }

            const SemType* branchExpected = conditional->elseBranch ? expected : nullptr;

            SemType thenType = checkExpr(*conditional->thenBranch, branchExpected);

            SemType result;

            if (conditional->elseBranch) {
                SemType elseType = checkExpr(*conditional->elseBranch, expected);
                if (!mergeTypes(thenType, elseType, result)) {
                    error("if 两个分支的类型不一致");
                    result = neverType();
                }
            }
            else {
                if (thenType.kind != TypeKind::Unit && thenType.kind != TypeKind::Never) {
                    error("没有 else 的 if 分支必须产生 ()");
                }
                result = unitType();
            }
            if (condition.kind == TypeKind::Never) result = neverType();
            currentType_ = result;
            return result;
        }
        if (const auto* loop = dynamic_cast<const LoopExpr*>(&expr)) {
            LoopContext context;

            if (expected) {
                context.hasExpected = true;
                context.expected = *expected;
            }

            loops_.push_back(context);

            SemType bodyType = checkExpr(*loop->body);

            if (bodyType.kind != TypeKind::Unit && bodyType.kind != TypeKind::Never) {
                error("loop 的循环体必须产生 ()");
            }

            LoopContext finished = loops_.back();
            loops_.pop_back();
            currentType_ = finished.hasBreak ? finished.breakType : neverType();
            ExprInfo info;
            info.type = currentType_;
            info.blocked = finished.blockedResult;
            exprInfo_[&expr] = info;
            return currentType_;
        }
        expr.accept(*this);
        if (expected && currentType_.kind != TypeKind::Never && canCoerce(currentType_, *expected)) {
            currentType_ = *expected;
        }
        return currentType_;
    }

    SemType SemanticAnalyzer::checkExpr(const Expr& expr, const SemType* expected
    ) {
        exprInfo_.erase(&expr);
        SemType type = checkExprType(expr, expected);
        ExprInfo info;
        auto recorded = exprInfo_.find(&expr);

        if (recorded != exprInfo_.end()) {
            info = recorded->second;
        }
        info.type = type;

        if (type.kind == TypeKind::Ref) {
            if (const auto* block = dynamic_cast<const Block*>(&expr)) {
                const Expr* tail = block->tail.get();
                if (!tail && !block->statements.empty()) {
                    const auto* statement = dynamic_cast<const ExprStmt*>(block->statements.back().get());
                    if (statement && !statement->hasSemicolon) {
                        tail = statement->expression.get();
                    }
                }

                if (tail) {
                    auto source = exprInfo_.find(tail);
                    if (source != exprInfo_.end()) {
                        info.blocked = info.blocked || source->second.blocked;
                    }
                }
            }

            if (const auto* conditional = dynamic_cast<const IfExpr*>(&expr)) {
                auto first = exprInfo_.find(conditional->thenBranch.get());
                if (first != exprInfo_.end()) {
                    info.blocked = info.blocked || first->second.blocked;
                }
                if (conditional->elseBranch) {
                    auto second = exprInfo_.find(conditional->elseBranch.get());
                    if (second != exprInfo_.end()) {
                        info.blocked = info.blocked || second->second.blocked;
                    }
                }
            }
        }

        if (expected && expected->kind == TypeKind::Ref && expected->isMutable &&
            type.kind == TypeKind::Ref && type.isMutable && info.blocked) {
            error("该表达式不能提供可变引用访问");
        }
        exprInfo_[&expr] = info;
        currentType_ = type;
        return type;
    }

    SemType SemanticAnalyzer::checkBlockBody(const Block& block, const SemType* expected) {
        SemType result = unitType();
        bool canReachEnd = true;

        for (std::size_t i = 0; i < block.statements.size(); ++i) {
            const Stmt& statement = *block.statements[i];
            const auto* expressionStatement = dynamic_cast<const ExprStmt*>(&statement);
            SemType statementType;

            if (expressionStatement) {
                bool isTail = i + 1 == block.statements.size() && !block.tail &&
                    !expressionStatement->hasSemicolon;

                statementType = checkExpr(
                    *expressionStatement->expression,
                    isTail ? expected : nullptr
                );

                if (!isTail &&
                    !expressionStatement->hasSemicolon &&
                    statementType.kind != TypeKind::Unit &&
                    statementType.kind != TypeKind::Never) {
                    error("作为语句的块表达式必须产生 ()");
                }

                result = isTail ? statementType : unitType();
            }
            else {
                statement.accept(*this);
                statementType = currentType_;
                result = unitType();
            }
            if (statementType.kind == TypeKind::Never) canReachEnd = false;
        }
        if (block.tail) result = checkExpr(*block.tail, expected);
        if (!canReachEnd) result = neverType();
        currentType_ = result;
        return result;
    }

    void SemanticAnalyzer::visitArrayExpr(const ArrayExpr& expr) {
        currentType_ = checkExpr(expr);
    }

    void SemanticAnalyzer::visitStructExpr(const StructExpr& expr) {
        SemType st = resolveStruct(expr.path);
        if (st.kind != TypeKind::Struct || !st.structDef) {
            error("不是结构体");
            currentType_ = neverType();
            return;
        }
        std::unordered_map<std::string, SemType> fields;
        auto structure = structTypes_.find(st.structDef->name);
        if (structure != structTypes_.end()) {
            for (const auto& field : structure->second.fields) {
                fields.emplace(field.first, field.second);
            }
        }
        std::unordered_set<std::string> provided;

        for (std::size_t i = 0; i < expr.fields.size(); ++i) {
            const std::string& name = expr.fields[i];
            auto expectedField = fields.find(name);
            const SemType* expectedType = nullptr;
            if (expectedField != fields.end()) expectedType = &expectedField->second;
            SemType valueType = checkExpr(*expr.values[i], expectedType);

            if (!provided.insert(name).second) {
                error("重复初始化字段: " + name);
            }
            auto field = fields.find(name);
            if (field == fields.end()) {
                error("结构体没有字段: " + name);
                continue;
            }
            if (!sameType(field->second, valueType)) {
                error(
                    "字段 " + name +
                    " 需要 " + typeName(field->second) +
                    "，实际为 " + typeName(valueType)
                );
            }
        }
        for (const auto& field : st.structDef->fields) {
            if (provided.find(field.name) == provided.end()) {
                error("缺少字段: " + field.name);
            }
        }
        currentType_ = st;
    }

    void SemanticAnalyzer::visitIfExpr(const IfExpr& expr) {
        currentType_ = checkExpr(expr);
    }

    void SemanticAnalyzer::visitLoopExpr(const LoopExpr& expr) {
        currentType_ = checkExpr(expr);
    }


    void SemanticAnalyzer::visitWhileExpr(const WhileExpr& expr) {
        std::vector<LoopContext> outerLoops = std::move(loops_);
        loops_.clear();
        SemType expected = boolType();
        SemType condition = checkExpr(*expr.condition, &expected);
        loops_ = std::move(outerLoops);
        if (condition.kind != TypeKind::Bool && condition.kind != TypeKind::Never) {
            error("while 条件必须是 bool");
        }

        LoopContext context;
        context.allowsValue = false;
        loops_.push_back(context);

        SemType bodyType = checkExpr(*expr.body);
        if (bodyType.kind != TypeKind::Unit && bodyType.kind != TypeKind::Never) {
            error("while 的循环体必须产生 ()");
        }

        loops_.pop_back();
        currentType_ = condition.kind == TypeKind::Never ? neverType() : unitType();
    }

    void SemanticAnalyzer::visitBreakExpr(const BreakExpr& expr) {
        if (loops_.empty()) {
            if (expr.value) {
                checkExpr(*expr.value);
            }
            error("break 出现在循环外");
            currentType_ = neverType();
            return;
        }

        std::size_t target = loops_.size() - 1;

        bool allowsValue = loops_[target].allowsValue;
        bool hasExpected = loops_[target].hasExpected;
        SemType expected = loops_[target].expected;

        SemType valueType = unitType();

        if (expr.value) {
            valueType = checkExpr(*expr.value, hasExpected ? &expected : nullptr);
        }

        if (expr.value && !allowsValue) error("while 中的 break 不能携带值");

        if (hasExpected && !canCoerce(valueType, expected)) {
            error("break 需要 " + typeName(expected) + "，实际为 " + typeName(valueType));
        }

        if (valueType.kind != TypeKind::Never) {
            if (!loops_[target].hasBreak) {
                loops_[target].hasBreak = true;
                loops_[target].breakType = valueType;
            }
            else {
                SemType merged;
                if (!mergeTypes(loops_[target].breakType, valueType, merged)) {
                    error("同一个 loop 的 break 值类型不一致");
                }
                else loops_[target].breakType = merged;
            }
        }
        if (expr.value && valueType.kind == TypeKind::Ref) {
            loops_[target].blockedResult =
                loops_[target].blockedResult ||
                exprInfo_[expr.value.get()].blocked;
        }
        currentType_ = neverType();
    }

    void SemanticAnalyzer::visitReturnExpr(const ReturnExpr& expr) {
        SemType valueType = unitType();
        if (expr.value) {
            valueType = checkExpr(*expr.value, &returnType_);
        }
        if (valueType.kind != TypeKind::Never && !sameType(valueType, returnType_))
            error(
                "返回值需要 " + typeName(returnType_) + "，实际为 " + typeName(valueType));
        currentType_ = neverType();
    }

    void SemanticAnalyzer::visitContinueExpr(const ContinueExpr&) {
        if (loops_.empty()) {
            error("continue 出现在循环外");
        }
        currentType_ = neverType();
    }

    void SemanticAnalyzer::visitUnitExpr(const UnitExpr&) {
        currentType_ = unitType();
    }

    void SemanticAnalyzer::visitLetStmt(const LetStmt& stmt) {
        if (!stmt.initializer) {
            error("当前尚未实现无初始化值的变量声明");
            currentType_ = unitType();
            return;
        }

        SemType declared;
        const SemType* expected = nullptr;

        if (stmt.type) {
            declared = resolveType(*stmt.type);
            expected = &declared;
        }

        SemType initial = checkExpr(*stmt.initializer, expected);
        if (initial.kind == TypeKind::Ref && initial.isMutable && exprInfo_[stmt.initializer.get()].blocked) {
            error("该初始化表达式不能提供可变引用");
        }
        SemType bindingType = initial;

        if (expected) {
            if (initial.kind != TypeKind::Never && !sameType(declared, initial)) {
                error("变量 " + stmt.name + " 需要 " + typeName(declared) + "，实际为 " + typeName(initial));
            }
            bindingType = declared;
        }

        if (stmt.name != "_") {
            Symbol symbol;
            symbol.kind = Symbol::Kind::Var;
            symbol.type = bindingType;
            symbol.isMutable = stmt.isMutable;
            currentScope().insert(stmt.name, std::move(symbol));
        }

        currentType_ = initial.kind == TypeKind::Never ? neverType() : unitType();
    }

    void SemanticAnalyzer::visitExprStmt(const ExprStmt& stmt) {
        SemType type = checkExpr(*stmt.expression);
        currentType_ = type.kind == TypeKind::Never ? neverType() : unitType();
    }

    void SemanticAnalyzer::visitEmptyStmt(const EmptyStmt&) {}

    void SemanticAnalyzer::visitUseItem(const UseItem& item) {
        (void)item;
    }

    void SemanticAnalyzer::visitStructItem(const StructItem& item) {
        std::unordered_set<std::string> names;

        for (const auto& field : item.fields) {
            if (!names.insert(field.name).second) {
                error("结构体 " + item.name + " 重复定义字段: " + field.name);
            }
        }

        SemType type;
        type.kind = TypeKind::Struct;
        type.structDef = &item;
        checkLayout(type);
        std::unordered_set<std::string> requested;

        for (const auto& attribute : item.attributes) {
            if (!requested.insert(attribute).second) {
                error("重复的派生能力: " + attribute);
            }
        }
        if (requested.find("Copy") != requested.end() &&
            requested.find("Clone") == requested.end()) {
            error("派生 Copy 必须同时显式派生 Clone");
        }
        if (requested.find("Eq") != requested.end() &&
            requested.find("PartialEq") == requested.end()) {
            error("派生 Eq 必须同时显式派生 PartialEq");
        }
        for (const auto& trait : requested) {
            if (!supportsTrait(type, trait)) {
                error("结构体 " + item.name + " 的字段不满足 " + trait + " 要求");
            }
        }
    }

    void SemanticAnalyzer::visitFunctionItem(const FunctionItem& item) {
        SemType savedReturn = returnType_;
        returnType_ = item.resultType ? resolveType(*item.resultType) : unitType();

        scopes_.emplace_back(&currentScope());

        std::unordered_set<std::string> parameterNames;
        for (const auto& p : item.parameters) {
            if (!parameterNames.insert(p.name).second) {
                error("重复定义参数: " + p.name);
                continue;
            }
            Symbol sym;
            sym.kind = Symbol::Kind::Var;
            if (p.isSelf) {
                if (selfType_.kind == TypeKind::Struct) {
                    sym.type = selfType_;
                    if (p.isReference) {
                        SemType r;
                        r.kind = TypeKind::Ref;
                        r.isMutable = p.isMutable;
                        r.inner = std::make_shared<SemType>(selfType_);
                        sym.type = r;
                    }
                    sym.isMutable = p.isMutable;
                }
            }
            else {
                sym.type = resolveType(*p.type);
                sym.isMutable = p.isMutable;
            }
            currentScope().insert(p.name, std::move(sym));
        }

        SemType bodyType = checkBlockBody(*item.body, &returnType_);

        if (bodyType.kind != TypeKind::Never && !sameType(bodyType, returnType_))
            error(
                "函数 " + item.name + " 需要返回 " + typeName(returnType_) + "，函数体结果为 " + typeName(bodyType));

        scopes_.pop_back();
        returnType_ = savedReturn;
        currentType_ = unitType();
    }

    void SemanticAnalyzer::visitConstItem(const ConstItem& item) {
        std::string key = item.name;
        if (selfType_.kind == TypeKind::Struct && selfType_.structDef) {
            key = selfType_.structDef->name + "::" + item.name;
        }

        auto constant = constants_.find(key);
        if (constant == constants_.end()) {
            error("常量定义未登记: " + key);
            currentType_ = neverType();
            return;
        }
        if (constant->second.definition != &item) {
            currentType_ = neverType();
            return;
        }
        if (!evaluateConstant(constant->second)) {
            currentType_ = neverType();
            return;
        }
        currentType_ = constant->second.type;
    }

    void SemanticAnalyzer::visitImplItem(const ImplItem& item) {
        SemType target = resolveType(*item.selfType);
        if (target.kind != TypeKind::Struct) {
            error("impl 目标必须是结构体");
            return;
        }
        SemType savedSelf = selfType_;
        selfType_ = target;
        for (const auto& assoc : item.items) {
            assoc->accept(*this);
        }
        selfType_ = savedSelf;
    }

    bool SemanticAnalyzer::checkLayout(const SemType& type) {
        if (type.kind == TypeKind::Array) {
            return type.inner && checkLayout(*type.inner);
        }
        if (type.kind != TypeKind::Struct) return true;
        if (!type.structDef) return false;

        const StructItem* definition = type.structDef;
        int state = layoutState_[definition];
        if (state == 1) {
            error("结构体存在无限大小的递归布局: " + definition->name);
            return false;
        }
        if (state == 2) return true;
        if (state == 3) return false;

        layoutState_[definition] = 1;
        SemType savedSelf = selfType_;
        selfType_ = type;
        bool valid = true;
        for (const auto& field : definition->fields) {
            SemType fieldType = resolveType(*field.type);
            if (!checkLayout(fieldType)) {
                valid = false;
            }
        }
        selfType_ = savedSelf;
        layoutState_[definition] = valid ? 2 : 3;
        return valid;
    }
}
