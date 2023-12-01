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
        explicit Value(pType type, bool isConstant) : type(type), isConstant(isConstant), use(new Use{this}) {}

        explicit Value(pType type) : Value(type, false) {}

        Value(const Value &value) : type(value.type), name(value.name), isConstant(value.isConstant),
                                    use(new Use{this}) {}

        Value(Value &&) = default;

        virtual ~Value() = default;

        void setConst(bool constant = true) { isConstant = constant; }

        void setName(std::string str) { name = std::move(str); }

        [[nodiscard]] bool hasName() const { return !name.empty(); }

        [[nodiscard]] const std::string &getName() const { return hasName() ? name : anonymous; }

        [[nodiscard]] pType getType() const { return type; }

        [[nodiscard]] inline bool isUsed() const;

        [[nodiscard]] bool isConst() const { return isConstant; }

        [[nodiscard]] long getId() const { return std::strtol(getName().c_str() + 1, nullptr, 0); }

        // return a copy of users
        [[nodiscard]] auto users() const { return use->users; }

        void swap(Value *other) {
            std::swap(use->value, other->use->value);
            std::swap(use, other->use);
        }

        inline void moveTo(Value *other);
    };

    /**
     * User is the base class for all mir Value which uses other Values.
     */
    class User : public Value {
        /**
         * Use shared pointer here to avoid 'use after delete'. <br>
         */
        std::vector<std::shared_ptr<Use>> operands;

        void reInsertOperandsUser() {
            for (auto &operand: operands)
                operand->users.insert(this);
        }

    protected:
        void addOperand(const Value *value) {
            operands.push_back(value->use);
            value->use->users.insert(this);
        }

        void eraseOperand(int _first, int _end) {
            for (auto i = _first; i != _end; ++i) {
                auto &&operand = operands[i];
                operand->users.erase(this);
            }
            operands.erase(operands.cbegin() + _first, operands.cbegin() + _end);
            reInsertOperandsUser();
        }

        int findOperand(const Value *value) const {
            auto it = std::find_if(operands.begin(), operands.end(),
                                   [&value](auto &&use) { return use->value == value; });
            return static_cast<int>(it - operands.begin());
        }

    public:
        template<typename... Args>
        explicit User(pType type, Args... args) : Value(type), operands{args->use...} {
            reInsertOperandsUser();
        }

        explicit User(pType type, const std::vector<Value *> &args) : Value(type) {
            for (auto arg: args) addOperand(arg);
        }

        User(const User &user) : Value(user), operands(user.operands) {
            reInsertOperandsUser();
        }

        User(User &&) = default;

        ~User() override {
            for (auto &operand: operands)
                operand->users.erase(this);
        }

        template<typename R = Value>
        [[nodiscard]] R *getOperand(int i) const { return static_cast<R *>(operands[i]->value); }

        [[nodiscard]] auto getNumOperands() const { return operands.size(); }

        void substituteOperand(int pos, Value *_new) {
            operands[pos]->users.erase(this);
            operands[pos] = _new->use;
            reInsertOperandsUser();
        }

        void substituteOperand(Value *_old, Value *_new) {
            for (int i = 0; i < operands.size(); ++i) {
                if (operands[i]->value != _old) continue;
                substituteOperand(i, _new);
            }
        }

        /**
         * This function is virtual because some users may return sorted operands.
         * @return A copy of operands
         */
        [[nodiscard]] virtual std::vector<Value *> getOperands() const {
            std::vector<Value *> ret;
            std::transform(operands.begin(), operands.end(), std::back_inserter(ret),
                           [](auto &&use) { return use->value; });
            return ret;
        }
    };

    inline bool Value::isUsed() const {
        if (use->users.empty()) return false;
        if (use->users.size() > 1) return true;
        if (auto self = dynamic_cast<const User *>(this);
            use->users.count(const_cast<User *>(self)))
            return false;
        return true;
    }

    inline void Value::moveTo(Value *other) {
        if (this == other) return;
        if (other->use->users.empty()) return swap(other);
        for (auto &&user: users())
            user->substituteOperand(this, other);
        assert(use->users.empty());
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
