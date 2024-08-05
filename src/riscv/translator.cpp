//
// Created by toby on 2024/8/4.
//

#include "riscv/translator.h"
#include <list>
#include <memory>
#include <queue>

#include "backend/operand.h"
#include "mir.h"
#include "riscv/alias.h"
#include "riscv/instruction.h"
#include "riscv/operand.h"
#include "riscv/opt.h"
#include "riscv/reg_alloca.h"
#include "util.h"

namespace backend::riscv {
namespace {
void flatten(mir::Literal *literal, std::vector<int> &result) {
    if (auto v = dynamic_cast<mir::IntegerLiteral *>(literal)) {
        result.push_back(v->value);
    } else if (auto f = dynamic_cast<mir::FloatLiteral *>(literal)) {
        result.push_back(*reinterpret_cast<const int *>(&f->value));
    } else if (auto arr = dynamic_cast<mir::ArrayLiteral *>(literal)) {
        for (auto ele : arr->values) flatten(ele, result);
    } else if (auto zero = dynamic_cast<mir::ZeroInitializer *>(literal)) {
        auto size = zero->type->size() / 4;
        while (size--) result.push_back(0);
    } else {
        __builtin_unreachable();
    }
}

std::vector<int> flatten(mir::Literal *literal) {
    std::vector<int> result;
    flatten(literal, result);
    return result;
}
}  // namespace

template <Instruction::Ty rTy, Instruction::Ty iTy>
rRegister Translator::createBinaryInstHelperX(rRegister lhs, mir::Value *rhs) {
    assert(rTy != Instruction::Ty::NOP);
    auto dst = curFunc->newVirRegister(floatOp(rTy));
    if constexpr (iTy != Instruction::Ty::NOP) {
        if (auto literal = dynamic_cast<mir::IntegerLiteral *>(rhs)) {
            // rhs is immediate
            int imm = literal->value;
            if constexpr (rTy == Instruction::Ty::SUB) imm = -imm;  // use addi instead
            if (imm >= -2048 && imm < 2048) {
                curBlock->push_back(std::make_unique<IInstruction>(iTy, dst, lhs, create_imm(imm)));
                return dst;
            }
        }
    }
    auto rop = getRegister(rhs);
    assert(rop);
    curBlock->push_back(std::make_unique<RInstruction>(rTy, dst, lhs, rop));
    return dst;
}

template <mir::Instruction::InstrTy ty, size_t XLEN>
// ty = ADD, SUB, MUL, UDIV, SDIV, UREM, SREM, SHL, LSHR, ASHR, AND, OR, XOR
rRegister Translator::translateBinaryInstHelper(rRegister lhs, mir::Value *rhs) {
    static_assert(XLEN == 32 || XLEN == 64, "XLEN must be 32 or 64");
    constexpr bool X32 = XLEN == 32;
    using Ty = Instruction::Ty;

    constexpr std::pair<Instruction::Ty, Instruction::Ty> mipsTys[] = {
        {X32 ? Ty::ADDW : Ty::ADD, X32 ? Ty::ADDIW : Ty::ADDI},
        {X32 ? Ty::SUBW : Ty::SUB, X32 ? Ty::ADDIW : Ty::ADDI},
        {X32 ? Ty::MULW : Ty::MUL, {}},
        {X32 ? Ty::DIVUW : Ty::DIVU, {}},
        {X32 ? Ty::DIVW : Ty::DIV, {}},
        {X32 ? Ty::REMUW : Ty::REMU, {}},
        {X32 ? Ty::REMW : Ty::REM, {}},
        {Ty::FADD_S, {}},
        {Ty::FSUB_S, {}},
        {Ty::FMUL_S, {}},
        {Ty::FDIV_S, {}},
        {/* FREM */},
        {X32 ? Ty::SLLW : Ty::SLL, X32 ? Ty::SLLIW : Ty::SLLI},
        {X32 ? Ty::SRLW : Ty::SRL, X32 ? Ty::SRLIW : Ty::SRLI},
        {X32 ? Ty::SRAW : Ty::SRA, X32 ? Ty::SRAIW : Ty::SRAI},
        {Ty::AND, Ty::ANDI},
        {Ty::OR, Ty::ORI},
        {Ty::XOR, Ty::XORI},
    };

    constexpr auto index = ty - mir::Instruction::ADD;
    return createBinaryInstHelperX<mipsTys[index].first, mipsTys[index].second>(lhs, rhs);
}

void Translator::translateRetInst(const mir::Instruction::ret *retInst) {
    if (auto v = retInst->getReturnValue()) {
        auto reg = getRegister(v);
        assert(reg);
        curBlock->push_back(std::make_unique<MoveInstruction>("a0"_R, reg));
    }
    curBlock->push_back(std::make_unique<JumpInstruction>(curFunc->exitB->label.get()));
}

void Translator::translateBranchInst(const mir::Instruction::br *brInst) {
    if (!brInst->hasCondition()) {
        curBlock->push_back(
            std::make_unique<JumpInstruction>(bMap[brInst->getTarget()]->label.get()));
    } else if (auto cond = dynamic_cast<mir::Instruction::icmp *>(brInst->getCondition())) {
        auto lhs = getRegister(cond->getLhs());
        auto rhs = getRegister(cond->getRhs());
        assert(lhs && rhs);
        auto trueLabel = bMap[brInst->getIfTrue()]->label.get();
        auto falseLabel = bMap[brInst->getIfFalse()]->label.get();
        assert(trueLabel && falseLabel);
        auto cond_ty = cond->cond;
        if (brInst->likely >= 0.5) {
            cond_ty = ~cond_ty;
            std::swap(trueLabel, falseLabel);
        }
        Instruction::Ty riscvTy;
        switch (cond_ty) {
        case mir::Instruction::icmp::EQ: riscvTy = Instruction::Ty::BEQ; break;
        case mir::Instruction::icmp::NE: riscvTy = Instruction::Ty::BNE; break;
        case mir::Instruction::icmp::UGT:
            std::swap(lhs, rhs);
            riscvTy = Instruction::Ty::BLTU;
            break;
        case mir::Instruction::icmp::UGE: riscvTy = Instruction::Ty::BGEU; break;
        case mir::Instruction::icmp::ULT: riscvTy = Instruction::Ty::BLTU; break;
        case mir::Instruction::icmp::ULE:
            std::swap(lhs, rhs);
            riscvTy = Instruction::Ty::BGEU;
            break;
        case mir::Instruction::icmp::SGT:
            std::swap(lhs, rhs);
            riscvTy = Instruction::Ty::BLT;
            break;
        case mir::Instruction::icmp::SGE: riscvTy = Instruction::Ty::BGE; break;
        case mir::Instruction::icmp::SLT: riscvTy = Instruction::Ty::BLT; break;
        case mir::Instruction::icmp::SLE:
            std::swap(lhs, rhs);
            riscvTy = Instruction::Ty::BGE;
            break;
        }
        curBlock->push_back(std::make_unique<BInstruction>(riscvTy, lhs, rhs, trueLabel));
        curBlock->push_back(std::make_unique<JumpInstruction>(falseLabel));
    }
}

template <mir::Instruction::InstrTy ty>
void Translator::translateBinaryInst(const mir::Instruction::_binary_instruction<ty> *binInst) {
    auto lhs = getRegister(binInst->getLhs());
    assert(lhs);
    auto reg = translateBinaryInstHelper<ty, 32>(lhs, binInst->getRhs());
    put(binInst, reg);
}

void Translator::translateFnegInst(const mir::Instruction::fneg *fnegInst) {
    auto reg = getRegister(fnegInst->getOperand());
    auto dst = curFunc->newVirRegister(true);
    curBlock->push_back(std::make_unique<FnegInstruction>(dst, reg));
    put(fnegInst, dst);
}

void Translator::translateAllocaInst(const mir::Instruction::alloca_ *allocaInst) {
    auto alloca_size = allocaInst->type->size();
    assert(alloca_size % 4 == 0);
    curFunc->allocaSize += alloca_size;
    auto imm = create_imm((int)curFunc->allocaSize);
    stack_imm_pointers.push(&imm->value);
    used_operands.push(std::make_unique<Address>("sp"_R, std::move(imm)));
    auto addr = used_operands.top().get();
    put(allocaInst, addr);
}

void Translator::translateLoadInst(const mir::Instruction::load *loadInst) {
    bool isFloat = loadInst->type->isFloatTy();
    auto dst = curFunc->newVirRegister(isFloat);
    auto load_ty = isFloat ? Instruction::Ty::FLW : Instruction::Ty::LW;
    put(loadInst, dst);
    auto ptr = translateOperand(loadInst->getPointerOperand());
    if (auto label = dynamic_cast<rLabel>(ptr)) {
        curBlock->push_back(
            std::make_unique<IInstruction>(load_ty, dst, "x0"_R, create_imm(label)));
    } else if (auto reg = dynamic_cast<rRegister>(ptr)) {
        curBlock->push_back(std::make_unique<IInstruction>(load_ty, dst, reg, create_imm(0)));
    } else if (auto addr = dynamic_cast<rAddress>(ptr)) {
        curBlock->push_back(
            std::make_unique<IInstruction>(load_ty, dst, addr->base, addr->offset->clone()));
    } else {
        assert(false);
    }
}

void Translator::translateStoreInst(const mir::Instruction::store *storeInst) {
    bool isFloat = storeInst->getSrc()->type->isFloatTy();
    auto store_ty = isFloat ? Instruction::Ty::FSW : Instruction::Ty::SW;
    auto src = getRegister(storeInst->getSrc());
    auto dst = translateOperand(storeInst->getDest());
    if (auto label = dynamic_cast<rLabel>(dst)) {
        curBlock->push_back(
            std::make_unique<SInstruction>(store_ty, src, "x0"_R, create_imm(label)));
    } else if (auto reg = dynamic_cast<rRegister>(dst)) {
        curBlock->push_back(std::make_unique<SInstruction>(store_ty, src, reg, create_imm(0)));
    } else if (auto addr = dynamic_cast<rAddress>(dst)) {
        curBlock->push_back(
            std::make_unique<SInstruction>(store_ty, src, addr->base, addr->offset->clone()));
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
            auto _new = join_imm(addr->offset->clone(), create_imm(imm->value * deduce_size));
            addr = newAddress(addr->base, std::move(_new));
        } else {
            auto reg = getRegister(value);
            auto dst1 = translateBinaryInstHelper<mir::Instruction::MUL, 64>(
                reg, mir::getIntegerLiteral(deduce_size));
            auto dst2 = curFunc->newVirRegister();
            curBlock->push_back(
                std::make_unique<RInstruction>(Instruction::Ty::ADD, dst2, addr->base, dst1));
            addr = newAddress(dst2, addr->offset->clone());
        }
    }
    put(getPtrInst, addr);
}

void Translator::translateConversionInst(const mir::Instruction::trunc *truncInst) {
    // assume i32 -> i1
    auto reg = getRegister(truncInst->getValueOperand());
    auto dst = curFunc->newVirRegister();
    curBlock->push_back(std::make_unique<MoveInstruction>(dst, reg));
    curBlock->push_back(std::make_unique<RInstruction>(Instruction::Ty::SLTU, dst, "x0"_R, reg));
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
    curBlock->push_back(std::make_unique<RInstruction>(Instruction::Ty::SUB, dst, "x0"_R, reg));
    put(sextInst, reg);
}

template <mir::Instruction::InstrTy ty>
void Translator::translateFpConvInst(
    const mir::Instruction::_conversion_instruction<ty> *fpConvInst) {
    constexpr Instruction::Ty fpConvTy[] = {
        Instruction::Ty::FCVT_S_WU,
        Instruction::Ty::FCVT_S_W,
        Instruction::Ty::FCVT_WU_S,
        Instruction::Ty::FCVT_W_S,
    };
    constexpr size_t indexTy = ty - mir::Instruction::FPTOUI;
    static_assert(indexTy < 4, "wrong mir::Instruction::InstrTy");
    auto reg = getRegister(fpConvInst->getValueOperand());
    auto dst = curFunc->newVirRegister(fpConvInst->type->isFloatTy());
    curBlock->push_back(std::make_unique<FpConvInstruction>(fpConvTy[indexTy], dst, reg));
    put(fpConvInst, dst);
}

void Translator::translateIcmpInst(const mir::Instruction::icmp *icmpInst) {
    auto lhs = icmpInst->getLhs();
    auto rhs = icmpInst->getRhs();
    rRegister reg, tmp;
    switch (icmpInst->cond) {
    case mir::Instruction::icmp::EQ:
        reg = curFunc->newVirRegister();
        tmp = translateBinaryInstHelper<mir::Instruction::SUB, 32>(getRegister(lhs), rhs);
        curBlock->push_back(std::make_unique<IInstruction>(Instruction::Ty::SLTIU, reg, tmp, 1_I));
        break;
    case mir::Instruction::icmp::NE:
        reg = curFunc->newVirRegister();
        tmp = translateBinaryInstHelper<mir::Instruction::SUB, 32>(getRegister(lhs), rhs);
        curBlock->push_back(
            std::make_unique<RInstruction>(Instruction::Ty::SLTU, reg, "x0"_R, tmp));
        break;
    case mir::Instruction::icmp::UGT:
        reg = createBinaryInstHelperX<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(rhs), lhs);
        break;
    case mir::Instruction::icmp::UGE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelperX<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(lhs), rhs);
        curBlock->push_back(std::make_unique<IInstruction>(Instruction::Ty::XORI, reg, tmp, 1_I));
        break;
    case mir::Instruction::icmp::ULT:
        reg = createBinaryInstHelperX<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(lhs), rhs);
        break;
    case mir::Instruction::icmp::ULE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelperX<Instruction::Ty::SLTU, Instruction::Ty::SLTIU>(
            getRegister(rhs), lhs);
        curBlock->push_back(std::make_unique<IInstruction>(Instruction::Ty::XORI, reg, tmp, 1_I));
        break;
    case mir::Instruction::icmp::SGT:
        reg = createBinaryInstHelperX<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(rhs),
                                                                                   lhs);
        break;
    case mir::Instruction::icmp::SGE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelperX<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(lhs),
                                                                                   rhs);
        curBlock->push_back(std::make_unique<IInstruction>(Instruction::Ty::XORI, reg, tmp, 1_I));
        break;
    case mir::Instruction::icmp::SLT:
        reg = createBinaryInstHelperX<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(lhs),
                                                                                   rhs);
        break;
    case mir::Instruction::icmp::SLE:
        reg = curFunc->newVirRegister();
        tmp = createBinaryInstHelperX<Instruction::Ty::SLT, Instruction::Ty::SLTI>(getRegister(rhs),
                                                                                   lhs);
        curBlock->push_back(std::make_unique<IInstruction>(Instruction::Ty::XORI, reg, tmp, 1_I));
        break;
    }
    put(icmpInst, reg);
}

void Translator::translateFcmpInst(const mir::Instruction::fcmp *fcmpInst) {
    auto lhs = getRegister(fcmpInst->getLhs());
    auto rhs = getRegister(fcmpInst->getRhs());
    auto reg = curFunc->newVirRegister();
    using Ty = Instruction::Ty;
    Ty condTy{};
    switch (fcmpInst->cond) {
    case mir::Instruction::fcmp::FALSE:
        curBlock->push_back(std::make_unique<MoveInstruction>(reg, "x0"_R));
        break;
    case mir::Instruction::fcmp::OEQ: condTy = Ty::FEQ_S; break;
    case mir::Instruction::fcmp::OGT:
        condTy = Ty::FLT_S;
        std::swap(lhs, rhs);
        break;
    case mir::Instruction::fcmp::OGE:
        condTy = Ty::FLE_S;
        std::swap(lhs, rhs);
        break;
    case mir::Instruction::fcmp::OLT: condTy = Ty::FLT_S; break;
    case mir::Instruction::fcmp::OLE: condTy = Ty::FLE_S; break;
    case mir::Instruction::fcmp::ONE:
        curBlock->push_back(std::make_unique<RInstruction>(Ty::FEQ_S, reg, lhs, rhs));
        curBlock->push_back(std::make_unique<IInstruction>(Ty::XORI, reg, reg, 1_I));
        break;
    case mir::Instruction::fcmp::ORD:
    case mir::Instruction::fcmp::UEQ:
    case mir::Instruction::fcmp::UGT:
    case mir::Instruction::fcmp::UGE:
    case mir::Instruction::fcmp::ULT:
    case mir::Instruction::fcmp::ULE:
    case mir::Instruction::fcmp::UNE:
    case mir::Instruction::fcmp::UNO: TODO("not implemented");
    case mir::Instruction::fcmp::TRUE:
        curBlock->push_back(std::make_unique<IInstruction>(Ty::ADDI, reg, "x0"_R, 1_I));
        break;
    }
    if (condTy != Ty::NOP)
        curBlock->push_back(std::make_unique<RInstruction>(condTy, reg, lhs, rhs));
    put(fcmpInst, reg);
}

void Translator::translatePhiInst(const mir::Instruction::phi *phiInst) {
    // Put a new virtual register to indicate the phi instruction,
    // and we will translate it later.
    if (oMap.count(phiInst)) return;
    put(phiInst, curFunc->newVirRegister());
}

// NOLINTNEXTLINE
void Translator::translateSelectInst(const mir::Instruction::select *selectInst) {
    TODO("select should not be used in riscv IR");
}

void Translator::translateCallInst(const mir::Instruction::call *callInst) {
    auto func = callInst->getFunction();
    auto label = func->isLibrary() ? getLibLabel(func->name) : fMap.at(func)->label.get();
    unsigned use_stack = 0, x_cnt = 0, f_cnt = 0;
    for (int i = 0; i < callInst->getNumArgs(); i++) {
        auto arg = callInst->getArg(i);
        bool isFloat = arg->type->isFloatTy();
        auto reg = getRegister(arg);
        if (!isFloat) {
            if (++x_cnt > 8) {
                curBlock->push_back(std::make_unique<SInstruction>(Instruction::Ty::SW, reg, "sp"_R,
                                                                   create_imm((int)use_stack)));
                use_stack += 8;
            } else {
                curBlock->push_back(std::make_unique<MoveInstruction>(
                    PhyRegister::get("a" + std::to_string(x_cnt - 1)), reg));
            }
        } else {
            if (++f_cnt > 8) {
                curBlock->push_back(std::make_unique<SInstruction>(
                    Instruction::Ty::FSW, reg, "sp"_R, create_imm((int)use_stack)));
                use_stack += 8;
            } else {
                curBlock->push_back(std::make_unique<MoveInstruction>(
                    PhyRegister::get("fa" + std::to_string(f_cnt - 1)), reg));
            }
        }
    }
    curFunc->argSize = std::max(curFunc->argSize, use_stack);
    curBlock->push_back(std::make_unique<CallInstruction>(label));
    // get return value
    if (func->retType->isIntegerTy()) {
        auto temp = curFunc->newVirRegister(false);
        curBlock->push_back(std::make_unique<MoveInstruction>(temp, "a0"_R));
        put(callInst, temp);
    } else if (func->retType->isFloatTy()) {
        auto temp = curFunc->newVirRegister(true);
        curBlock->push_back(std::make_unique<MoveInstruction>(temp, "fa0"_R));
        put(callInst, temp);
    }
}

void Translator::translateMemsetInst(const mir::Instruction::memset *memsetInst) {
    if (memsetInst->size <= 0) return;
    if (memsetInst->val != 0) TODO("not implemented for memset none zero");
    if (memsetInst->size % 4 != 0) TODO("not implemented for memset size not multiple of 4");
    auto addr = getAddress(memsetInst->getBase());
    if (memsetInst->size / 4 <= 32) {
        for (int i = 0; i < memsetInst->size / 4; i++) {
            curBlock->push_back(
                std::make_unique<SInstruction>(Instruction::Ty::SW, "x0"_R, addr->base,
                                               join_imm(addr->offset->clone(), create_imm(i * 4))));
        }
        return;
    }
    for (auto &inst : translateImmAs("a1"_R, memsetInst->size / 4))
        curBlock->push_back(std::move(inst));
    auto ptr = addr2reg(addr);
    curBlock->push_back(std::make_unique<MoveInstruction>("a0"_R, ptr));
    curBlock->push_back(std::make_unique<CallInstruction>(getLibLabel("@sysy.memset0")));
}

void Translator::translateFunction(const mir::Function *mirFunction) {
    const bool isMain = mirFunction->isMain();
    const bool isLeaf = mirFunction->isLeaf();
    const bool retValue = !mirFunction->retType->isVoidTy();
    std::string name = mirFunction->name.substr(1);
    curFunc = new Function{std::move(name), isMain, isLeaf, retValue};
    fMap[mirFunction] = curFunc;
    if (!isLeaf) curFunc->shouldSave.insert("ra"_R);
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
    unsigned use_stack = 0, x_cnt = 0, f_cnt = 0;
    for (auto arg : mirFunction->args) {
        bool isFloat = arg->type->isFloatTy();
        auto reg = curFunc->newVirRegister(isFloat);
        if (!isFloat) {
            if (++x_cnt < 8) {
                // move %vr, ax
                curFunc->blocks.front()->push_back(std::make_unique<MoveInstruction>(
                    reg, PhyRegister::get("a" + std::to_string(x_cnt - 1))));
            } else {
                // lw %vr, x(sp)
                auto offset = create_imm((int)use_stack);
                stack_imm_pointers.push(&offset->value);
                curFunc->blocks.front()->push_back(std::make_unique<IInstruction>(
                    Instruction::Ty::LW, reg, "sp"_R, std::move(offset)));
                use_stack += 8;
            }
        } else {
            if (++f_cnt < 8) {
                // move %vr, ax
                curFunc->blocks.front()->push_back(std::make_unique<MoveInstruction>(
                    reg, PhyRegister::get("fa" + std::to_string(f_cnt - 1))));
            } else {
                // lw %vr, x(sp)
                auto offset = create_imm((int)use_stack);
                stack_imm_pointers.push(&offset->value);
                curFunc->blocks.front()->push_back(std::make_unique<IInstruction>(
                    Instruction::Ty::FLW, reg, "sp"_R, std::move(offset)));
                use_stack += 8;
            }
        }
        put(arg, reg);
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
    assert(curFunc->argSize % 8 == 0);

    // reformat blocks & alloca registers
    assert((curFunc->allocName(), true));
    optimizeBeforeAlloc();
    register_alloca(curFunc, stack_imm_pointers);

    // save registers before function & restore registers
    compute_func_start();
    compute_func_exit();

    // stack
    while (!stack_imm_pointers.empty()) {
        auto ptr = stack_imm_pointers.top();
        stack_imm_pointers.pop();
        *ptr += curFunc->stackOffset;
    }

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
    case mir::Instruction::FADD:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::fadd *>(mirInst));
    case mir::Instruction::FSUB:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::fsub *>(mirInst));
    case mir::Instruction::FMUL:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::fmul *>(mirInst));
    case mir::Instruction::FDIV:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::fdiv *>(mirInst));
    case mir::Instruction::FREM:
        return translateBinaryInst(dynamic_cast<const mir::Instruction::frem *>(mirInst));
    case mir::Instruction::FNEG:
        return translateFnegInst(dynamic_cast<const mir::Instruction::fneg *>(mirInst));
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
    case mir::Instruction::FPTOSI:
        return translateFpConvInst(dynamic_cast<const mir::Instruction::fptosi *>(mirInst));
    case mir::Instruction::FPTOUI:
        return translateFpConvInst(dynamic_cast<const mir::Instruction::fptoui *>(mirInst));
    case mir::Instruction::SITOFP:
        return translateFpConvInst(dynamic_cast<const mir::Instruction::sitofp *>(mirInst));
    case mir::Instruction::UITOFP:
        return translateFpConvInst(dynamic_cast<const mir::Instruction::uitofp *>(mirInst));
    case mir::Instruction::ICMP:
        return translateIcmpInst(dynamic_cast<const mir::Instruction::icmp *>(mirInst));
    case mir::Instruction::FCMP:
        return translateFcmpInst(dynamic_cast<const mir::Instruction::fcmp *>(mirInst));
    case mir::Instruction::PHI:
        return translatePhiInst(dynamic_cast<const mir::Instruction::phi *>(mirInst));
    case mir::Instruction::SELECT:
        return translateSelectInst(dynamic_cast<const mir::Instruction::select *>(mirInst));
    case mir::Instruction::CALL:
        return translateCallInst(dynamic_cast<const mir::Instruction::call *>(mirInst));
    case mir::Instruction::MEMSET:
        return translateMemsetInst(dynamic_cast<const mir::Instruction::memset *>(mirInst));
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
        if (imm->value == 0) return "x0"_R;
        auto reg = curFunc->newVirRegister();
        for (auto &inst : translateImmAs(reg, imm->value)) curBlock->push_back(std::move(inst));
        return reg;
    }
    if (auto imm = dynamic_cast<const mir::FloatLiteral *>(mirValue)) {
        TODO("translate float literal");
    }
    if (auto op = oMap.find(mirValue); op != oMap.end()) return op->second;
    if (auto gv = dynamic_cast<const mir::GlobalVar *>(mirValue)) {
        if (gMap[gv]->inExtern)
            return newAddress(PhyRegister::get("$gp"), create_imm(gMap[gv]->offsetofGp));
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
        using move_t = MoveInstruction;
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
            for (auto &inst : translateImmAs(dst, imm)) instructions.push_back(std::move(inst));
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
                newBlock->push_back(std::make_unique<JumpInstruction>(label));
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

#define align(x) (((x) + 15) & ~15)
    curFunc->stackOffset = align(
        static_cast<int>(curFunc->allocaSize + curFunc->argSize + 8 * curFunc->shouldSave.size()));
#undef align
    // addiu $sp, $sp, -(allocaSize+argSize)
    if (curFunc->stackOffset)
        startB->push_back(std::make_unique<IInstruction>(Instruction::Ty::ADDI, "sp"_R, "sp"_R,
                                                         create_imm(-curFunc->stackOffset)));
    // save registers
    int cnt = 0;
    for (auto reg : curFunc->shouldSave) {
        startB->push_back(std::make_unique<SInstruction>(
            Instruction::Ty::SD, reg, "sp"_R,
            create_imm(static_cast<int>(curFunc->stackOffset - curFunc->allocaSize - ++cnt * 8))));
    }
    startB->push_back(std::make_unique<JumpInstruction>(first_block->label.get()));
}

void Translator::compute_func_exit() const {
    int cnt = 0;
    for (auto reg : curFunc->shouldSave) {
        curFunc->exitB->push_back(std::make_unique<IInstruction>(
            Instruction::Ty::LD, reg, "sp"_R,
            create_imm(static_cast<int>(curFunc->stackOffset - curFunc->allocaSize - ++cnt * 8))));
    }
    if (curFunc->stackOffset)
        curFunc->exitB->push_back(std::make_unique<IInstruction>(
            Instruction::Ty::ADDI, "sp"_R, "sp"_R, create_imm((int)curFunc->stackOffset)));
    // ret
    curFunc->exitB->push_back(std::make_unique<RetInstruction>());
}

void Translator::optimizeBeforeAlloc() const {  // TODO opt
    clearDeadCode(curFunc);
}

void Translator::optimizeAfterAlloc() const {}  // TODO opt
}  // namespace backend::riscv
