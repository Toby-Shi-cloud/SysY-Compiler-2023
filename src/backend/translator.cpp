//
// Created by toby on 2023/11/5.
//

#include "translator.h"

namespace backend {
    static inline void flatten(mir::Literal *literal, std::vector<int> &result) {
        if (auto v = dynamic_cast<mir::IntegerLiteral *>(literal)) {
            return result.push_back(v->value);
        }
        auto arr = dynamic_cast<mir::ArrayLiteral *>(literal);
        assert(arr);
        for (auto ele: arr->values) {
            flatten(ele, result);
        }
    }

    static inline std::vector<int> flatten(mir::Literal *literal) {
        std::vector<int> result;
        flatten(literal, result);
        return result;
    }
}

namespace backend {
    void Translator::translateRetInst(mir::Instruction::ret *retInst) {
        if (auto v = retInst->getReturnValue(); v && curFunc != mipsModule->main.get()) {
            if (auto imm = dynamic_cast<mir::IntegerLiteral *>(v))
                // li $v0, v
                curBlock->instructions.push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::ORI, mips::PhyRegister::get("$v0"),
                        mips::PhyRegister::get("$zero"), imm->value));
            else if (auto reg = dynamic_cast<mips::rRegister>(oMap[v]))
                // move $v0, v
                curBlock->instructions.push_back(std::make_unique<mips::BinaryRInst>(
                        mips::Instruction::Ty::OR, mips::PhyRegister::get("$v0"),
                        reg, mips::PhyRegister::get("$zero")));
            else
                assert(false);
        }
        curBlock->instructions.push_back(std::make_unique<mips::JumpInst>(
                mips::Instruction::Ty::J, curFunc->exitB->label.get()));
    }

    void Translator::translateBranchInst(mir::Instruction::br *brInst) {
        if (!brInst->hasCondition()) {
            curBlock->instructions.push_back(std::make_unique<mips::JumpInst>(
                    mips::Instruction::Ty::J, bMap[brInst->getTarget()]->label.get()));
        } else {
            curBlock->instructions.push_back(std::make_unique<mips::BranchInst>(
                    mips::Instruction::Ty::BEQ, mips::PhyRegister::get("$zero"),
                    bMap[brInst->getIfFalse()]->label.get()));
            curBlock->instructions.push_back(std::make_unique<mips::JumpInst>(
                    mips::Instruction::Ty::J, bMap[brInst->getIfTrue()]->label.get()));
        }
    }

    template<mir::Instruction::InstrTy ty>
    // ty = ADD, SUB, MUL, UDIV, SDIV, UREM, SREM, SHL, LSHR, ASHR, AND, OR, XOR
    void Translator::translateBinaryInst(mir::Instruction::_binary_instruction<ty> *binInst) {
        constexpr std::pair<mips::Instruction::Ty, mips::Instruction::Ty> mipsTys[] = {
                {mips::Instruction::Ty::ADDU, mips::Instruction::Ty::ADDIU},
                {mips::Instruction::Ty::SUBU, mips::Instruction::Ty::ADDIU},
                {mips::Instruction::Ty::MUL,  {}},
                {mips::Instruction::Ty::DIVU, {}},
                {mips::Instruction::Ty::DIV,  {}},
                {mips::Instruction::Ty::DIVU, {}},
                {mips::Instruction::Ty::DIV,  {}},
                {mips::Instruction::Ty::SLLV, mips::Instruction::Ty::SLL},
                {mips::Instruction::Ty::SRLV, mips::Instruction::Ty::SRL},
                {mips::Instruction::Ty::SRAV, mips::Instruction::Ty::SRA},
                {mips::Instruction::Ty::AND,  mips::Instruction::Ty::ANDI},
                {mips::Instruction::Ty::OR,   mips::Instruction::Ty::ORI},
                {mips::Instruction::Ty::XOR,  mips::Instruction::Ty::XORI},
        };
        constexpr auto mipsTy = mipsTys[ty - mir::Instruction::InstrTy::ADD];
        constexpr auto mipsTyV = mipsTy.first;
        constexpr auto mipsTyI = mipsTy.second;
        auto dst = curFunc->newVirRegister();
        put(binInst, dst);
        // lhs must be register
        auto lhs = getRegister(binInst->getLhs());
        assert(lhs);
        if (auto literal = dynamic_cast<mir::IntegerLiteral *>(binInst->getRhs());
                literal && mipsTyI != mips::Instruction::Ty::NOP) {
            // rhs is immediate
            int imm = literal->value;
            if constexpr (ty == mir::Instruction::SUB) imm = -imm; // use addiu instead
            curBlock->instructions.push_back(std::make_unique<mips::BinaryIInst>(
                    mipsTyI, dst, lhs, imm));
            return;
        }
        auto rhs = getRegister(binInst->getRhs());
        assert(rhs);
        if constexpr (ty >= mir::Instruction::UDIV && ty <= mir::Instruction::SREM) {
            // division is complex...
            curBlock->instructions.push_back(std::make_unique<mips::BinaryRInst>(
                    mipsTyV, lhs, rhs));
            if constexpr (ty <= mir::Instruction::SDIV) { // div
                curBlock->instructions.push_back(std::make_unique<mips::MoveInst>(
                        mips::Instruction::Ty::MFLO, dst));
            } else { // rem
                curBlock->instructions.push_back(std::make_unique<mips::MoveInst>(
                        mips::Instruction::Ty::MFHI, dst));
            }
        } else {
            curBlock->instructions.push_back(std::make_unique<mips::BinaryRInst>(
                    mipsTyV, dst, lhs, rhs));
        }
    }

    void Translator::translateAllocaInst(mir::Instruction::alloca_ *allocaInst) {
        //TODO
    }

    void Translator::translateLoadInst(mir::Instruction::load *loadInst) {
        auto dst = curFunc->newVirRegister();
        put(loadInst, dst);
        auto ptr = translateOperand(loadInst->getPointerOperand());
        if (auto label = dynamic_cast<mips::rLabel>(ptr)) {
            curBlock->instructions.push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, dst, label));
        } else if (auto reg = dynamic_cast<mips::rRegister>(ptr)) {
            curBlock->instructions.push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, dst, reg, 0));
        } else {
            assert(false);
        }
    }

    void Translator::translateStoreInst(mir::Instruction::store *storeInst) {
        //TODO
    }

    void Translator::translateGetPtrInst(mir::Instruction::getelementptr *getPtrInst) {
        //TODO
    }

    template<mir::Instruction::InstrTy ty>
    void Translator::translateConversionInst(mir::Instruction::_conversion_instruction<ty> *convInst) {
        //TODO
    }

    void Translator::translateIcmpInst(mir::Instruction::icmp *icmpInst) {
        //TODO
    }

    void Translator::translatePhiInst(mir::Instruction::phi *phiInst) {
        //TODO
    }

    void Translator::translateCallInst(mir::Instruction::call *callInst) {
        //TODO
    }

    void Translator::translateFunction(mir::Function *mirFunction) {
        bool isMain = mirFunction->getName() == "@main";
        std::string name = mirFunction->getName().substr(1);
        unsigned argSize = 4 * mirFunction->getType()->getFunctionParamCount();
        curFunc = new mips::Function{std::move(name), 8, argSize};
        fMap[mirFunction] = curFunc;
        if (isMain) mipsModule->main = mips::pFunction(curFunc);
        else mipsModule->functions.emplace_back(curFunc);

        // arguments
        curFunc->blocks.emplace_back(new mips::Block(curFunc));
        for (int i = 0; i < mirFunction->args.size(); ++i) {
            auto reg = curFunc->newVirRegister();
            if (i < 4) {
                // move %vr, $ax
                curFunc->blocks[0]->instructions.push_back(std::make_unique<mips::BinaryRInst>(
                        mips::Instruction::Ty::OR, reg,
                        mips::PhyRegister::get("$a" + std::to_string(i)),
                        mips::PhyRegister::get("$zero")));
            } else {
                // lw %vr, x($fp)
                curFunc->blocks[0]->instructions.push_back(std::make_unique<mips::LoadInst>(
                        mips::Instruction::Ty::LW, reg,
                        mips::PhyRegister::get("$fp"), (i - 4) * 4));
            }
            put(mirFunction->args[i], reg);
        }

        // blocks
        for (auto bb: mirFunction->bbs)
            translateBasicBlock(bb);
        assert(curFunc != nullptr);
        assert(curFunc->allocaSize % 4 == 0);

        // startB
        // sw $ra, -4($sp)
        curFunc->startB->instructions.push_back(std::make_unique<mips::StoreInst>(
                mips::Instruction::Ty::SW, mips::PhyRegister::get("$ra"),
                mips::PhyRegister::get("$sp"), -4));
        // sw $fp, -8($sp)
        curFunc->startB->instructions.push_back(std::make_unique<mips::StoreInst>(
                mips::Instruction::Ty::SW, mips::PhyRegister::get("$fp"),
                mips::PhyRegister::get("$sp"), -8));
        // move $fp, $sp
        curFunc->startB->instructions.push_back(std::make_unique<mips::BinaryRInst>(
                mips::Instruction::Ty::OR, mips::PhyRegister::get("$fp"),
                mips::PhyRegister::get("$sp"), mips::PhyRegister::get("$zero")));
        // addiu $sp, $sp, -allocaSize
        curFunc->startB->instructions.push_back(std::make_unique<mips::BinaryIInst>(
                mips::Instruction::Ty::ADDIU, mips::PhyRegister::get("$sp"),
                mips::PhyRegister::get("$sp"), -curFunc->allocaSize));

        // exitB
        if (isMain) {
            // syscall 10
            curFunc->exitB->instructions.push_back(std::make_unique<mips::SyscallInst>(
                    mips::SyscallInst::SyscallId::ExitProc));
        } else {
            // move $sp, $fp
            curFunc->exitB->instructions.push_back(std::make_unique<mips::BinaryRInst>(
                    mips::Instruction::Ty::OR, mips::PhyRegister::get("$sp"),
                    mips::PhyRegister::get("$fp"), mips::PhyRegister::get("$zero")));
            // lw $fp, -8($sp)
            curFunc->exitB->instructions.push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, mips::PhyRegister::get("$fp"),
                    mips::PhyRegister::get("$sp"), -8));
            // lw $ra, -4($sp)
            curFunc->exitB->instructions.push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, mips::PhyRegister::get("$ra"),
                    mips::PhyRegister::get("$sp"), -4));
            // jr $ra
            curFunc->exitB->instructions.push_back(std::make_unique<mips::JumpInst>(
                    mips::Instruction::Ty::JR, mips::PhyRegister::get("$ra")));
        }

        // labels
        for (auto &block: curFunc->blocks) {
            lMap[block->label.get()] = block.get();
        }
        lMap[curFunc->startB->label.get()] = curFunc->startB.get();
        lMap[curFunc->exitB->label.get()] = curFunc->exitB.get();
    }

    void Translator::translateBasicBlock(mir::BasicBlock *mirBlock) {
        curBlock = new mips::Block("$BB." + curFunc->label->name + "." + mirBlock->getName().substr(1), curFunc);
        bMap[mirBlock] = curBlock;
        curFunc->blocks.emplace_back(curBlock);
        for (auto inst: mirBlock->instructions)
            translateInstruction(inst);
        //TODO
    }

    void Translator::translateInstruction(mir::Instruction *mirInst) {
        switch (mirInst->instrTy) {
            case mir::Instruction::RET:
                return translateRetInst(dynamic_cast<mir::Instruction::ret *>(mirInst));
            case mir::Instruction::BR:
                return translateBranchInst(dynamic_cast<mir::Instruction::br *>(mirInst));
            case mir::Instruction::ADD:
                return translateBinaryInst(dynamic_cast<mir::Instruction::add *>(mirInst));
            case mir::Instruction::SUB:
                return translateBinaryInst(dynamic_cast<mir::Instruction::sub *>(mirInst));
            case mir::Instruction::MUL:
                return translateBinaryInst(dynamic_cast<mir::Instruction::mul *>(mirInst));
            case mir::Instruction::UDIV:
                return translateBinaryInst(dynamic_cast<mir::Instruction::udiv *>(mirInst));
            case mir::Instruction::SDIV:
                return translateBinaryInst(dynamic_cast<mir::Instruction::sdiv *>(mirInst));
            case mir::Instruction::UREM:
                return translateBinaryInst(dynamic_cast<mir::Instruction::urem *>(mirInst));
            case mir::Instruction::SREM:
                return translateBinaryInst(dynamic_cast<mir::Instruction::srem *>(mirInst));
            case mir::Instruction::SHL:
                return translateBinaryInst(dynamic_cast<mir::Instruction::shl *>(mirInst));
            case mir::Instruction::LSHR:
                return translateBinaryInst(dynamic_cast<mir::Instruction::lshr *>(mirInst));
            case mir::Instruction::ASHR:
                return translateBinaryInst(dynamic_cast<mir::Instruction::ashr *>(mirInst));
            case mir::Instruction::AND:
                return translateBinaryInst(dynamic_cast<mir::Instruction::and_ *>(mirInst));
            case mir::Instruction::OR:
                return translateBinaryInst(dynamic_cast<mir::Instruction::or_ *>(mirInst));
            case mir::Instruction::XOR:
                return translateBinaryInst(dynamic_cast<mir::Instruction::xor_ *>(mirInst));
            case mir::Instruction::ALLOCA:
                return translateAllocaInst(dynamic_cast<mir::Instruction::alloca_ *>(mirInst));
            case mir::Instruction::LOAD:
                return translateLoadInst(dynamic_cast<mir::Instruction::load *>(mirInst));
            case mir::Instruction::STORE:
                return translateStoreInst(dynamic_cast<mir::Instruction::store *>(mirInst));
            case mir::Instruction::GETELEMENTPTR:
                return translateGetPtrInst(dynamic_cast<mir::Instruction::getelementptr *>(mirInst));
            case mir::Instruction::TRUNC:
                return translateConversionInst(dynamic_cast<mir::Instruction::trunc *>(mirInst));
            case mir::Instruction::ZEXT:
                return translateConversionInst(dynamic_cast<mir::Instruction::zext *>(mirInst));
            case mir::Instruction::SEXT:
                return translateConversionInst(dynamic_cast<mir::Instruction::sext *>(mirInst));
            case mir::Instruction::ICMP:
                return translateIcmpInst(dynamic_cast<mir::Instruction::icmp *>(mirInst));
            case mir::Instruction::PHI:
                return translatePhiInst(dynamic_cast<mir::Instruction::phi *>(mirInst));
            case mir::Instruction::CALL:
                return translateCallInst(dynamic_cast<mir::Instruction::call *>(mirInst));
        }
    }

    void Translator::translateGlobalVar(mir::GlobalVar *mirVar) {
        mips::rGlobalVar result;
        std::string name;
        if (mirVar->unnamed) name = mirVar->getName(), name[0] = '$';
        else name = mirVar->getName().substr(1);
        if (mirVar->init == nullptr)
            result = new mips::GlobalVar{std::move(name), false, false, false,
                                         (unsigned) mirVar->getType()->size(), {}};
        else if (auto str = dynamic_cast<mir::StringLiteral *>(mirVar->init))
            result = new mips::GlobalVar{std::move(name), true, true, true,
                                         (unsigned) mirVar->getType()->size(), str->value};
        else
            result = new mips::GlobalVar{std::move(name), true, false, false,
                                         (unsigned) mirVar->getType()->size(), flatten(mirVar->init)};
        gMap[mirVar] = result;
        mipsModule->globalVars.emplace_back(result);
    }

    mips::rOperand Translator::translateOperand(mir::Value *mirValue) {
        if (auto imm = dynamic_cast<mir::IntegerLiteral *>(mirValue)) {
            auto reg = curFunc->newVirRegister();
            curBlock->instructions.push_back(std::make_unique<mips::BinaryIInst>(
                    mips::Instruction::Ty::ORI, reg,
                    mips::PhyRegister::get("$zero"), imm->value));
            return reg;
        } else if (auto op = oMap.find(mirValue); op != oMap.end()) {
            return op->second;
        } else if (auto gv = dynamic_cast<mir::GlobalVar *>(mirValue)) {
            return gMap[gv]->label.get();
        } else if (auto bb = dynamic_cast<mir::BasicBlock *>(mirValue)) {
            return bMap[bb]->label.get();
        } else if (auto func = dynamic_cast<mir::Function *>(mirValue)) {
            return fMap[func]->label.get();
        } else {
            assert(false);
        }
    }
}
