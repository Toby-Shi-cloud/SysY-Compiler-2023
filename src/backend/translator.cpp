//
// Created by toby on 2023/11/5.
//

#include <queue>
#include "reg_alloca.h"
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
    template<mips::Instruction::Ty rTy, mips::Instruction::Ty iTy>
    mips::rRegister Translator::createBinaryInstHelper(mips::rRegister lhs, mir::Value *rhs) {
        auto dst = curFunc->newVirRegister();
        if (auto literal = dynamic_cast<mir::IntegerLiteral *>(rhs);
                literal && literal->value < 1 << 15 && literal->value >= -(1 << 15)) {
            // rhs is immediate
            int imm = literal->value;
            if constexpr (rTy == mips::Instruction::Ty::SUBU) imm = -imm; // use addiu instead
            curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                    iTy, dst, lhs, imm));
        } else {
            auto rop = getRegister(rhs);
            assert(rop);
            curBlock->push_back(std::make_unique<mips::BinaryRInst>(
                    rTy, dst, lhs, rop));
        }
        return dst;
    }

    template<mir::Instruction::InstrTy ty>
    // ty = ADD, SUB, MUL, UDIV, SDIV, UREM, SREM, SHL, LSHR, ASHR, AND, OR, XOR
    mips::rRegister Translator::translateBinaryInstHelper(mips::rRegister lhs, mir::Value *rhs) {
        constexpr std::pair<mips::Instruction::Ty, mips::Instruction::Ty> mipsTys[] = {
                {mips::Instruction::Ty::ADDU, mips::Instruction::Ty::ADDIU},
                {mips::Instruction::Ty::SUBU, mips::Instruction::Ty::ADDIU},
                {mips::Instruction::Ty::MUL,  mips::Instruction::Ty::MUL},
                {mips::Instruction::Ty::DIVU, mips::Instruction::Ty::DIVU},
                {mips::Instruction::Ty::DIV,  mips::Instruction::Ty::DIV},
                {mips::Instruction::Ty::REMU, mips::Instruction::Ty::REMU},
                {mips::Instruction::Ty::REM,  mips::Instruction::Ty::REM},
                {mips::Instruction::Ty::SLLV, mips::Instruction::Ty::SLL},
                {mips::Instruction::Ty::SRLV, mips::Instruction::Ty::SRL},
                {mips::Instruction::Ty::SRAV, mips::Instruction::Ty::SRA},
                {mips::Instruction::Ty::AND,  mips::Instruction::Ty::ANDI},
                {mips::Instruction::Ty::OR,   mips::Instruction::Ty::ORI},
                {mips::Instruction::Ty::XOR,  mips::Instruction::Ty::XORI},
        };
        constexpr auto mipsTy = mipsTys[ty - mir::Instruction::InstrTy::ADD];
        constexpr auto mipsTyR = mipsTy.first;
        constexpr auto mipsTyI = mipsTy.second;
        return createBinaryInstHelper<mipsTyR, mipsTyI>(lhs, rhs);
    }

    mips::rRegister Translator::addressCompute(mips::rAddress addr) {
        auto dst = curFunc->newVirRegister();
        if (addr->label) {
            curBlock->push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LA, dst, addr->label));
            if (addr->base != mips::PhyRegister::get(0))
                curBlock->push_back(std::make_unique<mips::BinaryRInst>(
                        mips::Instruction::Ty::ADDU, dst, dst, addr->base));
            if (addr->offset)
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::ADDIU, dst, dst, addr->offset));
        } else {
            curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                    mips::Instruction::Ty::ADDIU, dst, addr->base, addr->offset));
        }
        return dst;
    }

    void Translator::translateRetInst(mir::Instruction::ret *retInst) {
        if (auto v = retInst->getReturnValue()) {
            if (auto imm = dynamic_cast<mir::IntegerLiteral *>(v))
                // li $v0, v
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::LI, mips::PhyRegister::get("$v0"), imm->value));
            else if (auto reg = dynamic_cast<mips::rRegister>(oMap[v]))
                // move $v0, v
                curBlock->push_back(std::make_unique<mips::MoveInst>(
                        mips::PhyRegister::get("$v0"), reg));
            else
                assert(false);
        }
        curBlock->push_back(std::make_unique<mips::JumpInst>(
                mips::Instruction::Ty::J, curFunc->exitB->label.get()));
    }

    void Translator::translateBranchInst(mir::Instruction::br *brInst) {
        if (!brInst->hasCondition()) {
            curBlock->push_back(std::make_unique<mips::JumpInst>(
                    mips::Instruction::Ty::J, bMap[brInst->getTarget()]->label.get()));
        } else {
            auto cond = dynamic_cast<mir::Instruction::icmp *>(brInst->getCondition());
            if (cond->cond == mir::Instruction::icmp::EQ) {
                auto lhs = getRegister(cond->getLhs());
                auto rhs = getRegister(cond->getRhs());
                assert(lhs && rhs);
                curBlock->push_back(std::make_unique<mips::BranchInst>(
                        mips::Instruction::Ty::BNE, lhs, rhs,
                        bMap[brInst->getIfFalse()]->label.get()));
            } else if (cond->cond == mir::Instruction::icmp::NE) {
                auto lhs = getRegister(cond->getLhs());
                auto rhs = getRegister(cond->getRhs());
                assert(lhs && rhs);
                curBlock->push_back(std::make_unique<mips::BranchInst>(
                        mips::Instruction::Ty::BEQ, lhs, rhs,
                        bMap[brInst->getIfFalse()]->label.get()));
            } else {
                auto rhs = getRegister(cond);
                curBlock->push_back(std::make_unique<mips::BranchInst>(
                        mips::Instruction::Ty::BEQ, rhs, mips::PhyRegister::get("$zero"),
                        bMap[brInst->getIfFalse()]->label.get()));
            }
            curBlock->push_back(std::make_unique<mips::JumpInst>(
                    mips::Instruction::Ty::J, bMap[brInst->getIfTrue()]->label.get()));
        }
    }

    template<mir::Instruction::InstrTy ty>
    void Translator::translateBinaryInst(mir::Instruction::_binary_instruction<ty> *binInst) {
        auto lhs = getRegister(binInst->getLhs());
        assert(lhs);
        auto reg = translateBinaryInstHelper<ty>(lhs, binInst->getRhs());
        put(binInst, reg);
    }

    void Translator::translateAllocaInst(mir::Instruction::alloca_ *allocaInst) {
        auto alloca_size = allocaInst->getType()->size();
        assert(alloca_size % 4 == 0);
        curFunc->allocaSize += alloca_size;
        auto addr = curFunc->newAddress(mips::PhyRegister::get("$fp"), -curFunc->allocaSize);
        put(allocaInst, addr);
    }

    void Translator::translateLoadInst(mir::Instruction::load *loadInst) {
        auto dst = curFunc->newVirRegister();
        put(loadInst, dst);
        auto ptr = translateOperand(loadInst->getPointerOperand());
        if (auto label = dynamic_cast<mips::rLabel>(ptr)) {
            curBlock->push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, dst, label));
        } else if (auto reg = dynamic_cast<mips::rRegister>(ptr)) {
            curBlock->push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, dst, reg, 0));
        } else if (auto addr = dynamic_cast<mips::rAddress>(ptr)) {
            curBlock->push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, dst, addr));
        } else {
            assert(false);
        }
    }

    void Translator::translateStoreInst(mir::Instruction::store *storeInst) {
        auto src = getRegister(storeInst->getSrc());
        auto dst = translateOperand(storeInst->getDest());
        if (auto label = dynamic_cast<mips::rLabel>(dst)) {
            curBlock->push_back(std::make_unique<mips::StoreInst>(
                    mips::Instruction::Ty::SW, src, label));
        } else if (auto reg = dynamic_cast<mips::rRegister>(dst)) {
            curBlock->push_back(std::make_unique<mips::StoreInst>(
                    mips::Instruction::Ty::SW, src, reg, 0));
        } else if (auto addr = dynamic_cast<mips::rAddress>(dst)) {
            curBlock->push_back(std::make_unique<mips::StoreInst>(
                    mips::Instruction::Ty::SW, src, addr));
        } else {
            assert(false);
        }
    }

    void Translator::translateGetPtrInst(mir::Instruction::getelementptr *getPtrInst) {
        auto deduce_type = getPtrInst->getIndexTy();
        auto addr = getAddress(getPtrInst->getPointerOperand());
        for (int i = 0; i < getPtrInst->getNumIndices(); i++) {
            if (i != 0) deduce_type = deduce_type->getBase();
            auto value = getPtrInst->getIndexOperand(i);
            int deduce_size = (int) deduce_type->size();
            if (auto imm = dynamic_cast<mir::IntegerLiteral *>(value)) {
                addr = curFunc->newAddress(addr->base, addr->offset + imm->value * deduce_size, addr->label);
            } else {
                auto reg = getRegister(value);
                auto dst1 = curFunc->newVirRegister();
                auto dst2 = curFunc->newVirRegister();
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::MUL, dst1, reg, deduce_size));
                curBlock->push_back(std::make_unique<mips::BinaryRInst>(
                        mips::Instruction::Ty::ADDU, dst2, addr->base, dst1));
                addr = curFunc->newAddress(dst2, addr->offset, addr->label);
            }
        }
        put(getPtrInst, addr);
    }

    template<mir::Instruction::InstrTy ty>
    void Translator::translateConversionInst(mir::Instruction::_conversion_instruction<ty> *convInst) {
        assert(ty == mir::Instruction::ZEXT);
        auto icmp = dynamic_cast<mir::Instruction::icmp *>(convInst->getValueOperand());
        assert(icmp);
        put(convInst, oMap[icmp]);
    }

    void Translator::translateIcmpInst(mir::Instruction::icmp *icmpInst) {
        auto lhs = icmpInst->getLhs();
        auto rhs = icmpInst->getRhs();
        mips::rRegister reg;
        switch (icmpInst->cond) {
            case mir::Instruction::icmp::EQ:
                reg = translateBinaryInstHelper<mir::Instruction::SUB>(getRegister(lhs), rhs);
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::SLTIU, reg, reg, 1));
                break;
            case mir::Instruction::icmp::NE:
                reg = translateBinaryInstHelper<mir::Instruction::SUB>(getRegister(lhs), rhs);
                curBlock->push_back(std::make_unique<mips::BinaryRInst>(
                        mips::Instruction::Ty::SLTU, reg, mips::PhyRegister::get(0), reg));
                break;
            case mir::Instruction::icmp::UGT:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLTU, mips::Instruction::Ty::SLTIU>(
                        getRegister(rhs), lhs);
                break;
            case mir::Instruction::icmp::UGE:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLTU, mips::Instruction::Ty::SLTIU>(
                        getRegister(lhs), rhs);
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::XORI, reg, reg, 1));
                break;
            case mir::Instruction::icmp::ULT:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLTU, mips::Instruction::Ty::SLTIU>(
                        getRegister(lhs), rhs);
                break;
            case mir::Instruction::icmp::ULE:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLTU, mips::Instruction::Ty::SLTIU>(
                        getRegister(rhs), lhs);
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::XORI, reg, reg, 1));
                break;
            case mir::Instruction::icmp::SGT:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLT, mips::Instruction::Ty::SLTI>(
                        getRegister(rhs), lhs);
                break;
            case mir::Instruction::icmp::SGE:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLT, mips::Instruction::Ty::SLTI>(
                        getRegister(lhs), rhs);
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::XORI, reg, reg, 1));
                break;
            case mir::Instruction::icmp::SLT:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLT, mips::Instruction::Ty::SLTI>(
                        getRegister(lhs), rhs);
                break;
            case mir::Instruction::icmp::SLE:
                reg = createBinaryInstHelper<mips::Instruction::Ty::SLT, mips::Instruction::Ty::SLTI>(
                        getRegister(rhs), lhs);
                curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                        mips::Instruction::Ty::XORI, reg, reg, 1));
                break;
        }
        put(icmpInst, reg);
    }

    void Translator::translatePhiInst(mir::Instruction::phi *phiInst) {
        // Put a new virtual register to indicate the phi instruction,
        // and we will translate it later.
        if (oMap.count(phiInst)) return;
        put(phiInst, curFunc->newVirRegister());
    }

    void Translator::translateCallInst(mir::Instruction::call *callInst) {
        auto func = callInst->getFunction();
        if (func == mir::Function::getint) {
            auto dst = curFunc->newVirRegister();
            curBlock->push_back(std::make_unique<mips::SyscallInst>(
                    mips::SyscallInst::SyscallId::ReadInteger));
            curBlock->push_back(std::make_unique<mips::MoveInst>(
                    dst, mips::PhyRegister::get("$v0")));
            put(callInst, dst);
        } else if (func == mir::Function::putint) {
            auto src = getRegister(callInst->getArg(0));
            curBlock->push_back(std::make_unique<mips::MoveInst>(
                    mips::PhyRegister::get("$a0"), src));
            curBlock->push_back(std::make_unique<mips::SyscallInst>(
                    mips::SyscallInst::SyscallId::PrintInteger));
        } else if (func == mir::Function::putch) {
            auto src = getRegister(callInst->getArg(0));
            curBlock->push_back(std::make_unique<mips::MoveInst>(
                    mips::PhyRegister::get("$a0"), src));
            curBlock->push_back(std::make_unique<mips::SyscallInst>(
                    mips::SyscallInst::SyscallId::PrintCharacter));
        } else if (func == mir::Function::putstr) {
            if (auto src = getRegister(callInst->getArg(0))) {
                curBlock->push_back(std::make_unique<mips::MoveInst>(
                        mips::PhyRegister::get("$a0"), src));
            } else if (auto addr = getAddress(callInst->getArg(0))) {
                auto reg = addressCompute(addr);
                curBlock->push_back(std::make_unique<mips::MoveInst>(
                        mips::PhyRegister::get("$a0"), reg));
            } else {
                assert(false);
            }
            curBlock->push_back(std::make_unique<mips::SyscallInst>(
                    mips::SyscallInst::SyscallId::PrintString));
        } else {
            auto callee = fMap[func];
            unsigned stack_arg_cnt = std::max(0, (int) callInst->getNumArgs() - 4);
            curFunc->argSize = std::max(curFunc->argSize, stack_arg_cnt * 4);
            for (int i = 0; i < callInst->getNumArgs(); i++) {
                auto reg = getRegister(callInst->getArg(i));
                if (!reg) reg = addressCompute(getAddress(callInst->getArg(i)));
                if (i < 4) {
                    curBlock->push_back(std::make_unique<mips::MoveInst>(
                            mips::PhyRegister::get("$a" + std::to_string(i)), reg));
                } else {
                    curBlock->push_back(std::make_unique<mips::StoreInst>(
                            mips::Instruction::Ty::SW, reg,
                            mips::PhyRegister::get("$sp"), (i - 4) * 4));
                }
            }
            curBlock->push_back(std::make_unique<mips::JumpInst>(
                    mips::Instruction::Ty::JAL, callee->label.get()));
            if (callInst->isValue()) {
                auto reg = curFunc->newVirRegister();
                curBlock->push_back(std::make_unique<mips::MoveInst>(
                        reg, mips::PhyRegister::get("$v0")));
                put(callInst, reg);
            }
        }
    }

    void Translator::translateFunction(mir::Function *mirFunction) {
        const bool isMain = mirFunction->isMain();
        const bool isLeaf = mirFunction->isLeaf();
        std::string name = mirFunction->getName().substr(1);
        curFunc = new mips::Function{std::move(name), isMain, isLeaf};
        fMap[mirFunction] = curFunc;
        if (!isLeaf) curFunc->shouldSave.insert(mips::PhyRegister::get("$ra"));
        if (!isMain) curFunc->shouldSave.insert(mips::PhyRegister::get("$fp"));
        if (isMain) mipsModule->main = mips::pFunction(curFunc);
        else mipsModule->functions.emplace_back(curFunc);

        // blocks
        lMap[curFunc->exitB->label.get()] = curFunc->exitB.get();
        for (auto bb: mirFunction->bbs) {
            auto block = new mips::Block(curFunc);
            bMap[bb] = block;
            lMap[block->label.get()] = block;
            block->node = curFunc->blocks.emplace(curFunc->end(), block);
        }

        // arguments
        assert(mirFunction->bbs.front()->predecessors.empty());
        for (int i = 0; i < mirFunction->args.size(); ++i) {
            auto reg = curFunc->newVirRegister();
            if (i < 4) {
                // move %vr, $ax
                curFunc->blocks.front()->push_back(std::make_unique<mips::MoveInst>(
                        reg, mips::PhyRegister::get("$a" + std::to_string(i))));
            } else {
                // lw %vr, x($fp)
                curFunc->blocks.front()->push_back(std::make_unique<mips::LoadInst>(
                        mips::Instruction::Ty::LW, reg,
                        mips::PhyRegister::get("$fp"), (i - 4) * 4));
            }
            put(mirFunction->args[i], reg);
        }

        // translate
        for (auto bb: mirFunction->bbs)
            translateBasicBlock(bb);
        compute_phi(mirFunction);
        assert(curFunc != nullptr);
        assert(curFunc->allocaSize % 4 == 0);
        assert(curFunc->argSize % 4 == 0);

        // reformat blocks & alloca registers
        spliceBlocks();
        for (auto &block: curFunc->blocks)
            compute_pre_suc(block.get());
        register_alloca(curFunc);

        // save registers before function & restore registers
        if (isMain) curFunc->shouldSave.clear(); // save nothing
        compute_func_start();
        if (isMain)
            curFunc->exitB->push_back(std::make_unique<mips::SyscallInst>(
                    mips::SyscallInst::SyscallId::ExitProc));
        else compute_func_exit();

        curFunc->allocName();
    }

    void Translator::translateBasicBlock(mir::BasicBlock *mirBlock) {
        curBlock = bMap[mirBlock];
        for (auto inst: mirBlock->instructions)
            translateInstruction(inst);
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
            curBlock->push_back(std::make_unique<mips::BinaryIInst>(
                    mips::Instruction::Ty::LI, reg, imm->value));
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
            put(mirValue, curFunc->newVirRegister());
            return oMap[mirValue];
        }
    }

    void Translator::compute_phi(mir::Function *mirFunction) {
        using parallel_copy_t = std::vector<std::pair<mips::rRegister, mips::rRegister>>;
        const auto phi2move = [this](const parallel_copy_t &pc) {
            using move_t = mips::MoveInst;
            std::vector<mips::pInstruction> instructions;
            std::unordered_map<mips::rRegister, size_t> inDegree;
            std::unordered_map<mips::rRegister, mips::rRegister> map;
            std::unordered_map<mips::rRegister, std::vector<mips::rRegister>> edges;
            for (auto &[dst, src]: pc) {
                edges[dst].push_back(src);
                inDegree[dst]; // make sure dst in inDegree
                inDegree[src]++;
                map[src] = src;
            }
            std::queue<mips::rRegister> queue;
            for (auto &[r, d]: inDegree)
                if (d == 0) queue.push(r);
            while (!inDegree.empty()) {
                while (!queue.empty()) {
                    auto reg = queue.front();
                    queue.pop(), inDegree.erase(reg);
                    for (auto &v: edges[reg]) {
                        instructions.emplace_back(new move_t{reg, map[v]});
                        if (map[v] == v) {
                            inDegree[v]--;
                            if (inDegree[v] == 0) queue.push(v);
                        }
                    }
                }
                if (inDegree.empty()) break;
                auto &[reg, _] = *std::min_element(inDegree.begin(), inDegree.end(),
                                                   [](auto &&x, auto &&y) { return x.second < y.second; });
                auto vir = curFunc->newVirRegister();
                instructions.emplace_back(new move_t{vir, reg});
                map[reg] = vir;
                queue.push(reg);
            }
            return instructions;
        };

        for (auto &bb: mirFunction->bbs) {
            auto label = bMap[bb]->label.get();
            auto end = bb->phi_end();
            if (bb->instructions.begin() == end) continue;
            std::unordered_map<mir::BasicBlock *, parallel_copy_t> pcs;
            for (auto it = bb->instructions.begin(); it != end; ++it) {
                auto phi = dynamic_cast<mir::Instruction::phi *>(*it);
                assert(phi);
                auto dst = getRegister(phi);
                for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
                    auto [value, block] = phi->getIncomingValue(i);
                    curBlock = bMap[block];
                    auto src = getRegister(value);
                    pcs[block].emplace_back(dst, src);
                }
            }
            for (auto &pre: bb->predecessors) {
                auto block = bMap[pre];
                if (pre->successors.size() > 1) {
                    auto newBlock = new mips::Block(block->parent);
                    auto it = block->node;
                    newBlock->node = curFunc->blocks.emplace(++it, newBlock);
                    lMap[newBlock->label.get()] = newBlock;
                    newBlock->push_back(std::make_unique<mips::JumpInst>(
                            mips::Instruction::Ty::J, label));
                    if (block->fallthroughJump && block->fallthroughJump->getJumpLabel() == label)
                        block->fallthroughJump->setJumpLabel(newBlock->label.get());
                    if (block->conditionalJump && block->conditionalJump->getJumpLabel() == label)
                        block->conditionalJump->setJumpLabel(newBlock->label.get());
                    block = newBlock;
                }
                for (auto &inst: phi2move(pcs[pre]))
                    block->push_back(std::move(inst));
            }
        }
    }

    void Translator::compute_func_start() {
        int cnt = 0;
        auto it = curFunc->blocks.front()->instructions.begin();
        for (auto reg: curFunc->shouldSave) {
            curFunc->blocks.front()->insert(it, std::make_unique<mips::StoreInst>(
                    mips::Instruction::Ty::SW, reg,
                    mips::PhyRegister::get("$sp"), -curFunc->allocaSize - ++cnt * 4));
        }
        // move $fp, $sp
        curFunc->blocks.front()->insert(it, std::make_unique<mips::MoveInst>(
                mips::PhyRegister::get("$fp"), mips::PhyRegister::get("$sp")));
        // addiu $sp, $sp, -(allocaSize+argSize)
        if (-curFunc->allocaSize - curFunc->argSize - cnt * 4 != 0)
            curFunc->blocks.front()->insert(it, std::make_unique<mips::BinaryIInst>(
                    mips::Instruction::Ty::ADDIU, mips::PhyRegister::get("$sp"),
                    mips::PhyRegister::get("$sp"), -curFunc->allocaSize - curFunc->argSize - cnt * 4));
    }

    void Translator::compute_func_exit() {
        // move $sp, $fp
        curFunc->exitB->push_back(std::make_unique<mips::MoveInst>(
                mips::PhyRegister::get("$sp"), mips::PhyRegister::get("$fp")));
        int cnt = 0;
        for (auto reg: curFunc->shouldSave) {
            curFunc->exitB->push_back(std::make_unique<mips::LoadInst>(
                    mips::Instruction::Ty::LW, reg,
                    mips::PhyRegister::get("$sp"), -curFunc->allocaSize - ++cnt * 4));
        }
        // jr $ra
        curFunc->exitB->push_back(std::make_unique<mips::JumpInst>(
                mips::Instruction::Ty::JR, mips::PhyRegister::get("$ra")));
    }

    void Translator::spliceBlocks() {
        constexpr auto pred_if_jal = [](auto &&inst) { return inst->isFuncCall(); };
        for (auto it = curFunc->begin(); it != curFunc->end();) {
            auto &block = *it;
            auto pos = std::find_if(block->instructions.begin(), block->instructions.end(), pred_if_jal);
            if (auto nb = block->spliceFuncCall(pos)) {
                nb->node = curFunc->blocks.emplace(++it, nb);
                it = nb->node;
                lMap[nb->label.get()] = nb;
            } else it++;
        }
    }

    void Translator::relocateBlocks() {
        //TODO
    }

    void Translator::mergeExitBlock() {
        // only merge exitB...
        for (auto pre: curFunc->exitB->predecessors)
            if (pre->conditionalJump == nullptr)
                pre->merge(curFunc->exitB.get());
    }

    void Translator::compute_pre_suc(mips::rBlock block) {
        auto f = [&, this](auto &&inst) {
            if (!inst) return;
            auto label = inst->getJumpLabel();
            assert(label);
            if (auto suc = lMap[label]) {
                block->successors.insert(suc);
                suc->predecessors.insert(block);
            }
        };
        f(block->conditionalJump);
        f(block->fallthroughJump);
    }

    void Translator::optimize(mips::rFunction func) {
        curFunc = func;
        mergeExitBlock();
        relocateBlocks();
        func->allocName();
    }
}
