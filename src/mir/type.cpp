//
// Created by toby on 2023/9/26.
//

#include "type.h"

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

    pIntegerType Type::getI1Type() {
        return IntegerType::getIntegerType(1);
    }

    pIntegerType Type::getI8Type() {
        return IntegerType::getIntegerType(8);
    }

    pIntegerType Type::getI32Type() {
        return IntegerType::getIntegerType(32);
    }

    int Type::getIntegerBits() const {
        assert(isIntegerTy());
        return static_cast<pIntegerType>(this)->bits; // NOLINT
    }

    pType Type::getPointerBase() const {
        assert(isPointerTy());
        return static_cast<pPointerType>(this)->base; // NOLINT
    }

    int Type::getArraySize() const {
        assert(isArrayTy());
        return static_cast<pArrayType>(this)->size; // NOLINT
    }

    pType Type::getArrayBase() const {
        assert(isArrayTy());
        return static_cast<pArrayType>(this)->base; // NOLINT
    }

    pType Type::getFunctionRet() const {
        assert(isFunctionTy());
        return static_cast<pFunctionType>(this)->ret; // NOLINT
    }

    const std::vector<pType> &Type::getFunctionParams() const {
        assert(isFunctionTy());
        return static_cast<pFunctionType>(this)->params; // NOLINT
    }

    pType Type::getFunctionParam(int i) const {
        assert(isFunctionTy());
        return static_cast<pFunctionType>(this)->params[i]; // NOLINT
    }

    int Type::getFunctionParamCount() const {
        assert(isFunctionTy());
        return static_cast<pFunctionType>(this)->params.size(); // NOLINT
    }

    bool Type::convertableTo(pType other) const {
        if (this == other) return true;
        if (isIntegerTy()) return other->isIntegerTy();
        if (isArrayTy() && other->isPointerTy()) return getArrayBase() == other->getPointerBase();
        return false;
    }
}

#include <unordered_map>

template<typename First, typename Second>
struct [[maybe_unused]] std::hash<std::pair<First, Second>> {
    size_t operator()(const std::pair<First, Second> &p) const {
        return std::hash<First>()(p.first) ^ std::hash<Second>()(p.second);
    }
};

template<typename T>
struct [[maybe_unused]] std::hash<std::vector<T>> {
    size_t operator()(const std::vector<T> &v) const {
        size_t ret = 0;
        for (const auto &i: v) {
            ret ^= std::hash<T>()(i);
        }
        return ret;
    }
};

// Derived types
namespace mir {
    template<typename K, typename V>
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

    pArrayType ArrayType::getArrayTypeType(int size, pType base) {
        static map<std::pair<int, pType>, pArrayType> cache;
        std::pair<int, pType> key{size, base};
        if (cache.find(key) == cache.end()) {
            cache[key] = new ArrayType(size, base);
        }
        return cache[key];
    }

    pFunctionType FunctionType::getFunctionType(pType ret, std::vector<pType> &&params) {
        static map<std::pair<pType, std::vector<pType>>, pFunctionType> cache;
        std::pair<pType, std::vector<pType>> key{ret, params};
        if (cache.find(key) == cache.end()) {
            cache[key] = new FunctionType(ret, std::move(params));
        }
        return cache[key];
    }
}
