//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_VALUE_H
#define COMPILER_VALUE_H

#include "type.h"
#include <string>
#include <ostream>
#include <unordered_set>

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

    protected:
        /**
         * Value owns Use. <br>
         * Value must transfer use to another Value
         * when it is deleted, or delete the use itself. <br>
         * To mark the use is transferred, set use to nullptr. <br>
         * To mark the value is never used, set use to empty vector.
         */
        Use *use;

    public:
        explicit Value(pType type, bool isConstant) : use(new Use{this}), type(type), isConstant(isConstant) {}

        explicit Value(pType type) : Value(type, false) {}

        virtual ~Value() { delete use; }

        void setConst(bool constant = true) { isConstant = constant; }

        void setName(std::string str) { name = std::move(str); }

        [[nodiscard]] inline bool hasName() const { return !name.empty(); }

        [[nodiscard]] inline const std::string &getName() const { return hasName() ? name : anonymous; }

        [[nodiscard]] inline pType getType() const { return type; }

        [[nodiscard]] inline bool isUsed() const { return !use->users.empty(); }

        [[nodiscard]] inline bool isConst() const { return isConstant; }
    };

    /**
     * User is the base class for all mir Value which uses other Values.
     */
    class User : public Value {
        /**
         * User does NOT own any Use. <br>
         */
        std::vector<Use *> operands;

    public:
        template<typename... Args>
        explicit User(pType type, Args... args) : Value(type, (args->isConst() && ...)), operands{(args->use)...} {
            for (auto operand: operands) operand->users.insert(this);
        }

        ~User() override {
            for (auto operand: operands) operand->users.erase(this);
        }

        [[nodiscard]] const Value *getOperand(int i) const { return operands[i]->value; }

        [[nodiscard]] auto getNumOperands() const { return operands.size(); }
    };

    inline std::ostream &operator<<(std::ostream &os, const Value &value) {
        return os << value.getType() << " " << value.getName();
    }

    template<typename T>
    inline std::enable_if_t<std::is_base_of_v<Value, T>, std::ostream> &
    operator<<(std::ostream &os, const T *value) {
        return os << *value;
    }
}

#endif //COMPILER_VALUE_H
