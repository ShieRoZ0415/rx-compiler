#pragma once

#include <memory>
#include <string>

#include "ast.h"

namespace rx
{
    enum class TypeKind {
        Int, Uint, Isize, Usize, Bool, Unit, Never, Struct, Ref, Array, Box, Vec
    };
    struct SemType {
        TypeKind kind = TypeKind::Unit;
        const StructItem* structDef = nullptr;
        std::shared_ptr<SemType> inner;
        bool isMutable = false;
        int arrayLength = -1;
    };

    SemType intType();
    SemType uintType();
    SemType isizeType();
    SemType usizeType();
    SemType boolType();
    SemType unitType();
    SemType neverType();

    bool isInteger(const SemType& type);
    bool isCopy(const SemType& type);
    bool sameType(const SemType& a, const SemType& b);
    bool canCoerce(const SemType& actual, const SemType& expected);
    bool mergeTypes(const SemType& first, const SemType& second, SemType& result);
    std::string typeName(const SemType& type);
}
