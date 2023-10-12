//
// Created by toby on 2023/9/26.
//

#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

#include "../dbg.h"
#include <vector>
#include <ostream>

namespace mir {
    struct Type;
    struct IntegerType;
    struct PointerType;
    struct ArrayType;
    struct FunctionType;
    using pType = const Type *;
    using pIntegerType = const IntegerType *;
    using pPointerType = const PointerType *;
    using pArrayType = const ArrayType *;
    using pFunctionType = const FunctionType *;
}

namespace mir {
    /**
     * Note: <br>
     * Since we don't need to modify the type, we can use const pointer to represent the type. <br>
     * Type must be immutable. DO NOT use const_cast. <br>
     * Two types are equal iff they are the same object. <br>
     * Once a type is created, it will never be destroyed. <br>
     */
    struct Type {
        enum TypeID {
            VOID, LABEL, INTEGER, POINTER, ARRAY, FUNCTION
        } type;

        Type() = delete;

        Type(const Type &) = delete;

        Type(Type &&) = delete;

        Type &operator=(const Type &) = delete;

        Type &operator=(Type &&) = delete;

        static pType getVoidType();

        static pType getLabelType();

        static pIntegerType getI1Type();

        static pIntegerType getI8Type();

        static pIntegerType getI32Type();

        bool operator==(const Type &other) const { return this == &other; }

        bool operator!=(const Type &other) const { return this != &other; }

        [[nodiscard]] bool isVoidTy() const { return type == VOID; }

        [[nodiscard]] bool isLabelTy() const { return type == LABEL; }

        [[nodiscard]] bool isIntegerTy() const { return type == INTEGER; }

        [[nodiscard]] bool isPointerTy() const { return type == POINTER; }

        [[nodiscard]] bool isArrayTy() const { return type == ARRAY; }

        [[nodiscard]] bool isFunctionTy() const { return type == FUNCTION; }

        [[nodiscard]] bool isStringTy() const { return type == ARRAY && getArrayBase() == (Type *)getI8Type(); }

        [[nodiscard]] int getIntegerBits() const;

        [[nodiscard]] pType getPointerBase() const;

        [[nodiscard]] int getArraySize() const;

        [[nodiscard]] pType getArrayBase() const;

        [[nodiscard]] pType getFunctionRet() const;

        [[nodiscard]] const std::vector<pType> &getFunctionParams() const;

        [[nodiscard]] pType getFunctionParam(int i) const;

        [[nodiscard]] int getFunctionParamCount() const;

        [[nodiscard]] bool convertableTo(pType other) const;

        [[nodiscard]] size_t size() const;

        [[nodiscard]] ssize_t ssize() const;

        [[nodiscard]] std::string to_string() const;

        friend std::ostream &operator<<(std::ostream &o, const Type &t) { return o << t.to_string(); }

        friend std::ostream &operator<<(std::ostream &o, pType t) { return o << t->to_string(); }

    protected:
        explicit Type(TypeID type) : type(type) {}
    };

    struct IntegerType : Type {
        int bits;

        static pIntegerType getIntegerType(int bits);

    private:
        explicit IntegerType(int bits) : Type(INTEGER), bits(bits) {}
    };

    struct PointerType : Type {
        pType base;

        static pPointerType getPointerType(pType base);

    private:
        explicit PointerType(pType base) : Type(POINTER), base(base) {}
    };

    struct ArrayType : Type {
        int size;
        pType base;

        static pArrayType getArrayType(int size, pType base);

    private:
        explicit ArrayType(int size, pType base) : Type(ARRAY), size(size), base(base) {}
    };

    struct FunctionType : Type {
        pType ret;
        std::vector<pType> params;

        static pFunctionType getFunctionType(pType ret, std::vector<pType> &&params);

    private:
        explicit FunctionType(pType ret, std::vector<pType> &&params) :
                Type(FUNCTION), ret(ret), params(std::move(params)) {}
    };
}

#endif //COMPILER_TYPE_H
