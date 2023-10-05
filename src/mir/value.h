//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_VALUE_H
#define COMPILER_VALUE_H

#include "type.h"
#include <string>
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
         * Value must own the name.
         */
        const char *name = nullptr;

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
        explicit Value(pType type) : use(new Use{this}), type(type) {}

        virtual ~Value() { delete use, delete name; }

        void setName(const char *str) { delete name, name = str ? strdup(str) : nullptr; }

        void setName(const std::string &str) { setName(str.c_str()); }

        [[nodiscard]] inline bool hasName() const { return name; }

        [[nodiscard]] inline const char *getName() const { return hasName() ? name : "<anonymous>"; }

        [[nodiscard]] inline pType getType() const { return type; }

        [[nodiscard]] inline bool isUsed() const { return !use->users.empty(); }
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
        explicit User(pType type, Args... args) : Value(type), operands{(args->use)...} {
            for (auto operand: operands) operand->users.insert(this);
        }

        ~User() override {
            for (auto operand: operands) operand->users.erase(this);
        }

        [[nodiscard]] const Value *getOperand(int i) const { return operands[i]->value; }

        [[nodiscard]] auto getNumOperands() const { return operands.size(); }
    };
}

#endif //COMPILER_VALUE_H
