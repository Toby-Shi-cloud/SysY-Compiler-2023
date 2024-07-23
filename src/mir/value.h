//
// Created by toby on 2023/10/5.
//

#ifndef COMPILER_MIR_VALUE_H
#define COMPILER_MIR_VALUE_H

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "../enum.h"
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

using value_map_t = std::unordered_map<const Value *, Value *>;
using magic_enum::lowercase::operator<<;
}  // namespace mir

// Values
namespace mir {
/**
 * Value is the base class for all mir values.
 */
class Value {
    friend User;
    /**
     * Use shared pointer here to avoid 'use after delete'. <br>
     */
    std::shared_ptr<Use> use;

 public:
    const pType type;
    std::string name;

 public:
    Value(pType type, std::string name) : type(type), name(std::move(name)), use(new Use{this}) {}
    explicit Value(pType type) : Value(type, "<anonymous>") {}
    Value(const Value &value) : Value(value.type, value.name) {}
    Value(Value &&) = default;
    virtual ~Value() = default;
    Value &operator=(const Value &) = delete;
    Value &operator=(Value &&) = delete;

    [[nodiscard]] virtual bool isConstLVal() const { return false; }
    [[nodiscard]] inline bool isUsed() const;
    [[nodiscard]] auto getId() const { return std::stoi(name.c_str() + 1); }

    // return a copy of users
    [[nodiscard]] auto users() const { return use->users; }

    // An inline value will be deleted by the last user.
    [[nodiscard]] virtual size_t *inlineRefCounter() const { return nullptr; }

    void swap(Value *other) {
        std::swap(use->value, other->use->value);
        std::swap(use, other->use);
    }

    inline void moveTo(Value *other);

    template <typename Func>
    void moveTo(Value *other, Func &&pred);
};

/**
 * User is the base class for all mir Value which uses other Values.
 */
class User : public Value {
    /**
     * Use shared pointer here to avoid 'use after delete'. <br>
     */
    std::vector<std::weak_ptr<Use>> operands;

    void reInsertOperandsUser() {
        for (auto &operand : operands) operand.lock()->users.insert(this);
    }

 protected:
    void addOperand(const Value *value) {
        operands.push_back(value->use);
        value->use->users.insert(this);
    }

    void eraseOperand(int _first, int _end) {
        for (auto i = _first; i != _end; ++i) {
            auto &&operand = operands[i];
            operand.lock()->users.erase(this);
        }
        operands.erase(operands.cbegin() + _first, operands.cbegin() + _end);
        reInsertOperandsUser();
    }

    int findOperand(const Value *value) const {
        auto it =
            std::find_if(operands.begin(), operands.end(), [&value](auto &&use) { return use.lock()->value == value; });
        return static_cast<int>(it - operands.begin());
    }

 public:
    template <typename... Args>
    explicit User(pType type, Args... args) : Value(type), operands{args->use...} {
        reInsertOperandsUser();
    }

    explicit User(pType type, const std::vector<Value *> &args) : Value(type) {
        for (auto arg : args) addOperand(arg);
    }

    User(const User &user) : Value(user), operands(user.operands) { reInsertOperandsUser(); }

    User(User &&) = default;

    ~User() override {
        for (auto &operand : operands) {
            if (auto op = operand.lock()) {
                op->users.erase(this);
                if (auto counter = op->value->inlineRefCounter()) {
                    if (*counter == 0 && op->users.empty()) delete op->value;
                }
            }
        }
    }

    template <typename R = Value>
    [[nodiscard]] R *getOperand(int i) const {
        return static_cast<R *>(operands[i].lock()->value);
    }

    [[nodiscard]] auto getNumOperands() const { return operands.size(); }

    void substituteOperand(int pos, Value *_new) {
        operands[pos].lock()->users.erase(this);
        operands[pos] = _new->use;
        reInsertOperandsUser();
    }

    void substituteOperand(Value *_old, Value *_new) {
        for (int i = 0; i < operands.size(); ++i) {
            if (getOperand(i) != _old) continue;
            substituteOperand(i, _new);
        }
    }

    void substituteOperands(const value_map_t &map) {
        for (int i = 0; i < operands.size(); ++i)
            if (map.count(getOperand(i))) substituteOperand(i, map.at(getOperand(i)));
    }

    /**
     * This function is virtual because some users may return sorted operands.
     * @return A copy of operands
     */
    [[nodiscard]] virtual std::vector<Value *> getOperands() const {
        std::vector<Value *> ret;
        std::transform(operands.begin(), operands.end(), std::back_inserter(ret),
                       [](auto &&use) { return use.lock()->value; });
        return ret;
    }
};

inline bool Value::isUsed() const {
    if (use->users.empty()) return false;
    if (use->users.size() > 1) return true;
    if (auto self = dynamic_cast<const User *>(this); use->users.count(const_cast<User *>(self))) return false;
    return true;
}

inline void Value::moveTo(Value *other) {
    if (this == other) return;
    if (other->use->users.empty()) return swap(other);
    for (auto &&user : users()) user->substituteOperand(this, other);
    assert(use->users.empty());
}

template <typename Func>
void Value::moveTo(Value *other, Func &&pred) {
    if (this == other) return;
    for (auto &&user : users())
        if (std::invoke(pred, user)) user->substituteOperand(this, other);
}

inline std::ostream &operator<<(std::ostream &os, const Value &value) {
    if (value.type->isArrayTy())
        os << "ptr";
    else
        os << value.type;
    return os << " " << value.name;
}

inline void try_delete(Value *value) {
    if (auto counter = value->inlineRefCounter())
        if (--*counter == 0 && !value->isUsed()) delete value;
}

template <typename T>
std::enable_if_t<std::is_base_of_v<Value, T>, std::ostream> &operator<<(std::ostream &os, const T *value) {
    return os << *value;
}
}  // namespace mir

#endif  // COMPILER_MIR_VALUE_H
