#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "type.h"

namespace rx
{
    struct Symbol {
        enum class Kind { Var, Func, Struct, Const };

        Kind kind = Kind::Var;
        SemType type;
        bool isMutable = false;
        bool moved = false;

        std::vector<SemType> paramTypes;
        SemType returnType;

        std::vector<std::pair<std::string, SemType>> fields;
    };

    class Scope {
    public:
        explicit Scope(Scope* parent = nullptr);

        Symbol* lookup(const std::string& name);
        void insert(const std::string& name, Symbol symbol);

    private:
        std::unordered_map<std::string, Symbol> symbols_;
        Scope* parent_;
    };
}
