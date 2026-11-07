#include "ir.h"

#include <iostream>
#include <cassert>

namespace rx
{
    void IRBuilder::beginFunction(const std::string& header) {
        header_ = header;
        allocas_.clear();
        instructions_.clear();
        nextTemp_ = 0;
        nextLabel_ = 0;
        terminated_ = false;
    }

    void IRBuilder::emit(const std::string& instruction) {
        assert(!terminated_);
        instructions_.push_back("  " + instruction);
    }

    void IRBuilder::emitAlloca(const std::string& instruction) {
        allocas_.push_back("  " + instruction);
    }

    void IRBuilder::endBlock(const std::string& instruction) {
        assert(!terminated_);
        instructions_.push_back("  " + instruction);
        terminated_ = true;
    }

    void IRBuilder::beginBlock(const std::string& label) {
        assert(terminated_);
        instructions_.push_back(label + ":");
        terminated_ = false;
    }

    bool IRBuilder::isTerminated() const {
        return terminated_;
    }

    void IRBuilder::finishFunction(std::ostream& output) {
        assert(!header_.empty());
        assert(terminated_);
        output << header_ << " {\n";
        output << "entry:\n";
        for (const auto& instruction : allocas_) {
            output << instruction << "\n";
        }
        for (const auto& instruction : instructions_) {
            output << instruction << "\n";
        }
        output << "}\n\n";
    }

    std::string llvmType(const SemType& type) {
        switch (type.kind) {
        case TypeKind::Int:
        case TypeKind::Uint:
        case TypeKind::Isize:
        case TypeKind::Usize:
            return "i32";
        case TypeKind::Bool:
            return "i8";
        case TypeKind::Unit:
            return "{}";
        case TypeKind::Ref:
        case TypeKind::Box:
            return "ptr";
        case TypeKind::Vec:
            return "%rx.vec";
        case TypeKind::Array:
            if (!type.inner || type.arrayLength < 0) {
                std::cerr << "Invalid array type in IR.\n";
                return "";
            }
            return "[" + std::to_string(type.arrayLength) + " x " + llvmType(*type.inner) + "]";
        case TypeKind::Struct:
            if (!type.structDef) {
                std::cerr << "Missing struct definition in IR.\n";
                return "";
            }
            return "%struct." + type.structDef->name;
        case TypeKind::Never:
            std::cerr << "Never has no normal IR value.\n";
            return "";
        }
        return "";
    }

    std::string IRBuilder::newTemp() {
        return "%v" + std::to_string(nextTemp_++);
    }

    std::string IRBuilder::newLabel(const std::string& prefix) {
        return prefix + "." + std::to_string(nextLabel_++);
    }
}
