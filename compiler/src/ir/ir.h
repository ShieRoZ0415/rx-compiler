#pragma once

#include <ostream>
#include <string>
#include <vector>

#include "semantic/type.h"

namespace rx
{
    struct IRValue {
        SemType type;
        std::string operand;
    };
    struct IRPlace {
        SemType type;
        std::string address;
    };

    std::string llvmType(const SemType& type);
    class IRBuilder {
    public:
        std::string newTemp();
        std::string newLabel(const std::string& prefix = "block");
        void beginFunction(const std::string& header);
        void emit(const std::string& instruction);
        void emitAlloca(const std::string& instruction);
        void beginBlock(const std::string& label);
        void endBlock(const std::string& instruction);
        bool isTerminated() const;
        void finishFunction(std::ostream& output);
    private:
        std::string header_;
        std::vector<std::string> allocas_;
        std::vector<std::string> instructions_;
        int nextTemp_ = 0;
        int nextLabel_ = 0;
        bool terminated_ = false;
    };
}
