#include "symbol.h"

namespace rx
{
    Scope::Scope(Scope* parent) : parent_(parent) {}

    Symbol* Scope::lookup(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) return &it->second;
        if (parent_) return parent_->lookup(name);
        return nullptr;
    }

    void Scope::insert(const std::string& name, Symbol symbol) {
        symbols_[name] = std::move(symbol);
    }
}
