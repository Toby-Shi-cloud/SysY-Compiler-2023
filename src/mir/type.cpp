//
// Created by toby on 2023/9/26.
//

#include "mir/type.h"

// Type
namespace mir {
pType Type::getVoidType() {
    static Type type(VOID);
    return &type;
}

pType Type::getLabelType() {
    static Type type(LABEL);
    return &type;
}

pIntegerType Type::getI1Type() { return IntegerType::getIntegerType(1); }
pIntegerType Type::getI8Type() { return IntegerType::getIntegerType(8); }
pIntegerType Type::getI32Type() { return IntegerType::getIntegerType(32); }
pIntegerType Type::getI64Type() { return IntegerType::getIntegerType(64); }

pType Type::getFloatType() {
    static Type type(FLOAT);
    return &type;
}

pPointerType Type::getPointerType(pType base) { return PointerType::getPointerType(base); }
pPointerType Type::getStringType() { return PointerType::getPointerType(getI8Type()); }
pArrayType Type::getStringType(int size) { return ArrayType::getArrayType(size, getI8Type()); }

bool Type::isStringTy() const {
    return isPointerTy() && getPointerBase() == getI8Type() || isArrayTy() && getArrayBase() == getI8Type();
}

int Type::getIntegerBits() const {
    assert(isIntegerTy());
    return static_cast<pIntegerType>(this)->bits;  // NOLINT
}

pType Type::getPointerBase() const {
    assert(isPointerTy());
    return static_cast<pPointerType>(this)->base;  // NOLINT
}

int Type::getArraySize() const {
    assert(isArrayTy());
    return static_cast<pArrayType>(this)->size;  // NOLINT
}

pType Type::getArrayBase() const {
    assert(isArrayTy());
    return static_cast<pArrayType>(this)->base;  // NOLINT
}

pType Type::getBase() const {
    if (isPointerTy()) return getPointerBase();
    if (isArrayTy()) return getArrayBase();
    return nullptr;
}

pType Type::getBaseRecursive() const {
    if (auto ty = getBase()) return ty->getBaseRecursive();
    return this;
}

pType Type::getFunctionRet() const {
    assert(isFunctionTy());
    return static_cast<pFunctionType>(this)->ret;  // NOLINT
}

const std::vector<pType> &Type::getFunctionParams() const {
    assert(isFunctionTy());
    return static_cast<pFunctionType>(this)->params;  // NOLINT
}

pType Type::getFunctionParam(int i) const {
    assert(isFunctionTy());
    return static_cast<pFunctionType>(this)->params[i];  // NOLINT
}

int Type::getFunctionParamCount() const {
    assert(isFunctionTy());
    return static_cast<pFunctionType>(this)->params.size();  // NOLINT
}

bool Type::convertableTo(pType other) const {
    if (this == other) return true;
    if (isIntegerTy() || isFloatTy()) return other->isIntegerTy() || other->isFloatTy();
    if (isArrayTy() && other->isPointerTy()) return getArrayBase() == other->getPointerBase();
    return false;
}

size_t Type::size() const { return static_cast<size_t>(ssize()); }

ssize_t Type::ssize() const {
    if (isIntegerTy()) return (getIntegerBits() + 7) / 8;
    if (isFloatTy()) return 4;
    if (isPointerTy() || isFunctionTy()) return 4;
    if (isArrayTy()) return getArraySize() * getArrayBase()->ssize();
    return 0;
}

std::string Type::to_string() const {
    using namespace std::string_literals;
    if (isLabelTy()) return "<label>"s;
    if (isVoidTy()) return "void"s;
    if (isFloatTy()) return "float"s;
    if (isIntegerTy()) return "i" + std::to_string(getIntegerBits());
    if (isPointerTy()) return "ptr"s;
    if (isArrayTy()) return "[" + std::to_string(getArraySize()) + " x " + getArrayBase()->to_string() + "]";
    if (isFunctionTy()) {
        std::string ret = getFunctionRet()->to_string();
        std::string params;
        for (auto param : getFunctionParams()) {
            params += param->to_string() + ", ";
        }
        if (!params.empty()) params.pop_back(), params.pop_back();
        return ret + " (" + params + ")";
    }
    return "<unknown>"s;
}
}  // namespace mir

#include <unordered_map>

template <typename First>
struct [[maybe_unused]] std::hash<std::pair<First, mir::pType>> {
    size_t operator()(const std::pair<First, mir::pType> &p) const {
        return std::hash<First>()(p.first) * 10007 + std::hash<mir::pType>()(p.second);
    }
};

template <typename Second>
struct [[maybe_unused]] std::hash<std::pair<mir::pType, Second>> {
    size_t operator()(const std::pair<mir::pType, Second> &p) const {
        return std::hash<mir::pType>()(p.first) * 10007 + std::hash<Second>()(p.second);
    }
};

template <>
struct [[maybe_unused]] std::hash<std::vector<mir::pType>> {
    size_t operator()(const std::vector<mir::pType> &v) const noexcept {
        size_t ret = 0;
        for (const auto &e : v) {
            ret = ret * 10007 + std::hash<mir::pType>()(e);
        }
        return ret;
    }
};

// Derived types
namespace mir {
template <typename K, typename V>
using map = std::unordered_map<K, V>;

pIntegerType IntegerType::getIntegerType(int bits) {
    static map<int, pIntegerType> cache;
    if (cache.find(bits) == cache.end()) {
        cache[bits] = new IntegerType(bits);
    }
    return cache[bits];
}

pPointerType PointerType::getPointerType(pType base) {
    static map<pType, pPointerType> cache;
    if (cache.find(base) == cache.end()) {
        cache[base] = new PointerType(base);
    }
    return cache[base];
}

pArrayType ArrayType::getArrayType(int size, pType base) {
    static map<std::pair<int, pType>, pArrayType> cache;
    std::pair key{size, base};
    if (cache.find(key) == cache.end()) {
        cache[key] = new ArrayType(size, base);
    }
    return cache[key];
}

pFunctionType FunctionType::getFunctionType(pType ret, std::vector<pType> &&params) {
    static map<std::pair<pType, std::vector<pType>>, pFunctionType> cache;
    std::pair key{ret, params};
    if (cache.find(key) == cache.end()) {
        cache[key] = new FunctionType(ret, std::move(params));
    }
    return cache[key];
}
}  // namespace mir
