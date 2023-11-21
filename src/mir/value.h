//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MIR_VALUE_H
#define COMPILER_MIR_VALUE_H

#include <memory>
#include <string>
#include <ostream>
#include <unordered_set>
#include "type.h"

// Use List
namespace mir {
    class Value;

    class User;

    /**
     * Use does NOT own any Value or User
     */
    struct Use {
        Value *value;
        std::unordered_set<User *> users;
    };
}

// Values
namespace mir {
    /**
     * Value is the base class for all mir values.
     */
    class Value {
        friend User;

        /**
         * Every Value has a type. <br>
         */
        pType type;

        /**
         * Not every Value has a name. <br>
         * To mark the value as anonymous, set name to empty string.
         */
        std::string name;
        inline static const std::string anonymous = "<anonymous>";

        /**
         * To mark the value as constant, set isConstant to true. <br>
         * False by default.
         */
        bool isConstant;

        /**
         * Use shared pointer here to avoid 'use after delete'. <br>
         */
        std::shared_ptr<Use> use;

    public:
        explicit Value(pType type, bool isConstant) : use(new Use{this}), type(type), isConstant(isConstant) {}

        explicit Value(pType type) : Value(type, false) {}

        virtual ~Value() = default;

        void setConst(bool constant = true) { isConstant = constant; }

        void setName(std::string str) { name = std::move(str); }

        [[nodiscard]] bool hasName() const { return !name.empty(); }

        [[nodiscard]] const std::string &getName() const { return hasName() ? name : anonymous; }

        [[nodiscard]] pType getType() const { return type; }

        [[nodiscard]] bool isUsed() const { return !use->users.empty(); }

        [[nodiscard]] bool isConst() const { return isConstant; }

        [[nodiscard]] long getId() const { return std::strtol(getName().c_str() + 1, nullptr, 0); }

        void swap(Value *other) {
            std::swap(use->value, other->use->value);
            std::swap(use, other->use);
        }

        inline void moveTo(const Value *other) const;
    };

    /**
     * User is the base class for all mir Value which uses other Values.
     */
    class User : public Value {
        friend Value;

        /**
         * Use shared pointer here to avoid 'use after delete'. <br>
         */
        std::vector<std::shared_ptr<Use>> operands;

    protected:
        void addOperand(const Value *value) {
            operands.push_back(value->use);
            if (value != this)
                value->use->users.insert(this);
        }

    public:
        template<typename... Args>
        explicit User(pType type, Args... args) : Value(type), operands{args->use...} {
            for (auto &operand: operands) operand->users.insert(this);
        }

        explicit User(pType type, const std::vector<Value *> &args) : Value(type) {
            for (auto arg: args) addOperand(arg);
        }

        ~User() override {
            for (auto &operand: operands)
                operand->users.erase(this);
        }

        template<typename R = Value>
        [[nodiscard]] R *getOperand(int i) const { return static_cast<R *>(operands[i]->value); }

        [[nodiscard]] auto getNumOperands() const { return operands.size(); }
    };

    inline void Value::moveTo(const Value *other) const {
        if (this == other) return;
        for (auto &&user: use->users)
            for (auto &&operand: user->operands)
                if (operand == this->use)
                    operand = other->use;
        other->use->users.insert(use->users.begin(), use->users.end());
        use->users.clear();
    }

    inline std::ostream &operator<<(std::ostream &os, const Value &value) {
        if (value.getType()->isArrayTy()) os << "ptr";
        else os << value.getType();
        return os << " " << value.getName();
    }

    template<typename T>
    std::enable_if_t<std::is_base_of_v<Value, T>, std::ostream> &
    operator<<(std::ostream &os, const T *value) {
        return os << *value;
    }
}

#endif //COMPILER_MIR_VALUE_H
