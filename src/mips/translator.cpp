//
// Created by toby on 2023/11/5.
//

#include "mips/translator.h"
#include <queue>
#include "mips/opt.h"
#include "mips/reg_alloca.h"
#include "settings.h"

namespace backend::mips {
static void flatten(mir::Literal *literal, std::vector<int> &result) {
    if (auto v = dynamic_cast<mir::IntegerLiteral *>(literal)) return result.push_back(v->value);
    auto arr = dynamic_cast<mir::ArrayLiteral *>(literal);
    assert(arr);
    for (auto ele : arr->values) flatten(ele, result);
}

static std::vector<int> flatten(mir::Literal *literal) {
    std::vector<int> result;
    flatten(literal, result);
    return result;
}
}  // namespace backend::mips

namespace backend::mips {
template <Instruction::Ty rTy, Instruction::Ty iTy>
rRegister Translator::createBinaryInstHelper(rRegister lhs, mir::Value *rhs) {
    auto dst = curFunc->newVirRegister();
    if (auto literal = dynamic_cast<mir::IntegerLiteral *>(rhs);
        literal && literal->value < 1 << 15 && literal->value >= -(1 << 15)) {
        // rhs is immediate
        int imm = literal->value;
        if constexpr (rTy == Instruction::Ty::SUBU) imm = -imm;  // use addiu instead
        curBlock->push_back(std::make_unique<BinaryIInst>(iTy, dst, lhs, imm));
    } else {
        auto rop = getRegister(rhs);
        assert(rop);
        curBlock->push_back(std::make_unique<BinaryRInst>(rTy, dst, lhs, rop));
    }
    return dst;
}

template <mir::Instruction::InstrTy ty>
// ty = ADD, SUB, MUL, UDIV, SDIV, UREM, SREM, SHL, LSHR, ASHR, AND, OR, XOR
rRegister Translator::translateBinaryInstHelper(rRegister lhs, mir::Value *rhs) {
    constexpr std::pair<Instruction::Ty, Instruction::Ty> mipsTys[] = {
        {Instruction::Ty::ADDU, Instruction::Ty::ADDIU},
        {Instruction::Ty::SUBU, Instruction::Ty::ADDIU},
        {/* MUL */},
        {/* UDIV */},
        {/* SDIV */},
        {/* UREM */},
        {/* SREM */},
        {/* FADD */},
        {/* FSUB */},
        {/* FMUL */},
        {/* FDIV */},
        {/* FREM */},
        {Instruction::Ty::SLLV, Instruction::Ty::SLL},
        {Instruction::Ty::SRLV, Instruction::Ty::SRL},
        {Instruction::Ty::SRAV, Instruction::Ty::SRA},
        {Instruction::Ty::AND, Instruction::Ty::ANDI},
        {Instruction::Ty::OR, Instruction::Ty::ORI},
        {Instruction::Ty::XOR, Instruction::Ty::XORI},
    };
    if constexpr (constexpr auto index = ty - mir::Instruction::InstrTy::ADD;
                  mipsTys[index].first != Instruction::Ty::NOP)
        return createBinaryInstHelper<mipsTys[index].first, mipsTys[index].second>(lhs, rhs);
    auto dst = curFunc->newVirRegister();
    auto rhsv = getRegister(rhs);
    if constexpr (ty == mir::Instruction::MUL)
        curBlock->push_back(std::make_unique<BinaryMInst>(Instruction::Ty::MUL, dst, lhs, rhsv));
    else if constexpr (ty == mir::Instruction::UDIV || ty == mir::Instruction::UREM)
        curBlock->push_back(std::make_unique<BinaryMInst>(Instruction::Ty::DIVU, lhs, rhsv));
    else
        curBlock->push_back(std::make_unique<BinaryMInst>(Instruction::Ty::DIV, lhs, rhsv));
    if constexpr (ty == mir::Instruction::SREM || ty == mir::Instruction::UREM)
        curBlock->push_back(std::make_unique<MoveInst>(dst, PhyRegister::HI));
    else if constexpr (ty != mir::Instruction::MUL)
        curBlock->push_back(std::make_unique<MoveInst>(dst, PhyRegister::LO));
    return dst;
}

rRegister Translator::addressCompute(rAddress addr) const {
    if (addr->label == nullptr) {
        auto imm_reg = curFunc->newVirRegister();
        curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::ADDIU, imm_reg,
                                                          addr->base, addr->offset->clone()));
        return imm_reg;
    }
    auto addr_reg = curFunc->newVirRegister();
    auto dst1 = curFunc->newVirRegister();
    auto dst2 = curFunc->newVirRegister();
    curBlock->push_back(std::make_unique<LoadInst>(Instruction::Ty::LA, addr_reg, addr->label));
    curBlock->push_back(
        std::make_unique<BinaryRInst>(Instruction::Ty::ADDU, dst1, addr_reg, addr->base));
    curBlock->push_back(
        std::make_unique<BinaryIInst>(Instruction::Ty::ADDIU, dst2, dst1, addr->offset->clone()));
    return dst2;
}

void Translator::translateRetInst(const mir::Instruction::ret *retInst) {
    if (auto v = retInst->getReturnValue()) {
        if (auto imm = dynamic_cast<mir::IntegerLiteral *>(v))
            // li $v0, v
            curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::LI,
                                                              PhyRegister::get("$v0"), imm->value));
        else if (auto reg = dynamic_cast<rRegister>(oMap[v]))
            // move $v0, v
            curBlock->push_back(std::make_unique<MoveInst>(PhyRegister::get("$v0"), reg));
        else
            assert(false);
    }
    curBlock->push_back(
        std::make_unique<JumpInst>(Instruction::Ty::J, curFunc->exitB->label.get()));
}

void Translator::translateBranchInst(const mir::Instruction::br *brInst) {
    if (!brInst->hasCondition()) {
        curBlock->push_back(
            std::make_unique<JumpInst>(Instruction::Ty::J, bMap[brInst->getTarget()]->label.get()));
    } else {
        if (auto cond = dynamic_cast<mir::Instruction::icmp *>(brInst->getCondition());
            cond->cond == mir::Instruction::icmp::EQ) {
            auto lhs = getRegister(cond->getLhs());
            auto rhs = getRegister(cond->getRhs());
            assert(lhs && rhs);
            curBlock->push_back(std::make_unique<BranchInst>(
                Instruction::Ty::BNE, lhs, rhs, bMap[brInst->getIfFalse()]->label.get()));
        } else if (cond->cond == mir::Instruction::icmp::NE) {
            auto lhs = getRegister(cond->getLhs());
            auto rhs = getRegister(cond->getRhs());
            assert(lhs && rhs);
            curBlock->push_back(std::make_unique<BranchInst>(
                Instruction::Ty::BEQ, lhs, rhs, bMap[brInst->getIfFalse()]->label.get()));
        } else {
            auto rhs = getRegister(cond);
            curBlock->push_back(
                std::make_unique<BranchInst>(Instruction::Ty::BEQ, rhs, PhyRegister::get("$zero"),
                                             bMap[brInst->getIfFalse()]->label.get()));
        }
        curBlock->push_back(
            std::make_unique<JumpInst>(Instruction::Ty::J, bMap[brInst->getIfTrue()]->label.get()));
    }
}

template <mir::Instruction::InstrTy ty>
void Translator::translateBinaryInst(const mir::Instruction::_binary_instruction<ty> *binInst) {
    auto lhs = getRegister(binInst->getLhs());
    assert(lhs);
    auto reg = translateBinaryInstHelper<ty>(lhs, binInst->getRhs());
    put(binInst, reg);
}

void Translator::translateAllocaInst(const mir::Instruction::alloca_ *allocaInst) {
    auto alloca_size = allocaInst->type->size();
    assert(alloca_size % 4 == 0);
    curFunc->allocaSize += alloca_size;
    auto addr = newAddress(PhyRegister::get("$sp"), -curFunc->allocaSize, &curFunc->stackOffset);
    put(allocaInst, addr);
}

void Translator::translateLoadInst(const mir::Instruction::load *loadInst) {
    auto dst = curFunc->newVirRegister();
    put(loadInst, dst);
    auto ptr = translateOperand(loadInst->getPointerOperand());
    if (auto label = dynamic_cast<rLabel>(ptr)) {
        curBlock->push_back(std::make_unique<LoadInst>(Instruction::Ty::LW, dst, label));
    } else if (auto reg = dynamic_cast<rRegister>(ptr)) {
        curBlock->push_back(std::make_unique<LoadInst>(Instruction::Ty::LW, dst, reg, 0));
    } else if (auto addr = dynamic_cast<rAddress>(ptr)) {
        curBlock->push_back(std::make_unique<LoadInst>(Instruction::Ty::LW, dst, addr));
    } else {
        assert(false);
    }
}

void Translator::translateStoreInst(const mir::Instruction::store *storeInst) {
    auto src = getRegister(storeInst->getSrc());
    auto dst = translateOperand(storeInst->getDest());
    if (auto label = dynamic_cast<rLabel>(dst)) {
        curBlock->push_back(std::make_unique<StoreInst>(Instruction::Ty::SW, src, label));
    } else if (auto reg = dynamic_cast<rRegister>(dst)) {
        curBlock->push_back(std::make_unique<StoreInst>(Instruction::Ty::SW, src, reg, 0));
    } else if (auto addr = dynamic_cast<rAddress>(dst)) {
        curBlock->push_back(std::make_unique<StoreInst>(Instruction::Ty::SW, src, addr));
    } else {
        assert(false);
    }
}

void Translator::translateGetPtrInst(const mir::Instruction::getelementptr *getPtrInst) {
    auto deduce_type = getPtrInst->indexTy;
    auto addr = getAddress(getPtrInst->getPointerOperand());
    for (int i = 0; i < getPtrInst->getNumIndices(); i++) {
        if (i != 0) deduce_type = deduce_type->getBase();
        auto value = getPtrInst->getIndexOperand(i);
        int deduce_size = static_cast<int>(deduce_type->size());
        if (auto imm = dynamic_cast<mir::IntegerLiteral *>(value)) {
            auto _new = addr->offset->clone();
            _new->value += imm->value * deduce_size;
            addr = newAddress(addr->base, std::move(_new), addr->label);
        } else {
            auto reg = getRegister(value);
            auto dst1 = translateBinaryInstHelper<mir::Instruction::MUL>(
                reg, mir::getIntegerLiteral(deduce_size));
            auto dst2 = curFunc->newVirRegister();
            curBlock->push_back(
                std::make_unique<BinaryRInst>(Instruction::Ty::ADDU, dst2, addr->base, dst1));
            addr = newAddress(dst2, addr->offset->clone(), addr->label);
        }
    }
    put(getPtrInst, addr);
}

void Translator::translateConversionInst(const mir::Instruction::trunc *truncInst) {
    // assume i32 -> i1
    auto reg = getRegister(truncInst->getValueOperand());
    auto dst = curFunc->newVirRegister();
    curBlock->push_back(std::make_unique<MoveInst>(dst, reg));
    curBlock->push_back(
        std::make_unique<BinaryRInst>(Instruction::Ty::SLTU, dst, PhyRegister::get(0), reg));
    put(truncInst, reg);
}

void Translator::translateConversionInst(const mir::Instruction::zext *zextInst) {
    // assume i1 -> i32
    put(zextInst, oMap[zextInst->getValueOperand()]);
}

void Translator::translateConversionInst(const mir::Instruction::sext *sextInst) {
    // assume i1 -> i32
    auto reg = getRegister(sextInst->getValueOperand());
    auto dst = curFunc->newVirRegister();
    curBlock->push_back(
        std::make_unique<BinaryRInst>(Instruction::Ty::SUBU, dst, PhyRegister::get(0), reg));
    put(sextInst, reg);
}

void Translator::translateIcmpInst(const mir::Instruction::icmp *icmpInst) {
    auto lhs = icmpInst->getLhs();
    auto rhs = icmpInst->getRhs();
    rRegister reg, tmp;
    switch (icmpInst->cond) {
    case mir::Instruction::icmp::EQ:
        reg = curFunc->newVirRegister();
        tmp = translateBinaryInstHelper<mir::Instruction::SUB>(getRegister(lhs), rhs);
        curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::SLTIU, reg, tmp, 1));
        break;
    case mir::Instruction::icmp::NE:
        reg = curFunc->newVirRegister();
        tmp = translateBinaryInstHelper<mir::Instruction::SUB>(getRegister(lhs), rhs);
        curBlock->push_back(
            std::make_unique<BinaryRInst>(Instruction::Ty::SLTU, reg, PhyRegister::get(0), tmp));
        break;
    case mir::Instruction::icmp::UGT:
        reg = createBinaryInstHelper<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(rhs), lhs);
        break;
    case mir::Instruction::icmp::UGE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelper<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(lhs), rhs);
        curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::XORI, reg, tmp, 1));
        break;
    case mir::Instruction::icmp::ULT:
        reg = createBinaryInstHelper<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(lhs), rhs);
        break;
    case mir::Instruction::icmp::ULE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelper<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(rhs), lhs);
        curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::XORI, reg, tmp, 1));
        break;
    case mir::Instruction::icmp::SGT:
        reg = createBinaryInstHelper<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(rhs),
                                                                                  lhs);
        break;
    case mir::Instruction::icmp::SGE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelper<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(lhs),
                                                                                  rhs);
        curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::XORI, reg, tmp, 1));
        break;
    case mir::Instruction::icmp::SLT:
        reg = createBinaryInstHelper<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(lhs),
                                                                                  rhs);
        break;
    case mir::Instruction::icmp::SLE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelper<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(rhs),
                                                                                  lhs);
        curBlock->push_back(std::make_unique<BinaryIInst>(Instruction::Ty::XORI, reg, tmp, 1));
        break;
    }
    put(icmpInst, reg);
}

void Translator::translatePhiInst(const mir::Instruction::phi *phiInst) {
    // Put a new virtual register to indicate the phi instruction,
    // and we will translate it later.
    if (oMap.count(phiInst)) return;
    put(phiInst, curFunc->newVirRegister());
}

void Translator::translateSelectInst(const mir::Instruction::select *selectInst) {
    auto cond = getRegister(selectInst->getCondition());
    auto trueValue = getRegister(selectInst->getTrueValue());
    auto falseValue = getRegister(selectInst->getFalseValue());
    auto result = curFunc->newVirRegister();
    curBlock->push_back(std::make_unique<MoveInst>(result, trueValue));
    curBlock->push_back(
        std::make_unique<BinaryRInst>(Instruction::Ty::MOVZ, result, falseValue, cond));
    put(selectInst, result);
}

void Translator::translateCallInst(const mir::Instruction::call *callInst) {
    if (auto func = callInst->getFunction(); func == mir::Function::getint()) {
        auto dst = curFunc->newVirRegister();
        curBlock->push_back(SyscallInst::syscall(SyscallInst::SyscallId::ReadInteger));
        curBlock->push_back(std::make_unique<MoveInst>(dst, PhyRegister::get("$v0")));
        put(callInst, dst);
    } else if (func == mir::Function::putint()) {
        auto src = getRegister(callInst->getArg(0));
        curBlock->push_back(std::make_unique<MoveInst>(PhyRegister::get("$a0"), src));
        curBlock->push_back(SyscallInst::syscall(SyscallInst::SyscallId::PrintInteger));
    } else if (func == mir::Function::putch()) {
        auto src = getRegister(callInst->getArg(0));
        curBlock->push_back(std::make_unique<MoveInst>(PhyRegister::get("$a0"), src));
        curBlock->push_back(SyscallInst::syscall(SyscallInst::SyscallId::PrintCharacter));
    } else {
        auto callee = fMap[func];
        unsigned stack_arg_cnt = std::max(0, static_cast<int>(callInst->getNumArgs()) - 4);
        curFunc->argSize = std::max(curFunc->argSize, stack_arg_cnt * 4);
        auto mipsCall = new JumpInst(Instruction::Ty::JAL, callee->label.get());
        for (int i = 0; i < callInst->getNumArgs(); i++) {
            auto reg = getRegister(callInst->getArg(i));
            if (!reg) reg = addressCompute(getAddress(callInst->getArg(i)));
            if (i < 4) {
                auto arg = PhyRegister::get("$a" + std::to_string(i));
                curBlock->push_back(std::make_unique<MoveInst>(arg, reg));
                mipsCall->reg_use_push_back(arg);
            } else {
                curBlock->push_back(std::make_unique<StoreInst>(
                    Instruction::Ty::SW, reg, PhyRegister::get("$sp"), (i - 4) * 4));
            }
        }
        curBlock->push_back(pInstruction{mipsCall});
        if (callInst->isValue()) {
            mipsCall->reg_def_push_back(PhyRegister::get("$v0"));
            auto reg = curFunc->newVirRegister();
            curBlock->push_back(std::make_unique<MoveInst>(reg, PhyRegister::get("$v0")));
            put(callInst, reg);
        }
    }
}

void Translator::translateFunction(const mir::Function *mirFunction) {
    const bool isMain = mirFunction->isMain();
    const bool isLeaf = mirFunction->isLeaf();
    const bool retValue = mirFunction->retType == mir::Type::getI32Type() && !isMain;
    std::string name = mirFunction->name.substr(1);
    curFunc = new Function{std::move(name), isMain, isLeaf, retValue};
    fMap[mirFunction] = curFunc;
    if (!isLeaf) curFunc->shouldSave.insert(PhyRegister::get("$ra"));
    if (isMain)
        assemblyModule->main = pFunction(curFunc);
    else
        assemblyModule->functions.emplace_back(curFunc);

    // blocks
    for (auto bb : mirFunction->bbs) {
        auto block = new Block(curFunc);
        bMap[bb] = block;
        block->node = curFunc->blocks.emplace(curFunc->end(), block);
    }

    // arguments
    assert(mirFunction->bbs.front()->predecessors.empty());
    for (int i = 0; i < mirFunction->args.size(); ++i) {
        auto reg = curFunc->newVirRegister();
        if (i < 4) {
            // move %vr, $ax
            curFunc->blocks.front()->push_back(
                std::make_unique<MoveInst>(reg, PhyRegister::get("$a" + std::to_string(i))));
        } else {
            // lw %vr, x($sp)
            curFunc->blocks.front()->push_back(
                std::make_unique<LoadInst>(Instruction::Ty::LW, reg, PhyRegister::get("$sp"),
                                           (i - 4) * 4, &curFunc->stackOffset));
        }
        put(mirFunction->args[i], reg);
    }

    // translate
    std::priority_queue<std::pair<int, const mir::BasicBlock *>> worklist;
    mirFunction->reCalcBBInfo();
    mirFunction->calcDomDepth();
    for (auto bb : mirFunction->bbs) worklist.emplace(-bb->dom_depth, bb);
    while (!worklist.empty()) {
        auto bb = worklist.top().second;
        worklist.pop();
        translateBasicBlock(bb);
    }
    compute_phi(mirFunction);
    assert(curFunc != nullptr);
    assert(curFunc->allocaSize % 4 == 0);
    assert(curFunc->argSize % 4 == 0);

    // reformat blocks & alloca registers
    assert((curFunc->allocName(), true));
    optimizeBeforeAlloc();
    register_alloca(curFunc);

    // save registers before function & restore registers
    if (isMain) curFunc->shouldSave.clear();  // save nothing
    compute_func_start();
    compute_func_exit();
    optimizeAfterAlloc();
}

void Translator::translateBasicBlock(const mir::BasicBlock *mirBlock) {
    curBlock = bMap[mirBlock];
    for (auto inst : mirBlock->instructions) translateInstruction(inst);
}

void Translator::translateInstruction(const mir::Instruction *mirInst) {
    switch (mirInst->instrTy) {
    case mir::Instruction::RET:
        return translateRetInst(dynamic_cast<const mir::Instruction::ret *>(mirInst));
    case mir::Instruction::BR:
        return translateBranchInst(dynamic_cast<const mir::Instruction::br *>(mirInst));
    case mir::Instruction::ADD:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::add *>(mirInst));
    case mir::Instruction::SUB:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::sub *>(mirInst));
    case mir::Instruction::MUL:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::mul *>(mirInst));
    case mir::Instruction::UDIV:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::udiv *>(mirInst));
    case mir::Instruction::SDIV:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::sdiv *>(mirInst));
    case mir::Instruction::UREM:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::urem *>(mirInst));
    case mir::Instruction::SREM:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::srem *>(mirInst));
    case mir::Instruction::SHL:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::shl *>(mirInst));
    case mir::Instruction::LSHR:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::lshr *>(mirInst));
    case mir::Instruction::ASHR:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::ashr *>(mirInst));
    case mir::Instruction::AND:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::and_ *>(mirInst));
    case mir::Instruction::OR:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::or_ *>(mirInst));
    case mir::Instruction::XOR:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::xor_ *>(mirInst));
    case mir::Instruction::ALLOCA:
        return translateAllocaInst(dynamic_cast<const mir::Instruction::alloca_ *>(mirInst));
    case mir::Instruction::LOAD:
        return translateLoadInst(dynamic_cast<const mir::Instruction::load *>(mirInst));
    case mir::Instruction::STORE:
        return translateStoreInst(dynamic_cast<const mir::Instruction::store *>(mirInst));
    case mir::Instruction::GETELEMENTPTR:
        return translateGetPtrInst(dynamic_cast<const mir::Instruction::getelementptr *>(mirInst));
    case mir::Instruction::TRUNC:
        return translateConversionInst(dynamic_cast<const mir::Instruction::trunc *>(mirInst));
    case mir::Instruction::ZEXT:
        return translateConversionInst(dynamic_cast<const mir::Instruction::zext *>(mirInst));
    case mir::Instruction::SEXT:
        return translateConversionInst(dynamic_cast<const mir::Instruction::sext *>(mirInst));
    case mir::Instruction::ICMP:
        return translateIcmpInst(dynamic_cast<const mir::Instruction::icmp *>(mirInst));
    case mir::Instruction::PHI:
        return translatePhiInst(dynamic_cast<const mir::Instruction::phi *>(mirInst));
    case mir::Instruction::SELECT:
        return translateSelectInst(dynamic_cast<const mir::Instruction::select *>(mirInst));
    case mir::Instruction::CALL:
        return translateCallInst(dynamic_cast<const mir::Instruction::call *>(mirInst));
    default: throw std::runtime_error("not implement!");
    }
}

void Translator::translateGlobalVar(const mir::GlobalVar *mirVar) {
    rGlobalVar result;
    std::string name;
    if (mirVar->unnamed)
        name = mirVar->name, name[0] = '$';
    else
        name = mirVar->name.substr(1);
    if (mirVar->init == nullptr)
        result = new GlobalVar{
            std::move(name), false, false, false, static_cast<unsigned>(mirVar->type->size()), {}};
    else if (auto str = dynamic_cast<mir::StringLiteral *>(mirVar->init))
        result = new GlobalVar{
            std::move(name), true, true, true, static_cast<unsigned>(mirVar->type->size()),
            str->value};
    else
        result = new GlobalVar{
            std::move(name),      true, false, false, static_cast<unsigned>(mirVar->type->size()),
            flatten(mirVar->init)};
    gMap[mirVar] = result;
    assemblyModule->globalVars.emplace_back(result);
}

rOperand Translator::translateOperand(const mir::Value *mirValue) {
    if (auto imm = dynamic_cast<const mir::IntegerLiteral *>(mirValue)) {
        if (imm->value == 0) return PhyRegister::get(0);
        auto reg = curFunc->newVirRegister();
        if ((imm->value & 0xffff) == 0)
            curBlock->push_back(
                std::make_unique<BinaryIInst>(Instruction::Ty::LUI, reg, imm->value >> 16));
        else
            curBlock->push_back(
                std::make_unique<BinaryIInst>(Instruction::Ty::LI, reg, imm->value));
        return reg;
    }
    if (auto op = oMap.find(mirValue); op != oMap.end()) return op->second;
    if (auto gv = dynamic_cast<const mir::GlobalVar *>(mirValue)) {
        if (gMap[gv]->inExtern) return newAddress(PhyRegister::get("$gp"), gMap[gv]->offsetofGp);
        return gMap[gv]->label.get();
    }
    if (auto bb = dynamic_cast<const mir::BasicBlock *>(mirValue)) return bMap[bb]->label.get();
    if (auto func = dynamic_cast<const mir::Function *>(mirValue)) return fMap[func]->label.get();
    put(mirValue, curFunc->newVirRegister());
    return oMap[mirValue];
}

void Translator::compute_phi(const mir::Function *mirFunction) {
    using parallel_copy_t = std::vector<std::pair<rRegister, std::variant<rRegister, int>>>;
    const auto phi2move = [this](const parallel_copy_t &pc) {
        using move_t = MoveInst;
        std::vector<pInstruction> instructions;
        std::unordered_map<rRegister, size_t> inDegree;
        std::unordered_map<rRegister, rRegister> map;
        std::unordered_map<rRegister, std::vector<rRegister>> edges;
        std::unordered_map<rRegister, int> loadImm;
        for (auto &[dst, src] : pc) {
            if (std::holds_alternative<int>(src)) {
                auto imm = std::get<int>(src);
                loadImm[dst] = imm;
            } else {
                auto src_reg = std::get<rRegister>(src);
                edges[dst].push_back(src_reg);
                inDegree[dst];  // make sure dst in inDegree
                inDegree[src_reg]++;
                map[src_reg] = src_reg;
            }
        }
        std::queue<rRegister> queue;
        for (auto &[r, d] : inDegree)
            if (d == 0) queue.push(r);
        while (!inDegree.empty()) {
            while (!queue.empty()) {
                auto reg = queue.front();
                queue.pop(), inDegree.erase(reg);
                for (auto &v : edges[reg]) {
                    instructions.emplace_back(new move_t{reg, map[v]});
                    if (map[v] == v) {
                        inDegree[v]--;
                        if (inDegree[v] == 0) queue.push(v);
                    }
                }
            }
            if (inDegree.empty()) break;
            auto &[reg, _] =
                *std::min_element(inDegree.begin(), inDegree.end(),
                                  [](auto &&x, auto &&y) { return x.second < y.second; });
            auto vir = curFunc->newVirRegister();
            instructions.emplace_back(new move_t{vir, reg});
            map[reg] = vir;
            queue.push(reg);
        }
        for (auto &[dst, imm] : loadImm)
            instructions.emplace_back(new BinaryIInst(Instruction::Ty::LI, dst, imm));
        return instructions;
    };

    for (auto &bb : mirFunction->bbs) {
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
                if (auto imm = dynamic_cast<mir::IntegerLiteral *>(value))
                    pcs[block].emplace_back(dst, imm->value);
                else
                    pcs[block].emplace_back(dst, getRegister(value));
            }
        }
        for (auto &pre : bb->predecessors) {
            auto block = bMap[pre];
            if (pre->successors.size() > 1) {
                auto newBlock = new Block(block->parent);
                auto it = block->node;
                newBlock->node = curFunc->blocks.emplace(++it, newBlock);
                newBlock->push_back(std::make_unique<JumpInst>(Instruction::Ty::J, label));
                for (auto &sub : block->subBlocks)
                    if (sub->back()->getJumpLabel() == label)
                        sub->back()->setJumpLabel(newBlock->label.get());
                block = newBlock;
            }
            for (auto &inst : phi2move(pcs[pre]))
                block->backBlock()->insert(block->backInst()->node, std::move(inst));
        }
    }
}

void Translator::compute_func_start() const {
    auto &first_block = curFunc->blocks.front();
    auto startB = new Block(curFunc);
    startB->node = curFunc->blocks.emplace(curFunc->blocks.begin(), startB);

    // addiu $sp, $sp, -(allocaSize+argSize)
    curFunc->stackOffset =
        static_cast<int>(curFunc->allocaSize + curFunc->argSize + 4 * curFunc->shouldSave.size());
    if (curFunc->stackOffset)
        startB->push_back(
            std::make_unique<BinaryIInst>(Instruction::Ty::ADDIU, PhyRegister::get("$sp"),
                                          PhyRegister::get("$sp"), -curFunc->stackOffset));
    // save registers
    int cnt = 0;
    for (auto reg : curFunc->shouldSave) {
        startB->push_back(
            std::make_unique<StoreInst>(Instruction::Ty::SW, reg, PhyRegister::get("$sp"),
                                        -curFunc->allocaSize - ++cnt * 4, &curFunc->stackOffset));
    }
    startB->push_back(std::make_unique<JumpInst>(Instruction::Ty::J, first_block->label.get()));
}

void Translator::compute_func_exit() const {
    if (curFunc->isMain) {
        curFunc->exitB->push_back(SyscallInst::syscall(SyscallInst::SyscallId::ExitProc));
    } else {
        int cnt = 0;
        for (auto reg : curFunc->shouldSave)
            curFunc->exitB->push_back(std::make_unique<LoadInst>(
                Instruction::Ty::LW, reg, PhyRegister::get("$sp"), -curFunc->allocaSize - ++cnt * 4,
                &curFunc->stackOffset));
        if (curFunc->stackOffset)
            curFunc->exitB->push_back(
                std::make_unique<BinaryIInst>(Instruction::Ty::ADDIU, PhyRegister::get("$sp"),
                                              PhyRegister::get("$sp"), curFunc->stackOffset));
        // jr $ra
        curFunc->exitB->push_back(
            std::make_unique<JumpInst>(Instruction::Ty::JR, PhyRegister::get("$ra")));
    }
}

void Translator::optimizeBeforeAlloc() const {
    if (!opt_settings.force_no_opt) clearDeadCode(curFunc);
    if (opt_settings.using_div2mul) div2mul(curFunc);
    if (!opt_settings.force_no_opt) divisionFold(curFunc);
    if (!opt_settings.force_no_opt) arithmeticFolding(curFunc);
    clearDeadCode(curFunc);
}

void Translator::optimizeAfterAlloc() const {
    if (!opt_settings.force_no_opt) clearDuplicateInst(curFunc);
    if (opt_settings.using_block_relocation) relocateBlock(curFunc);
}

void Translator::translate() {
    translateGlobalVars();
    if (opt_settings.using_gp) assemblyModule->calcGlobalVarOffset();
    translateFunctions();
}
}  // namespace backend::mips
