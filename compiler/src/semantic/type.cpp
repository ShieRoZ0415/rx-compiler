#include "type.h"

namespace rx
{
    SemType intType() { SemType t; t.kind = TypeKind::Int; return t; }
    SemType uintType() { SemType t; t.kind = TypeKind::Uint; return t; }
    SemType isizeType() { SemType t; t.kind = TypeKind::Isize; return t; }
    SemType usizeType() { SemType t; t.kind = TypeKind::Usize; return t; }
    SemType boolType() { SemType t; t.kind = TypeKind::Bool; return t; }
    SemType unitType() { SemType t; t.kind = TypeKind::Unit; return t; }
    SemType neverType() { SemType t; t.kind = TypeKind::Never; return t; }

    bool isInteger(const SemType& type) {
        return type.kind == TypeKind::Int || type.kind == TypeKind::Uint || type.kind == TypeKind::Isize || type.kind == TypeKind::Usize;
    }

    bool isCopy(const SemType& type) {
        switch (type.kind) {
        case TypeKind::Int:
        case TypeKind::Uint:
        case TypeKind::Isize:
        case TypeKind::Usize:
        case TypeKind::Bool:
        case TypeKind::Unit:
        case TypeKind::Never:
        case TypeKind::Ref:
            return true;
        case TypeKind::Struct:
            if (!type.structDef) return false;
            for (const auto& attr : type.structDef->attributes) {
                if (attr == "Copy") return true;
            }
            return false;
        default:
            return false;
        }
    }

    bool sameType(const SemType& a, const SemType& b) {
        if (a.kind != b.kind) return false;
        switch (a.kind) {
        case TypeKind::Struct:
            return a.structDef == b.structDef;
        case TypeKind::Ref:
            return a.isMutable == b.isMutable && a.inner && b.inner && sameType(*a.inner, *b.inner);
        case TypeKind::Array:
            return a.arrayLength == b.arrayLength && a.inner && b.inner && sameType(*a.inner, *b.inner);
        case TypeKind::Box:
        case TypeKind::Vec:
            return a.inner && b.inner && sameType(*a.inner, *b.inner);
        default:
            return true;
        }
    }

    bool canCoerce(const SemType& actual, const SemType& expected) {
        if (sameType(actual, expected)) return true;
        if (actual.kind == TypeKind::Never) return true;

        if (actual.kind != TypeKind::Ref || expected.kind != TypeKind::Ref || !actual.inner || !expected.inner) {
            return false;
        }
        if (expected.isMutable && !actual.isMutable) return false;

        const SemType* source = actual.inner.get();
        while (true) {
            if (sameType(*source, *expected.inner)) return true;

            if (source->kind == TypeKind::Box && source->inner) {
                source = source->inner.get();
            }
            else if (source->kind == TypeKind::Ref && source->inner && (!expected.isMutable || source->isMutable)) {
                source = source->inner.get();
            }
            else return false;
        }
    }

    bool mergeTypes(const SemType& first, const SemType& second, SemType& result) {
        if (canCoerce(first, second)) {
            result = second;
            return true;
        }

        if (canCoerce(second, first)) {
            result = first;
            return true;
        }
        return false;
    }

    std::string typeName(const SemType& type) {
        switch (type.kind) {
        case TypeKind::Int: return "i32";
        case TypeKind::Uint: return "u32";
        case TypeKind::Isize: return "isize";
        case TypeKind::Usize: return "usize";
        case TypeKind::Bool: return "bool";
        case TypeKind::Unit: return "()";
        case TypeKind::Never: return "!";
        case TypeKind::Struct: return type.structDef ? type.structDef->name : "struct";
        case TypeKind::Ref: return std::string("&") + (type.isMutable ? "mut " : "") + (type.inner ? typeName(*type.inner) : "?");
        case TypeKind::Array: return std::string("[") + (type.inner ? typeName(*type.inner) : "?") + "; " + std::to_string(type.arrayLength) + "]";
        case TypeKind::Box: return std::string("Box<") + (type.inner ? typeName(*type.inner) : "?") + ">";
        case TypeKind::Vec: return std::string("Vec<") + (type.inner ? typeName(*type.inner) : "?") + ">";
        }
        return "?";
    }
}
