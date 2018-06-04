#include "WrappedDomainBBAbsTransCreator.hpp"
#include "LlvmVocabularyUtils.hpp"
#include "MergeFunction.hpp"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/IR/Function.h"
#include <algorithm>
#include <cmath>

#include "wali/wpds/ewpds/EWPDS.hpp"
#include "wali/wpds/fwpds/FWPDS.hpp"

#include "utils/timer/timer.hpp"

#include "src/AbstractDomain/common/ReducedProductAbsVal.hpp"
#include "src/AbstractDomain/PointsetPowerset/pointset_powerset_av.hpp"
#include "ppl.hh"

using namespace llvm;
using namespace abstract_domain;

namespace llvm_abstract_transformer {
  WrappedDomainBBAbsTransCreator::WrappedDomainBBAbsTransCreator(std::unique_ptr<Module> M, ref_ptr<AbstractValue> av, bool add_array_bounds_check) : ExecutionEngine(std::move(M)), TD(Modules.back().get()), av_prevoc_(av->Top()), add_array_bounds_check_(add_array_bounds_check) {
    setDataLayout(&TD);
    IL = new IntrinsicLowering(TD);
  }

  void WrappedDomainBBAbsTransCreator::InitializeMembers
  (llvm::BasicBlock* bb, 
   BasicBlock::iterator start,
   BasicBlock::iterator end,
   const Vocabulary& pre_voc, 
   const Vocabulary& post_voc,
   const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map, 
   const value_to_dim_t& instr_value_to_dim,
   bool initialize_state) {

    // Initialize local variables
    bb_ = bb;
    if(initialize_state) {  
      alloca_map_ = alloca_map;
      instr_value_to_dim_ = instr_value_to_dim;
      val_to_abs_bool_store_.clear();
    }

    Vocabulary pre_post_voc;
    UnionVocabularies(pre_voc, post_voc, pre_post_voc);

    Vocabulary bb_voc = GetBasicBlockVocabulary(bb, start, end, getDataLayout(), instr_value_to_dim_, alloca_map_);
    Vocabulary pre_bb_voc = abstract_domain::getVocabularySubset(bb_voc, 0);
    Vocabulary post_bb_voc = abstract_domain::getVocabularySubset(bb_voc, 1);
    Vocabulary unversioned_bb_voc = abstract_domain::getVocabularySubset(bb_voc, UNVERSIONED_VERSION);

    DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                   std::cout << "\nInitializeMembers for bb " << getName(bb);
                   abstract_domain::print(std::cout << "\npre_voc:", pre_voc);
                   abstract_domain::print(std::cout << "\npost_voc:", post_voc);
                   abstract_domain::print(std::cout << "\npre_bb_voc:", pre_bb_voc);
                   abstract_domain::print(std::cout << "\npost_bb_voc:", post_bb_voc);
                   abstract_domain::print(std::cout << "\nunversioned_bb_voc:", unversioned_bb_voc);
                   abstract_domain::print(std::cout << "\nbb_voc:", bb_voc););

    //Vocabulary pre_pre_bb_unversioned_bb_voc, pre_pre_bb_voc;
    //UnionVocabularies(pre_voc, pre_bb_voc, pre_pre_bb_voc);
    //UnionVocabularies(pre_pre_bb_voc, unversioned_bb_voc, pre_pre_bb_unversioned_bb_voc);
    // av_prevoc_->AddVocabulary(pre_pre_bb_unversioned_bb_voc);
    // av_prevoc_->Project(pre_pre_bb_unversioned_bb_voc); // Ensure that av_prevoc_ only talks about pre_voc
    av_prevoc_->AddVocabulary(pre_voc);
    av_prevoc_->Project(pre_voc); // Ensure that av_prevoc_ only talks about pre_voc

    // Define post voc as union of post_local_voc, post_global_voc, post_bb_voc and return_voc
    Vocabulary post_voc_with_bb;
    UnionVocabularies(post_voc, post_bb_voc, post_voc_with_bb);


    if(initialize_state) {  
      // Use av_prevoc_ to get a handle on the abstract value for av_dvoc_top
      ref_ptr<AbstractValue> av_dvoc_top = av_prevoc_->Top(); 
      av_dvoc_top->AddVocabulary(bb_voc);
      av_dvoc_top->AddVocabulary(post_voc);

      // Keep the vocabulary sign empty in the beginning, 
      // add vocabulary for wrapping only if needed
      std::map<DimensionKey, bool> vocab_sign;
      state_ = new BitpreciseWrappedAbstractValue(av_dvoc_top, vocab_sign);
      ref_ptr<AvSemiring> av_sem = new AvSemiring(state_);
      ref_ptr<AvSemiring> one = dynamic_cast<AvSemiring*>(av_sem->one().get_ptr());
      one->SetToSyntacticOne();
      state_ = dynamic_cast<BitpreciseWrappedAbstractValue*>(one->GetAbstractValue().get_ptr());
    }

    state_->AddVocabulary(bb_voc);
    state_->AddVocabulary(post_voc);

    Vocabulary pre_post_bb_voc;
    UnionVocabularies(pre_post_voc, bb_voc, pre_post_bb_voc);
    state_->Project(pre_post_bb_voc);

    //state_->Project(pre_post_voc);
  }


  WrappedDomainBBAbsTransCreator::~WrappedDomainBBAbsTransCreator() {
  }

  llvm::Module* WrappedDomainBBAbsTransCreator::getModule() {
    return Modules.back().get();
  }

  ref_ptr<AbstractValue> WrappedDomainBBAbsTransCreator::GetState() {
    return state_.get_ptr();
  }

  ref_ptr<AbstractValue> WrappedDomainBBAbsTransCreator::GetPreVocAv() {
    return av_prevoc_;
  }

  ref_ptr<BitpreciseWrappedAbstractValue> WrappedDomainBBAbsTransCreator::GetBitpreciseWrappedState(ref_ptr<AbstractValue> st) const {
    ref_ptr<BitpreciseWrappedAbstractValue> bpwav = dynamic_cast<BitpreciseWrappedAbstractValue*>(st.get_ptr());
    assert(bpwav != NULL);
    if(false/*add_array_bounds_check_*/) {
      ref_ptr<ReducedProductAbsVal> rpav = dynamic_cast<ReducedProductAbsVal*>(bpwav->av().get_ptr());
      assert(rpav != NULL);

      ref_ptr<PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> > eqav =
        dynamic_cast<PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> *>(rpav->second().get_ptr());
      assert(eqav != NULL && eqav->equalities_only());
      ref_ptr<ReducedProductAbsVal> rpav_first =
        dynamic_cast<ReducedProductAbsVal*>(rpav->first().get_ptr());
    assert(rpav_first == NULL);
      return new BitpreciseWrappedAbstractValue(rpav->first(), bpwav->wrapped_voc());
    }
    return bpwav;
  }

  void WrappedDomainBBAbsTransCreator::SetState(ref_ptr<AbstractValue> st) {
    ref_ptr<BitpreciseWrappedAbstractValue> bpwav = dynamic_cast<BitpreciseWrappedAbstractValue*>(st.get_ptr());
    assert(bpwav != NULL);
    state_ = bpwav;

    // Sanity check
    if(false/*add_array_bounds_check_*/) {
      ref_ptr<ReducedProductAbsVal> rpav = dynamic_cast<ReducedProductAbsVal*>(bpwav->av().get_ptr());
      assert(rpav != NULL);
      ref_ptr<PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> > eqav =
        dynamic_cast<PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> *>(rpav->second().get_ptr());
      assert(eqav != NULL && eqav->equalities_only());
    }
  }

  void WrappedDomainBBAbsTransCreator::SetSucc(llvm::BasicBlock* succ) {
    succ_ = succ;
  }

  // Abstractly Execute BasicBlock except the terminator instruction, the result of abstractInterpretation
  // is stored in state_
  void WrappedDomainBBAbsTransCreator::abstractExecuteBasicBlock
  (BasicBlock* bb, 
   BasicBlock::iterator start,
   BasicBlock::iterator end,
   const std::map<Instruction *, Vocabulary>& insLiveBeforeMap,
   const std::map<Instruction *, Vocabulary>& insLiveAfterMap,
   const Vocabulary& pre_voc, 
   const Vocabulary& post_voc, 
   const std::map<Value*, std::pair<DimensionKey, unsigned> >& alloca_map, 
   const value_to_dim_t& instr_value_to_dim,
   bool initialize_state) {

    std::stringstream ss;
    Instruction* start_ins = start;
    ss << "AbsExecuteBB__" << getName(bb) << " starting from " << getName(start_ins) << ":";
    utils::Timer bb_timer(ss.str(), std::cout, false);

    Vocabulary pre_post_voc;
    UnionVocabularies(pre_voc, post_voc, pre_post_voc);

    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                   std::cout << "\nAbstractly executing BB: " << getName(bb) << 
                   " starting from " << getName(start_ins) << "\n";);

    // Initialize state_ and bb_to calculate abstract transformer
    InitializeMembers(bb, start, end, pre_voc, post_voc, alloca_map, instr_value_to_dim, initialize_state);

    assert(start != end);
    while(start != end) {
      Instruction* I = start;
      std::stringstream iss;
      iss << "AbsExecuteIns__" << getName(I) << ":";
      utils::Timer inst_timer(iss.str(), std::cout, false);

      bool found_terminator = false;
      switch(I->getOpcode()) {
        // Ignore terminator instructions such as branch, switch
      case Instruction::Ret:
      case Instruction::Br: 
      case Instruction::Switch: 
      case Instruction::Invoke:
      case Instruction::Resume:
      case Instruction::Unreachable:
        found_terminator = true;
        break;
      default:
        abstractExecuteInst(*I, insLiveBeforeMap, insLiveAfterMap, pre_post_voc);
      }

      if(found_terminator)
        break;

      DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                     std::cout << std::endl << "state_ after executing " << std::flush;
                     I->print(outs()); 
                     std::cout << " is:" << std::endl;
                     print(std::cout) << std::endl;);

      start++;

      DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << iss.str() << inst_timer.elapsed(););
    }

    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << ss.str() << bb_timer.elapsed(););
  }

   void WrappedDomainBBAbsTransCreator::abstractExecuteInst
   (Instruction& I, 
    const std::map<Instruction *, Vocabulary>& insLiveBeforeMap, 
    const std::map<Instruction *, Vocabulary>& insLiveAfterMap, 
    const Vocabulary& pre_post_voc) {
     DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                    std::cout << "\nAbstract execute instruction " << getName(&I) << ":" << std::flush;);

     // All voc is one, since varaible access and update work on voc 1
     Vocabulary ins_live_before = insLiveBeforeMap.at(&I);
     ins_live_before = abstract_domain::replaceVersion(ins_live_before, 0, 1);
     Vocabulary ins_live_after = insLiveAfterMap.at(&I);
     ins_live_after = abstract_domain::replaceVersion(ins_live_after, 0, 1);
     Vocabulary ins_live;
     UnionVocabularies(ins_live_before, ins_live_after, ins_live);
     Vocabulary ins_voc = GetInstructionVocabulary(&I, *(I.getParent()->getParent()), TD, instr_value_to_dim_, alloca_map_);
     Vocabulary state_voc, temp_voc;
     UnionVocabularies(pre_post_voc, ins_live, temp_voc);
     UnionVocabularies(temp_voc, ins_voc, state_voc);
     state_->AddVocabulary(state_voc);
     state_->Project(state_voc);
     
     DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                    abstract_domain::print(std::cout << "\nins_live_before:", ins_live_before);
                    abstract_domain::print(std::cout << "\nins_live_after:", ins_live_after);
                    abstract_domain::print(std::cout << "\nins_voc:", ins_live_after);
                    abstract_domain::print(std::cout << "\nstate_voc:", ins_live_after););
     

     switch (I.getOpcode()) {
     default: llvm_unreachable("Unknown instruction type encountered!");
       // Build the switch statement using the Instruction.def file...
#define HANDLE_INST(NUM, OPCODE, CLASS)                     \
       case Instruction::OPCODE: return                     \
         abstractExecute##OPCODE(static_cast<CLASS&>(I));
#include "llvm/IR/Instruction.def"
    }
     DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                    std::cout << std::endl << "state_ after executing " << getName(&I) << " is:" << std::endl;
                    state_->print(std::cout) << std::endl;);
  }

  //===----------------------------------------------------------------------===//
  //                     Terminator Instruction Implementations
  //===----------------------------------------------------------------------===//

  void WrappedDomainBBAbsTransCreator::abstractExecuteRet(ReturnInst &I) {
    assert(succ_ == nullptr); // Ensure that the successor is NULL

    // Save away the return value... (if we are not 'ret void' or the return not part of vocabulary)
    DimensionKey k;
    if (I.getNumOperands() && CreateDimensionFromType(std::string("return_") + getName(I.getParent()->getParent()), I.getReturnValue()->getType(), k, Version(1))) {
      WrappedDomain_Int R = getOperandValue(I.getReturnValue());
      SetDimensionInState(k, R);
    }
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSwitch(SwitchInst &I) {
    Value* Cond = I.getCondition();
    // Type *ElTy = Cond->getType();
    WrappedDomain_Int CondVal = getOperandValue(Cond);

    // Check to see if any of the cases match...
    // BasicBlock *Dest = nullptr;
    for (SwitchInst::CaseIt i = I.case_begin(), e = I.case_end(); i != e; ++i) {
      if(succ_ == cast<BasicBlock>(i.getCaseSuccessor())) {
        WrappedDomain_Int CaseVal = getOperandValue(i.getCaseValue());

        ref_ptr<BitpreciseWrappedAbstractValue> switch_cond; 
        switch_cond = CondVal.abs_equal(CaseVal);
        Meet(switch_cond);
        return;
      }
    }

    // No switch conditions available for defalt dest
    assert(succ_ == I.getDefaultDest());
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteBr(BranchInst &I) {
    BasicBlock *Dest;

    Dest = I.getSuccessor(0);          // Uncond branches have a fixed dest...
    if (!I.isUnconditional()) {
      // Succ should be one of the two branch conditions
      assert((succ_ == I.getSuccessor(0)) || (succ_ == I.getSuccessor(1)));

      Value *Cond = I.getCondition(); 
    
      bool found = false;
      std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > abs_cond = GetBoolValue(Cond, found);
      if(found) {
        if(succ_ == I.getSuccessor(0)) {
          DEBUG_PRINTING(DBG_PRINT_DETAILS, abs_cond.first->print(std::cout << "\nabs_cond.first:"););
          Meet(abs_cond.first);
        } else {
          DEBUG_PRINTING(DBG_PRINT_DETAILS, abs_cond.second->print(std::cout << "\nabs_cond.second:"););
          Dest = I.getSuccessor(1);
          Meet(abs_cond.second);
        }
        DEBUG_PRINTING(DBG_PRINT_EVERYTHING, print(std::cout << "\nstate_ after meet in Br:"););
      }
      else {
        // TODO: Handle cond_val as a cond_val != zero as a boolean value in case cond_val is boolean
        WrappedDomain_Int cond_val = getOperandValue(Cond);
        WrappedDomain_Int zero = cond_val.of_const(0);

        ref_ptr<BitpreciseWrappedAbstractValue> br_cond;
        if(succ_ == I.getSuccessor(0)) {
          br_cond = cond_val.abs_not_equal(zero); // succ is true successor
        } else {
          Dest = I.getSuccessor(1);
          br_cond = cond_val.abs_equal(zero); // succ is false successor
        }
        Meet(br_cond);
      }
    }
    assert(succ_ == Dest);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteIndirectBr(IndirectBrInst &I) {
    assert(false && "Cannot handle indirect branches.");
  }

  // Check to see if this is a model function. SVCOMP special functions such as 
  // __VERIFIER_assume and __VERIFIER_error are treated as models as well.
  // Additionally, f is a model when it is null or when it is an indirect call.
  // Note that the __VERIFIER_assert is handled specially by WrappedDomainWPDSCreator
  bool WrappedDomainBBAbsTransCreator::isModel(const Function *f) const {
    bool is_decl = (!f || (f && f->isDeclaration()));
    if(is_decl)
      return true;

    // Handle __VERIFIER* functions
    if(f->getName() == "__VERIFIER_assume" || f->getName() == "__VERIFIER_error" || f->getName() == "__VERIFIER_nondet_int")
      return true;
    return false;
  }


  // Creates the merge function for non-model functions
  wali::IMergeFn* WrappedDomainBBAbsTransCreator::obtainMergeFunc(llvm::CallSite& cs) {
    utils::Timer timer("obtainMergeFunction:", std::cout, true);

    // Check to see if this is a non-model function call
    assert (!isModel(cs.getCalledFunction()));

    Function *called_func = cs.getCalledFunction();
    Type* t = called_func->getReturnType();
    DimensionKey ret_key_callee, ret_key_caller;
    bool b = CreateDimensionFromType(std::string("return_") + getName(called_func),
                                     t,
                                     ret_key_callee,
                                     Version(1));
    if(b) {
      value_to_dim_t::const_iterator vec_it = findByValue(instr_value_to_dim_, cs.getInstruction());

      assert(vec_it != instr_value_to_dim_.end());
      ret_key_caller = vec_it->second;
    } else {
      ret_key_caller = DUMMY_KEY;
      ret_key_callee = DUMMY_KEY;
    }


    value_to_dim_t temp_instr_value_to_dim;

    Vocabulary arg_voc_cld_func = CreateFunctionArgumentVocabulary(called_func, Version(1), temp_instr_value_to_dim);
    std::map<Value*, std::pair<DimensionKey, unsigned> > temp_alloca_map;
    Vocabulary alloca_voc_cld_func = CreateAllocaVocabulary(called_func, Version(1), getDataLayout(), temp_alloca_map);

    Vocabulary post_voc;
    UnionVocabularies(arg_voc_cld_func, alloca_voc_cld_func, post_voc);
    state_->AddVocabulary(post_voc);

    Function::arg_iterator fargit = called_func->arg_begin();
    for (CallSite::arg_iterator argit = cs.arg_begin(), e = cs.arg_end(); argit != e && fargit != called_func->arg_end(); ++argit, fargit++) {
      std::stringstream ss;
      ss << fargit->getArgNo();
      std::string argprefix = getName(called_func) + std::string("_") + std::string("arg") + ss.str();

      WrappedDomain_Int caller_arg_val = getOperandValue(*argit);

      DimensionKey callee_arg_k;
      value_to_dim_t::const_iterator vec_it = findByValue(temp_instr_value_to_dim, fargit);

      if(vec_it != temp_instr_value_to_dim.end()) {
        callee_arg_k = vec_it->second;
        WrappedDomain_Int caller_arg_val = getOperandValue(*argit);
        ref_ptr<BitpreciseWrappedAbstractValue> assign_k = caller_arg_val.assign_to_two_voc(callee_arg_k, state_->GetVocabulary());
        state_->Meet(assign_k.get_ptr());
        
      }
    }

    // Create the merge function
    Vocabulary global_voc0 = CreateGlobalVocabulary(getModule(), Version(0), temp_instr_value_to_dim);
    Vocabulary global_voc1 = abstract_domain::replaceVersion(global_voc0, 0, 1);
    ref_ptr<AbstractValue> state_cp = state_->Copy();
    ref_ptr<BitpreciseWrappedAbstractValue> state_cp_bpw = GetBitpreciseWrappedState(state_cp);
    wali::IMergeFn* mf = new MergeFunction(global_voc1, ret_key_callee, ret_key_caller, new AvSemiring(state_cp_bpw.get_ptr(), getName(cs.getInstruction()), getName((BasicBlock*)cs.getCalledFunction()->begin())));

    return mf;
  }

  // Most of the callsites are handled by WrappedDomainWPDSCreator.
  // This function ignores callsites except the model call __VERIFIER_assume
  void WrappedDomainBBAbsTransCreator::abstractExecuteCallSite(llvm::CallSite CS) {
    Function *called_func = CS.getCalledFunction();

    // Handle __VERIFIER_assume function
    if(called_func && called_func->getName() == "__VERIFIER_assume") {
      CallSite::arg_iterator argit = CS.arg_begin();

      WrappedDomain_Int arg_val = getOperandValue(*argit);
      WrappedDomain_Int one = arg_val.of_const(1);

      ref_ptr<BitpreciseWrappedAbstractValue> assume_cond = arg_val.abs_equal(one);
      Meet(assume_cond);
    }
  }

  //===----------------------------------------------------------------------===//
  //                    Binary Instruction Implementations
  //===----------------------------------------------------------------------===//
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFAdd(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) const {
    // Float operations not supported in limited disj poly
    return WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFSub(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) const {
    // Float operations not supported in limited disj poly
    return WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFMul(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) const {
    // Float operations not supported in limited disj poly
    return WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFDiv(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) const {
    // Float operations not supported in limited disj poly
    return WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFRem(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) const {
    // Float operations not supported in limited disj poly
    return WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteAdd(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSub(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteMul(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFAdd(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFSub(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFMul(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFDiv(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFRem(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteUDiv(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSDiv(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteURem(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSRem(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteAnd(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteOr(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteXor(BinaryOperator &I) {
    abstractExecuteBinaryOperator(I);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteBinaryOperator(BinaryOperator &I) {
    Type *Ty    = I.getOperand(0)->getType();

    // Handle special case when both operands are bool and the operator is a boolean op
    DimensionKey k;
    bool op0_handled_dim = CreateDimensionFromValue("inst", I.getOperand(0), k, Version(1));
    if(op0_handled_dim && (GetBitsizeFromType(Ty) == utils::one)) {
      bool inboolstore = false;
      std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > b0 = GetBoolValue(I.getOperand(0), inboolstore);
      if(inboolstore) {
        std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > b1 = GetBoolValue(I.getOperand(1), inboolstore);
        if(inboolstore) {
          bool bool_op = true;
          std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > R;
          switch (I.getOpcode()) {
          default:
            bool_op = false;
            break;
          case Instruction::And:   
            // Given (b0,!b0) and (b1,!b1), their AND is pair (b0^b1, !(b0^b1)), ie (b0^b1, !b0 V !b1)
            R.first = WrappedDomain_Int::boolean_and(b0.first, b1.first); 
            R.second = WrappedDomain_Int::boolean_or(b0.second, b1.second); 
            break;
          case Instruction::Or:
            // Given (b0,!b0) and (b1,!b1), their OR is pair (b0 V b1, !(b0 V b1)), ie (b0 V b1, !b0 ^ !b1)
            R.first = WrappedDomain_Int::boolean_or(b0.first, b1.first); 
            R.second = WrappedDomain_Int::boolean_and(b0.second, b1.second); 
            DEBUG_PRINTING(DBG_PRINT_DETAILS,
                           std::cout << "\nIn OR operation:" << std::flush;
                           b0.first->print(std::cout << "\nb0.first:");
                           b0.second->print(std::cout << "\nb0.second:");
                           b1.first->print(std::cout << "\nb1.first:");
                           b1.second->print(std::cout << "\nb1.second:");
                           R.first->print(std::cout << "\nR.first:");
                           R.second->print(std::cout << "\nR.second:"););
            break;
          case Instruction::Xor: 
            // Given (b0,!b0) and (b1,!b1), their XOR is pair ((b0^!b1) V (!b0^b1), (b0^b1) V (!b0^!b1))
            R.first = WrappedDomain_Int::boolean_or(WrappedDomain_Int::boolean_and(b0.first, b1.second),
                                                    WrappedDomain_Int::boolean_and(b0.second, b1.first));
            R.second = WrappedDomain_Int::boolean_or(WrappedDomain_Int::boolean_and(b0.first, b1.first),
                                                     WrappedDomain_Int::boolean_and(b0.second, b1.second));
            break;
          }
          if(bool_op) {
            SetBoolValue(&I, R);
            SetValue(&I, GetBoolValueAsBitsize(R, utils::one));
            return;
          }
        }
      }
    }

    
    WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));
    WrappedDomain_Int R = WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);

    // First process vector operation
    if (Ty->isVectorTy()) {
      // Do nothing cannot handle vector or aggregates
    } else {
      switch (I.getOpcode()) {
      default:
        dbgs() << "Don't know how to handle this binary operator!\n-->" << I;
        llvm_unreachable(nullptr);
        break;
      case Instruction::Add:   R = Src1.plus(Src2); break;
      case Instruction::Sub:   R = Src1.minus(Src2); break;
      case Instruction::Mul:   R = Src1.times(Src2); break;
      case Instruction::FAdd:  R = abstractExecuteFAdd(Src1, Src2, Ty); break;
      case Instruction::FSub:  R = abstractExecuteFSub(Src1, Src2, Ty); break;
      case Instruction::FMul:  R = abstractExecuteFMul(Src1, Src2, Ty); break;
      case Instruction::FDiv:  R = abstractExecuteFDiv(Src1, Src2, Ty); break;
      case Instruction::FRem:  R = abstractExecuteFRem(Src1, Src2, Ty); break;
      case Instruction::UDiv:  R = Src1.udiv(Src2); break;
      case Instruction::SDiv:  R = Src1.sdiv(Src2); break;
      case Instruction::URem:  R = Src1.umod(Src2); break;
      case Instruction::SRem:  R = Src1.smod(Src2); break;
      case Instruction::And:   R = Src1.bitwise_and(Src2); break;
      case Instruction::Or:    R = Src1.bitwise_or(Src2); break;
      case Instruction::Xor:   R = Src1.bitwise_xor(Src2); break;
      }
    }
    SetValue(&I, R);
  }


  /***************************** Handle ICMP instruction **************************************/
  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_EQ(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(true/*is_signed*/);
    Src2.wrap(true/*is_signed*/);

    ret.first = Src1.abs_equal(Src2); // True case
    ret.second = Src1.abs_not_equal(Src2); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_NE(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(true/*is_signed*/);
    Src2.wrap(true/*is_signed*/);

    ret.first = Src1.abs_not_equal(Src2); // True case
    ret.second = Src1.abs_equal(Src2); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_ULT(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;

    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(false/*is_signed*/);
    Src2.wrap(false/*is_signed*/);

    ret.first = Src1.less_than_unsigned(Src2); // True case
    ret.second = Src2.less_than_or_equal_unsigned(Src1); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_SLT(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(true/*is_signed*/);
    Src2.wrap(true/*is_signed*/);

    ret.first = Src1.less_than_signed(Src2); // True case
    ret.second = Src2.less_than_or_equal_signed(Src1); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_UGT(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(false/*is_signed*/);
    Src2.wrap(false/*is_signed*/);

    ret.first = Src2.less_than_unsigned(Src1); // True case
    ret.second = Src1.less_than_or_equal_unsigned(Src2); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_SGT(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(true/*is_signed*/);
    Src2.wrap(true/*is_signed*/);

    ret.first = Src2.less_than_signed(Src1); // True case
    ret.second = Src1.less_than_or_equal_signed(Src2); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_ULE(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(false/*is_signed*/);
    Src2.wrap(false/*is_signed*/);

    ret.first = Src1.less_than_or_equal_unsigned(Src2); // True case
    ret.second = Src2.less_than_unsigned(Src1); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_SLE(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(true/*is_signed*/);
    Src2.wrap(true/*is_signed*/);

    ret.first = Src1.less_than_or_equal_signed(Src2); // True case
    ret.second = Src2.less_than_signed(Src1); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_UGE(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(false/*is_signed*/);
    Src2.wrap(false/*is_signed*/);

    ret.first = Src2.less_than_or_equal_unsigned(Src1); // True case
    ret.second = Src1.less_than_unsigned(Src2); // False case
    return ret;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteICMP_SGE(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > ret;
    // Avoid multiple calls to wrap, only added for performance
    Src1.wrap(true/*is_signed*/);
    Src2.wrap(true/*is_signed*/);

    ret.first = Src2.less_than_or_equal_signed(Src1); // True case
    ret.second = Src1.less_than_signed(Src2); // False case
    return ret;
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteICmp(ICmpInst &I) {
    utils::Timer timer(std::string("ICMP_timer for ") + getName((Instruction*)&I) + std::string(":"), std::cout, false);
    Type *Ty    = I.getOperand(0)->getType();
    WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > R;   // Result
  
    switch (I.getPredicate()) {
    case ICmpInst::ICMP_EQ:  R = abstractExecuteICMP_EQ(Src1,  Src2, Ty); break;
    case ICmpInst::ICMP_NE:  R = abstractExecuteICMP_NE(Src1,  Src2, Ty); break;
    case ICmpInst::ICMP_ULT: R = abstractExecuteICMP_ULT(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_SLT: R = abstractExecuteICMP_SLT(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_UGT: R = abstractExecuteICMP_UGT(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_SGT: R = abstractExecuteICMP_SGT(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_ULE: R = abstractExecuteICMP_ULE(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_SLE: R = abstractExecuteICMP_SLE(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_UGE: R = abstractExecuteICMP_UGE(Src1, Src2, Ty); break;
    case ICmpInst::ICMP_SGE: R = abstractExecuteICMP_SGE(Src1, Src2, Ty); break;
    default:
      dbgs() << "Don't know how to handle this ICmp predicate!\n-->" << I;
      llvm_unreachable(nullptr);
    }

    DEBUG_PRINTING(DBG_PRINT_DETAILS,
                   std::cout << "\nIn WrappedDomainBBAbsTransCreator::abstractExecuteICMP with I:" << std::flush;
                   I.print(outs()); 
                   Src1.print(std::cout << "\nSrc1:");
                   Src2.print(std::cout << "\nSrc2:");
                   R.first->print(std::cout << "\nR.first is:");
                   R.second->print(std::cout << "\nR.second is:"););
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nICmp time before setting:" << timer.elapsed(););
 
    SetBoolValue(&I, R);
    SetValue(&I, GetBoolValueAsBitsize(R, utils::one));
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nICmp time after setting:" << timer.elapsed(););
  }
  /***************************** End ICMP instruction **************************************/

  /***************************** Execute FCMP instruction **************************************/
  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_OEQ(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_ONE(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_OLE(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_OGE(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_OLT(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_OGT(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_UEQ(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_UNE(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_ULE(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_UGE(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_ULT(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_UGT(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_ORD(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_UNO(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                          Type *Ty) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteFCMP_BOOL(WrappedDomain_Int Src1, WrappedDomain_Int Src2,
                           Type *Ty, bool is_false) {
    // Cannot handle floating point instructions. Return pair top, top
    ref_ptr<BitpreciseWrappedAbstractValue> top = Src1.get_one_voc_relation();
    return std::make_pair(top, top);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFCmp(FCmpInst &I) {
    Type *Ty    = I.getOperand(0)->getType();
    WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > R;   // Result
  
    switch (I.getPredicate()) {
    default:
      dbgs() << "Don't know how to handle this FCmp predicate!\n-->" << I;
      llvm_unreachable(nullptr);
      break;
    case FCmpInst::FCMP_FALSE: R = abstractExecuteFCMP_BOOL(Src1, Src2, Ty, false); 
      break;
    case FCmpInst::FCMP_TRUE:  R = abstractExecuteFCMP_BOOL(Src1, Src2, Ty, true); 
      break;
    case FCmpInst::FCMP_ORD:   R = abstractExecuteFCMP_ORD(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_UNO:   R = abstractExecuteFCMP_UNO(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_UEQ:   R = abstractExecuteFCMP_UEQ(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_OEQ:   R = abstractExecuteFCMP_OEQ(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_UNE:   R = abstractExecuteFCMP_UNE(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_ONE:   R = abstractExecuteFCMP_ONE(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_ULT:   R = abstractExecuteFCMP_ULT(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_OLT:   R = abstractExecuteFCMP_OLT(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_UGT:   R = abstractExecuteFCMP_UGT(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_OGT:   R = abstractExecuteFCMP_OGT(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_ULE:   R = abstractExecuteFCMP_ULE(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_OLE:   R = abstractExecuteFCMP_OLE(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_UGE:   R = abstractExecuteFCMP_UGE(Src1, Src2, Ty); break;
    case FCmpInst::FCMP_OGE:   R = abstractExecuteFCMP_OGE(Src1, Src2, Ty); break;
    }
 
    SetBoolValue(&I, R);
  }
  /***************************** End FCMP instruction **************************************/


  //===----------------------------------------------------------------------===//
  //                     Memory Instruction Implementations
  //===----------------------------------------------------------------------===//
  void WrappedDomainBBAbsTransCreator::abstractExecuteAlloca(AllocaInst &I) {
    // Already handled when the vocabulary is created
    // This is a noop instruction essentially
    return;
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteLoad(LoadInst &I) {
    Type* Ty = I.getType();

    Value* v = I.getPointerOperand();
    std::map<Value*, std::pair<DimensionKey, unsigned> >::iterator it = alloca_map_.find(v);

    if(it != alloca_map_.end()) {
      std::pair<DimensionKey, unsigned> p = it->second;
      const unsigned LoadBytes = TD.getTypeStoreSize(Ty);
      // Number of bytes match, it is okay to load this value as a WrappedDomain_Int
      if(LoadBytes == p.second) {
        WrappedDomain_Int Result = WrappedDomain_Int::of_variable(state_, abstract_domain::replaceVersion(p.first, 0, 1), av_prevoc_->GetVocabulary());
        SetValue(&I, Result);
      }
    }
    else {
      // Do nothing since this store is not done on an alloca variable
    }

    if (I.isVolatile())
      dbgs() << "Volatile load " << I;
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteStore(StoreInst &I) {
    WrappedDomain_Int Val = getOperandValue(I.getValueOperand());
    Value* PtrDest = I.getPointerOperand();

    std::map<Value*, std::pair<DimensionKey, unsigned> >::iterator it = alloca_map_.find(PtrDest);

    Type* Ty = I.getOperand(0)->getType();
    const unsigned StoreBytes = TD.getTypeStoreSize(Ty);

    if(it != alloca_map_.end()) {
      std::pair<DimensionKey, unsigned> p = it->second;
      // Number of bytes match, it is okay to load this value as a WrappedDomain_Int
      if(StoreBytes == p.second) {
        SetDimensionInState(abstract_domain::replaceVersion(p.first, 0, 1), Val);
      } else {
        // Set this key to top as the number of bytes updated don't match
        WrappedDomain_Int top = WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
        SetDimensionInState(abstract_domain::replaceVersion(p.first, 0, 1), top);
      }
    }
    else {
      // Do nothing since this store is not done on an alloca variable
    }

  }

  // Cannot handle it right now
  //
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteGEPOperation(Value *Ptr, gep_type_iterator I, gep_type_iterator E) const {
    assert(Ptr->getType()->isPointerTy() &&
           "Cannot getElementOffset of a nonpointer type!");
    return WrappedDomain_Int(GetBitsizeFromType(Ptr->getType()), av_prevoc_);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteGetElementPtr(GetElementPtrInst &I) {
    SetValue(&I, abstractExecuteGEPOperation(I.getPointerOperand(), gep_type_begin(I), gep_type_end(I)));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecutePHI(llvm::PHINode &PN) {
    // Set the appropriate value by looking at the correct BB parent
    for(unsigned i = 0; i < PN.getNumIncomingValues(); i++) {
      // Find if this is the correct predecessor
      BasicBlock* bb = PN.getIncomingBlock(i);
      if(bb != bb_)
        continue;
      Value* val = PN.getIncomingValue(i);

      WrappedDomain_Int Src = getOperandValue(val);
      SetValue(&PN, Src);
    }

  }
  // Return Top because our domain at the moment cannot deal with trunc instruction
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteTrunc(Value *SrcVal, Type *DstTy) const {
    DimensionKey dst_k;
    bool isdim = CreateDimensionFromType("temp", DstTy, dst_k, Version(0));
    if(isdim) {
      utils::Bitsize bitsize = dst_k.GetBitsize();
      WrappedDomain_Int Src = getOperandValue(SrcVal);
      return Src.trunc(bitsize);
    }
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteSExt(Value *SrcVal, Type *DstTy) const {
    DimensionKey dst_k;
    bool isdim = CreateDimensionFromType("temp", DstTy, dst_k, Version(0));
    if(isdim) {
      utils::Bitsize bitsize = dst_k.GetBitsize();

      // Handle special case when SrcVal is a bool value
      bool inboolstore = false;
      std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > bool_val = GetBoolValue(SrcVal, inboolstore);
      if(inboolstore) {
        return GetBoolValueAsBitsize(bool_val, bitsize);
      }

      // If not found in BoolStore, try to use zero_extend
      WrappedDomain_Int Src = getOperandValue(SrcVal);
      return Src.sign_extend(bitsize);
    }
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with zero extension
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteZExt(Value *SrcVal, Type *DstTy) const {
    DimensionKey dst_k;
    bool isdim = CreateDimensionFromType("temp", DstTy, dst_k, Version(0));
    if(isdim) {
      utils::Bitsize bitsize = dst_k.GetBitsize();

      // Handle special case when SrcVal is a bool value
      bool inboolstore = false;
      std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > bool_val = GetBoolValue(SrcVal, inboolstore);
      if(inboolstore) {
        return GetBoolValueAsBitsize(bool_val, bitsize);
      }

      // If not found in BoolStore, try to use zero_extend
      WrappedDomain_Int Src = getOperandValue(SrcVal);
      return Src.zero_extend(bitsize);
    }
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with floating trunc operator
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFPTrunc(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with floating extemsion operator
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFPExt(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with floating to unsigned int operator
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFPToUI(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with floating to signed int operator
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteFPToSI(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with unsigned int to floating point operator
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteUIToFP(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal with signed int to floating point operator
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteSIToFP(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  // Simply return the value of SrcVal. In any case, the only change is in the type of the value.
  // There is no change in the value
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteIntToPtr(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = GetValue(SrcVal);
    return Ret;
  }

  // Simply return the value of SrcVal. In any case, the only change is in the type of the value.
  // There is no change in the value
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecutePtrToInt(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = GetValue(SrcVal);
    return Ret;
  }

  // Return Top because our domain at the moment cannot deal qith bitcast operation
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteBitCast(Value *SrcVal, Type *DstTy) const {
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(DstTy), av_prevoc_);
    return Ret;
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteSelect(std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > Src1, WrappedDomain_Int Src2, WrappedDomain_Int Src3, const Type *Ty) const {
    if(Ty->isVectorTy()) {
      // Cannot handle vectors with any precison
    } else {
      WrappedDomain_Int zero_int = WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_).of_const(0);
      ref_ptr<BitpreciseWrappedAbstractValue> tr = Src1.first;
      WrappedDomain_Int tr_int = WrappedDomain_Int::of_relation(tr, GetBitsizeFromType(Ty));

      ref_ptr<BitpreciseWrappedAbstractValue> fl = Src1.second;
      WrappedDomain_Int fl_int = WrappedDomain_Int::of_relation(fl, GetBitsizeFromType(Ty));

      WrappedDomain_Int tr_and_Src2 = tr_int;
      tr_and_Src2.meet(Src2);

      WrappedDomain_Int fl_and_Src3 = fl_int;
      fl_and_Src3.meet(Src3);

      WrappedDomain_Int Ret = tr_and_Src2;
      Ret.join(fl_and_Src3);

      return Ret;
    }
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
    return Ret;
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::abstractExecuteSelect(WrappedDomain_Int Src1, WrappedDomain_Int Src2, WrappedDomain_Int Src3, const Type *Ty) const {
    if(Ty->isVectorTy()) {
      // Cannot handle vectors with any precison
    } else {
      WrappedDomain_Int zero_int = WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_).of_const(0);
      ref_ptr<BitpreciseWrappedAbstractValue> is_zero = Src1.abs_equal(zero_int);
      WrappedDomain_Int is_zero_int = WrappedDomain_Int::of_relation(is_zero, GetBitsizeFromType(Ty));

      ref_ptr<BitpreciseWrappedAbstractValue> is_nonzero = Src1.abs_not_equal(zero_int);
      WrappedDomain_Int is_nonzero_int = WrappedDomain_Int::of_relation(is_nonzero, GetBitsizeFromType(Ty));

      WrappedDomain_Int is_zero_and_Src3 = is_zero_int;
      is_zero_and_Src3.meet(Src3);

      WrappedDomain_Int is_nonzero_and_Src2 = is_nonzero_int;
      is_nonzero_and_Src2.meet(Src2);

      WrappedDomain_Int Ret = is_zero_and_Src3;
      Ret.join(is_nonzero_and_Src2);

      return Ret;
    }
    WrappedDomain_Int Ret = WrappedDomain_Int(GetBitsizeFromType(Ty), av_prevoc_);
    return Ret;
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteTrunc(TruncInst &I) {
    SetValue(&I, abstractExecuteTrunc(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSExt(SExtInst &I) {
    SetValue(&I, abstractExecuteSExt(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteZExt(ZExtInst &I) {
    SetValue(&I, abstractExecuteZExt(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFPTrunc(FPTruncInst &I) {
    SetValue(&I, abstractExecuteFPTrunc(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFPExt(FPExtInst &I) {
    SetValue(&I, abstractExecuteFPExt(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteUIToFP(UIToFPInst &I) {
    SetValue(&I, abstractExecuteUIToFP(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSIToFP(SIToFPInst &I) {
    SetValue(&I, abstractExecuteSIToFP(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFPToUI(FPToUIInst &I) {
    SetValue(&I, abstractExecuteFPToUI(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteFPToSI(FPToSIInst &I) {
    SetValue(&I, abstractExecuteFPToSI(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecutePtrToInt(PtrToIntInst &I) {
    SetValue(&I, abstractExecutePtrToInt(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteIntToPtr(IntToPtrInst &I) {
    SetValue(&I, abstractExecuteIntToPtr(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteBitCast(BitCastInst &I) {
    SetValue(&I, abstractExecuteBitCast(I.getOperand(0), I.getType()));
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteSelect(SelectInst &I) {
    const Type * Ty = I.getOperand(0)->getType();

    WrappedDomain_Int R(GetBitsizeFromType(Ty), av_prevoc_);
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));
    WrappedDomain_Int Src3 = getOperandValue(I.getOperand(2));

    // Try if operand 0 has a conditional abstract value as a pair of abstract value
    bool found = false;
    std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > abs_cond = GetBoolValue(I.getOperand(0), found);


    // If found we can be more precise
    if(found) {
      R = abstractExecuteSelect(abs_cond, Src2, Src3, Ty);
    } else {
      WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
      R = abstractExecuteSelect(Src1, Src2, Src3, Ty);
    }
    SetValue(&I, R);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteUnreachable(UnreachableInst &I) {
    report_fatal_error("Program executed an 'unreachable' instruction!");
  }

  //===----------------------------------------------------------------------===//
  //                 Miscellaneous Instruction Implementations
  //===----------------------------------------------------------------------===//

  void WrappedDomainBBAbsTransCreator::abstractExecuteShl(BinaryOperator &I) {
    WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));

    const Type *Ty = I.getType();
    WrappedDomain_Int Dest(GetBitsizeFromType(Ty), av_prevoc_);

    if (Ty->isVectorTy()) {
      // Cannot handle vectors
    } else {
      // scalar
      Dest = Src1.lshift(Src2);
    }

    SetValue(&I, Dest);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteLShr(BinaryOperator &I) {
    WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));

    const Type *Ty = I.getType();
    WrappedDomain_Int Dest(GetBitsizeFromType(Ty), av_prevoc_);

    if (Ty->isVectorTy()) {
      // Cannot handle vectors
    } else {
      // scalar
      Dest = Src1.rshift(Src2);
    }

    SetValue(&I, Dest);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteAShr(BinaryOperator &I) {
    WrappedDomain_Int Src1 = getOperandValue(I.getOperand(0));
    WrappedDomain_Int Src2 = getOperandValue(I.getOperand(1));
    const Type *Ty = I.getType();
    WrappedDomain_Int Dest(GetBitsizeFromType(Ty), av_prevoc_);

    if (Ty->isVectorTy()) {
      // Cannot handle vectors
    } else {
      // scalar
      Dest = Src1.rshift_arith(Src2);
    }

    SetValue(&I, Dest);
  }


  // TODO: Handle VAArg instructions. They are critical to generating good function summaries for
  // functions with variable arguments
  void WrappedDomainBBAbsTransCreator::abstractExecuteVAArg(VAArgInst &I) {
    WrappedDomain_Int Dest(GetBitsizeFromType(I.getType()), av_prevoc_);
    SetValue(&I, Dest);
  }

  // These following instructions deal with vector or aggregated types
  // Since Poly domain only tracks invariants betweeen numerical avariables, we simply ignore these
  // functions.
  // Cannot handle vector access instruction
  void WrappedDomainBBAbsTransCreator::abstractExecuteExtractElement(ExtractElementInst &I) {
    WrappedDomain_Int Dest(GetBitsizeFromType(I.getType()), av_prevoc_);
    SetValue(&I, Dest);
  }

  // Cannot handle vector update instruction
  void WrappedDomainBBAbsTransCreator::abstractExecuteInsertElement(InsertElementInst &I) {
    WrappedDomain_Int Dest(GetBitsizeFromType(I.getType()), av_prevoc_);
    SetValue(&I, Dest);
  }

  // Cannot handle shuffle update instructiom
  void WrappedDomainBBAbsTransCreator::abstractExecuteShuffleVector(ShuffleVectorInst &I){
    WrappedDomain_Int Dest(GetBitsizeFromType(I.getType()), av_prevoc_);
    SetValue(&I, Dest);
  }

  // Don't know how to deal with aggregate values
  void WrappedDomainBBAbsTransCreator::abstractExecuteExtractValue(ExtractValueInst &I) {
    WrappedDomain_Int Dest(GetBitsizeFromType(I.getType()), av_prevoc_);
    SetValue(&I, Dest);
  }

  void WrappedDomainBBAbsTransCreator::abstractExecuteInsertValue(InsertValueInst &I) {
    WrappedDomain_Int Dest(GetBitsizeFromType(I.getType()), av_prevoc_);
    SetValue(&I, Dest);
  }


  //===----------------------------------------------------------------------===//
  //                     Various Helper Functions
  //===----------------------------------------------------------------------===//
  void WrappedDomainBBAbsTransCreator::SetValue(Value* v, const WrappedDomain_Int& val) {
    value_to_dim_t::const_iterator vec_it = findByValue(instr_value_to_dim_, v);

    if(vec_it != instr_value_to_dim_.end()) {
      DimensionKey k = vec_it->second;
      DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                     abstract_domain::print(std::cout << "\nFound value as key ", k);
                     std::cout << ", calling setDimensionInstate:" << std::flush;);
      SetDimensionInState(k, val);
    }
  }

  void WrappedDomainBBAbsTransCreator::SetDimensionInState(DimensionKey k, const WrappedDomain_Int& IntVal) {
    DimensionKey k_prime = abstract_domain::replaceVersion(k, 0, 1);
    Vocabulary k_prime_voc; k_prime_voc.insert(k_prime);

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   print(std::cout << "\nstate_ before havoc:");
                   abstract_domain::print(std::cout << "\nk_prime_voc:", k_prime_voc););
    state_ = dynamic_cast<BitpreciseWrappedAbstractValue*>(state_->Havoc(k_prime_voc).get_ptr());
    Vocabulary state_voc = state_->GetVocabulary();
    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   print(std::cout << "\nstate_ after havoc:"););

    if(state_voc.find(k_prime) == state_voc.end()) {
      abstract_domain::print(std::cout << "\nError: Did not find k_prime:", k_prime) << " in state_voc.";
      abstract_domain::print(std::cout << "\nstate_voc:", state_voc);
      //assert(false);
    } else {
      ref_ptr<BitpreciseWrappedAbstractValue> ret_var
        = IntVal.assign_to_two_voc(k_prime, state_->GetVocabulary());
      DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                     ret_var->print(std::cout << "\nret_var:");
                     IntVal.print(std::cout << "\nIntVal:"););
      Meet(ret_var);
      DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                     print(std::cout << "\nstate_ after meet:"););
    }
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::GetValue(Value* v) const {
    DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS, 
                   std::cout << "\nIn GetValue, Value v:" << std::flush;
                   outs() << *v;);

    value_to_dim_t::const_iterator vec_it = findByValue(instr_value_to_dim_, v);

    if(vec_it != instr_value_to_dim_.end()) {
      DimensionKey k = vec_it->second;
      DimensionKey k_prime = k;
      k_prime = abstract_domain::replaceVersion(k, 0, 1);

      Vocabulary state_voc = state_->GetVocabulary();
      if(state_voc.find(k_prime) == state_voc.end()) {
        std::cout << "\nError: Did not find variable corresponding to value v:" << getValueName(v);
        abstract_domain::print(std::cout << "\nk_prime:", k_prime);
        abstract_domain::print(std::cout << "\nstate_voc:", state_voc);
        //assert(false);
      } else {
        DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS,
                       abstract_domain::print(std::cout << "\nFound variable corresponding to value, k:", k);
                       abstract_domain::print(std::cout << "\nk_prime:", k_prime););
        return WrappedDomain_Int::of_variable(state_, k_prime, av_prevoc_->GetVocabulary());
      }
    }
    return WrappedDomain_Int(GetBitsizeFromType(v->getType()), av_prevoc_);
  }

  // Perform vanilla meet of state with val 
  void WrappedDomainBBAbsTransCreator::Meet(const ref_ptr<BitpreciseWrappedAbstractValue>& val) {
    state_->VanillaMeet(val.get_ptr());
  }

  typename WrappedDomainBBAbsTransCreator::BoolVal_T
  WrappedDomainBBAbsTransCreator::GetBoolValue(Value* Cond, bool& found) const {
    std::map<Value*, BoolVal_T>::const_iterator it = val_to_abs_bool_store_.find(Cond);
    if(it != val_to_abs_bool_store_.end()) {
      found = true;
      return it->second;
    }
    
    ref_ptr<BitpreciseWrappedAbstractValue> top =
      new BitpreciseWrappedAbstractValue
      (av_prevoc_->Copy(), 
       BitpreciseWrappedAbstractValue::VocabularySignedness());

    // Check if the cond is a bool type, set found to false and return top
    DimensionKey cond_k;
    bool isdim = CreateDimensionFromType("temp", Cond->getType(), cond_k, Version(0));
    if(isdim && (cond_k.GetBitsize() == utils::one)) {
      // Check for simple constants true and false
      if(User* U = dyn_cast<User>(Cond)) {
        if(Constant* C = dyn_cast<Constant>(U)) {
          if (!isa<UndefValue>(C) && !dyn_cast<ConstantExpr>(C)) {
            // C is a simple constant
            switch (C->getType()->getTypeID()) {
            default:
              break;
            case Type::IntegerTyID: 
              {
                WrappedDomain_Int Result = GetAbstractIntFromAPInt(cast<ConstantInt>(C)->getValue());
                outs() << "\nCond:" << *Cond;
                std::cout << std::flush;
                Result.print(std::cout << "\nResult:");
                mpz_class val;
                if(Result.is_constant(val)) {
                  std::cout << "\nResult is constant:" << val;
                  found = true;
                  ref_ptr<BitpreciseWrappedAbstractValue> bot =
                    dynamic_cast<BitpreciseWrappedAbstractValue*>(top->Bottom().get_ptr());
                  if(val == 1)
                    return std::make_pair(top, bot);
                  else if(val == 0)
                    return std::make_pair(bot, top);
                  assert(false);
                }
              }
              break;
            }  
          }
        }
      }
    }

    found = false;
    return std::make_pair(top, top);
  }

  void WrappedDomainBBAbsTransCreator::SetBoolValue(Value* Cond, BoolVal_T& val) const {
    val_to_abs_bool_store_[Cond] = val;
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::GetBoolValueAsBitsize(BoolVal_T& bool_val, utils::Bitsize bitsize) const {
    WrappedDomain_Int tp(bitsize, av_prevoc_);
    WrappedDomain_Int oneval = tp.of_const(1);
    WrappedDomain_Int onerel = WrappedDomain_Int::of_relation(bool_val.first, bitsize);
    WrappedDomain_Int zeroval = tp.of_const(0);
    WrappedDomain_Int zerorel = WrappedDomain_Int::of_relation(bool_val.second, bitsize);
    oneval.meet(onerel);
    zeroval.meet(zerorel);
    oneval.join(zeroval);
    return oneval;
  }

  std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> >
  abstractExecuteCmp(unsigned predicate, WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, Type *Ty) {
    switch (predicate) {
    case ICmpInst::ICMP_EQ:    return abstractExecuteICMP_EQ(Src1, Src2, Ty);
    case ICmpInst::ICMP_NE:    return abstractExecuteICMP_NE(Src1, Src2, Ty);
    case ICmpInst::ICMP_UGT:   return abstractExecuteICMP_UGT(Src1, Src2, Ty);
    case ICmpInst::ICMP_SGT:   return abstractExecuteICMP_SGT(Src1, Src2, Ty);
    case ICmpInst::ICMP_ULT:   return abstractExecuteICMP_ULT(Src1, Src2, Ty);
    case ICmpInst::ICMP_SLT:   return abstractExecuteICMP_SLT(Src1, Src2, Ty);
    case ICmpInst::ICMP_UGE:   return abstractExecuteICMP_UGE(Src1, Src2, Ty);
    case ICmpInst::ICMP_SGE:   return abstractExecuteICMP_SGE(Src1, Src2, Ty);
    case ICmpInst::ICMP_ULE:   return abstractExecuteICMP_ULE(Src1, Src2, Ty);
    case ICmpInst::ICMP_SLE:   return abstractExecuteICMP_SLE(Src1, Src2, Ty);
    case FCmpInst::FCMP_ORD:   return abstractExecuteFCMP_ORD(Src1, Src2, Ty);
    case FCmpInst::FCMP_UNO:   return abstractExecuteFCMP_UNO(Src1, Src2, Ty);
    case FCmpInst::FCMP_OEQ:   return abstractExecuteFCMP_OEQ(Src1, Src2, Ty);
    case FCmpInst::FCMP_UEQ:   return abstractExecuteFCMP_UEQ(Src1, Src2, Ty);
    case FCmpInst::FCMP_ONE:   return abstractExecuteFCMP_ONE(Src1, Src2, Ty);
    case FCmpInst::FCMP_UNE:   return abstractExecuteFCMP_UNE(Src1, Src2, Ty);
    case FCmpInst::FCMP_OLT:   return abstractExecuteFCMP_OLT(Src1, Src2, Ty);
    case FCmpInst::FCMP_ULT:   return abstractExecuteFCMP_ULT(Src1, Src2, Ty);
    case FCmpInst::FCMP_OGT:   return abstractExecuteFCMP_OGT(Src1, Src2, Ty);
    case FCmpInst::FCMP_UGT:   return abstractExecuteFCMP_UGT(Src1, Src2, Ty);
    case FCmpInst::FCMP_OLE:   return abstractExecuteFCMP_OLE(Src1, Src2, Ty);
    case FCmpInst::FCMP_ULE:   return abstractExecuteFCMP_ULE(Src1, Src2, Ty);
    case FCmpInst::FCMP_OGE:   return abstractExecuteFCMP_OGE(Src1, Src2, Ty);
    case FCmpInst::FCMP_UGE:   return abstractExecuteFCMP_UGE(Src1, Src2, Ty);
    case FCmpInst::FCMP_FALSE: return abstractExecuteFCMP_BOOL(Src1, Src2, Ty, false);
    case FCmpInst::FCMP_TRUE:  return abstractExecuteFCMP_BOOL(Src1, Src2, Ty, true);
    default:
      dbgs() << "Unhandled Cmp predicate\n";
      llvm_unreachable(nullptr);
    }
  }

  // Print human-readable value by looking at it's subclass
  void printValue(Value* v) {
    if(Argument* a = dyn_cast<Argument>(v)) {
      std::cout << "arg_";
      std::cout << getValueName(a);
    }
    else if(BasicBlock* bb = dyn_cast<BasicBlock>(v)) {
      std::cout << "bb_";
      std::cout << getName(bb);
    }
    //  else if(InlineAsm* ia = dyn_cast<InlineAsm>(v)) {
    //  emul_out->os() << "inlineasm_" << ia->getName();
    //}
    else if(MetadataAsValue* mav = dyn_cast<MetadataAsValue>(v)) {
      std::cout << "metadataasvalue_" << mav->getName().str();
    }
    else if(User* u = dyn_cast<User>(v)) {
      if(Constant* c = dyn_cast<Constant>(u)) {
        std::cout << "const_" << c->getName().str();
      }
      else if(Instruction* i = dyn_cast<Instruction>(u)) {
        std::cout << "inst_" << getName(i);
      }
      else {
        std::cout << "operator_" << u->getName().str();
      }
    }
    else {
      std::cout << "unknown_" << v->getName().str();
    }
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::getConstantExprValue (ConstantExpr *CE) const {
    switch (CE->getOpcode()) {
    case Instruction::Trunc:
      return abstractExecuteTrunc(CE->getOperand(0), CE->getType());
    case Instruction::ZExt:
      return abstractExecuteZExt(CE->getOperand(0), CE->getType());
    case Instruction::SExt:
      return abstractExecuteSExt(CE->getOperand(0), CE->getType());
    case Instruction::FPTrunc:
      return abstractExecuteFPTrunc(CE->getOperand(0), CE->getType());
    case Instruction::FPExt:
      return abstractExecuteFPExt(CE->getOperand(0), CE->getType());
    case Instruction::UIToFP:
      return abstractExecuteUIToFP(CE->getOperand(0), CE->getType());
    case Instruction::SIToFP:
      return abstractExecuteSIToFP(CE->getOperand(0), CE->getType());
    case Instruction::FPToUI:
      return abstractExecuteFPToUI(CE->getOperand(0), CE->getType());
    case Instruction::FPToSI:
      return abstractExecuteFPToSI(CE->getOperand(0), CE->getType());
    case Instruction::PtrToInt:
      return abstractExecutePtrToInt(CE->getOperand(0), CE->getType());
    case Instruction::IntToPtr:
      return abstractExecuteIntToPtr(CE->getOperand(0), CE->getType());
    case Instruction::BitCast:
      return abstractExecuteBitCast(CE->getOperand(0), CE->getType());
    case Instruction::GetElementPtr:
      return abstractExecuteGEPOperation(CE->getOperand(0), gep_type_begin(CE),
                                         gep_type_end(CE));
    case Instruction::FCmp:
    case Instruction::ICmp: {
      WrappedDomain_Int op0 = getOperandValue(CE->getOperand(0));
      WrappedDomain_Int op1 = getOperandValue(CE->getOperand(1));
      std::pair<ref_ptr<BitpreciseWrappedAbstractValue>, ref_ptr<BitpreciseWrappedAbstractValue> > cmp_val =
        abstractExecuteCmp(CE->getPredicate(),
                           op0,
                           op1,
                           CE->getOperand(0)->getType());
      Value* V = (Value*)(&CE);
      SetBoolValue(V, cmp_val);
      return GetBoolValueAsBitsize(cmp_val, utils::one);
    }
    case Instruction::Select:
      return abstractExecuteSelect(getOperandValue(CE->getOperand(0)),
                                   getOperandValue(CE->getOperand(1)),
                                   getOperandValue(CE->getOperand(2)),
                                   CE->getOperand(0)->getType());
    default :
      break;
    }

    WrappedDomain_Int Op0 = getOperandValue(CE->getOperand(0));
    WrappedDomain_Int Op1 = getOperandValue(CE->getOperand(1));
    Type * Ty = CE->getOperand(0)->getType();
    switch (CE->getOpcode()) {
    case Instruction::Add:  return Op0.plus(Op1);
    case Instruction::Sub:  return Op0.minus(Op1);
    case Instruction::Mul:  return Op0.times(Op1);
    case Instruction::FAdd: return abstractExecuteFAdd(Op0, Op1, Ty);
    case Instruction::FSub: return abstractExecuteFSub(Op0, Op1, Ty);
    case Instruction::FMul: return abstractExecuteFMul(Op0, Op1, Ty);
    case Instruction::FDiv: return abstractExecuteFDiv(Op0, Op1, Ty);
    case Instruction::FRem: return abstractExecuteFRem(Op0, Op1, Ty);
    case Instruction::SDiv: return Op0.sdiv(Op1);
    case Instruction::UDiv: return Op0.udiv(Op1);
    case Instruction::URem: return Op0.umod(Op1);
    case Instruction::SRem: return Op0.smod(Op1);
    case Instruction::And:  return Op0.bitwise_and(Op1);
    case Instruction::Or:   return Op0.bitwise_or(Op1);
    case Instruction::Xor:  return Op0.bitwise_xor(Op1);
    case Instruction::Shl:  return Op0.lshift(Op1);
    case Instruction::LShr: return Op0.rshift(Op1);
    case Instruction::AShr: return Op0.rshift_arith(Op1);
   
    default:
      dbgs() << "Unhandled ConstantExpr: " << *CE << "\n";
      llvm_unreachable("Unhandled ConstantExpr");
    }
    return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
  }

  // \brief Converts a Constant* into a WrappedDomain_Int, including handling of
  // ConstantExpr values.
  WrappedDomain_Int WrappedDomainBBAbsTransCreator::getConstantValue(const Constant *C) const {
    // If its undefined, return the garbage.
    if (isa<UndefValue>(C)) {
      return WrappedDomain_Int(GetBitsizeFromType(C->getType()), av_prevoc_);
    }

    // Otherwise, if the value is a ConstantExpr...
    if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
      Constant *Op0 = CE->getOperand(0);
      switch (CE->getOpcode()) {
      case Instruction::GetElementPtr: {
        // Compute the index
        WrappedDomain_Int Result = getConstantValue(Op0);
        APInt Offset_ap(TD.getPointerSizeInBits(), 0);
        cast<GEPOperator>(CE)->accumulateConstantOffset(TD, Offset_ap);
        Offset_ap = Offset_ap.sextOrSelf(32);
        mpz_class offset_mpz = (unsigned int) Offset_ap.getLimitedValue();
        WrappedDomain_Int Offset = WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_).of_const(offset_mpz);
        return Result.plus(Offset);
      }
      case Instruction::Trunc: {
        WrappedDomain_Int GV = getConstantValue(Op0);
        uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
        return GV.trunc(utils::GetBitsize(BitWidth));
      }
      case Instruction::ZExt: {
        WrappedDomain_Int GV = getConstantValue(Op0);
        uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
        return GV.zero_extend(utils::GetBitsize(BitWidth));
      }
      case Instruction::SExt: {
        WrappedDomain_Int GV = getConstantValue(Op0);
        uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
        return GV.sign_extend(utils::GetBitsize(BitWidth));
      }
      case Instruction::FPTrunc: {
        // TODO: Cannot handle even floating point constants
        // WrappedDomain_Int GV = getConstantValue(Op0);
        return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
      }
      case Instruction::FPExt:{
        // TODO: Cannot handle even floating point constants
        // WrappedDomain_Int GV = getConstantValue(Op0);
        return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
      }
      case Instruction::UIToFP: {
        // TODO: Cannot handle even floating point constants
        // WrappedDomain_Int GV = getConstantValue(Op0);
        return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
      }
      case Instruction::SIToFP: {
        // TODO: Cannot handle even floating point constants
        // WrappedDomain_Int GV = getConstantValue(Op0);
        return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
      }
      case Instruction::FPToUI: // double->APInt conversion handles sign
      case Instruction::FPToSI: {
        // TODO: Cannot handle even floating point constants
        // WrappedDomain_Int GV = getConstantValue(Op0);
        return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
      }
      case Instruction::PtrToInt: {
        WrappedDomain_Int GV = getConstantValue(Op0);
        uint32_t PtrWidth = TD.getTypeSizeInBits(Op0->getType());

        // Only pointers of 32-bits can be handled
        if(PtrWidth != 32) {
          return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
        }

        uint32_t IntWidth = TD.getTypeSizeInBits(CE->getType());
        GV = GV.zero_extend_or_trunc(utils::GetBitsize(IntWidth));
        return GV;
      }
      case Instruction::IntToPtr: {
        WrappedDomain_Int GV = getConstantValue(Op0);
        uint32_t PtrWidth = TD.getTypeSizeInBits(CE->getType());

        // Only pointers of 32-bits can be handled
        if(PtrWidth != 32) {
          return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
        }

        uint32_t IntWidth = TD.getTypeSizeInBits(CE->getType());
        GV = GV.zero_extend_or_trunc(utils::GetBitsize(IntWidth));
        return GV;
      }
      case Instruction::BitCast: {
        // Cannot handle bitcast instruction
        return WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
      }
      case Instruction::Add:
      case Instruction::FAdd:
      case Instruction::Sub:
      case Instruction::FSub:
      case Instruction::Mul:
      case Instruction::FMul:
      case Instruction::UDiv:
      case Instruction::SDiv:
      case Instruction::URem:
      case Instruction::SRem:
      case Instruction::And:
      case Instruction::Or:
      case Instruction::Xor: {
        WrappedDomain_Int LHS = getConstantValue(Op0);
        WrappedDomain_Int RHS = getConstantValue(CE->getOperand(1));
        WrappedDomain_Int GV = WrappedDomain_Int(GetBitsizeFromType(CE->getType()), av_prevoc_);
        switch (CE->getOperand(0)->getType()->getTypeID()) {
        default: llvm_unreachable("Bad add type!");
        case Type::IntegerTyID:
          switch (CE->getOpcode()) {
          default: llvm_unreachable("Invalid integer opcode");
          case Instruction::Add: GV = LHS.plus(RHS); break;
          case Instruction::Sub: GV = LHS.minus(RHS); break;
          case Instruction::Mul: GV = LHS.times(RHS); break;
          case Instruction::UDiv: GV = LHS.udiv(RHS); break;
          case Instruction::SDiv: GV = LHS.sdiv(RHS); break;
          case Instruction::URem: GV = LHS.umod(RHS); break;
          case Instruction::SRem: GV = LHS.smod(RHS); break;
          case Instruction::And: GV = LHS.bitwise_and(RHS); break;
          case Instruction::Or: GV = LHS.bitwise_or(RHS); break;
          case Instruction::Xor: GV = LHS.bitwise_xor(RHS); break;
          }
          break;
        case Type::FloatTyID:
          // Do nothing. Cannot handle floating-point instruction.
          break;
        case Type::DoubleTyID:
          // Do nothing. Cannot handle double floating-point instruction.
          break;
        case Type::X86_FP80TyID:
        case Type::PPC_FP128TyID:
        case Type::FP128TyID:
          // Do nothing. Cannot handle floating-point instruction.
          break;
        }
        return GV;
      }
      default:
        break;
      }
    }

    // Otherwise, we have a simple constant.
    WrappedDomain_Int Result(GetBitsizeFromType(C->getType()), av_prevoc_);
    switch (C->getType()->getTypeID()) {
    case Type::FloatTyID:
      // Do nothing. Cannot handle floating-point instruction.
      break;
    case Type::DoubleTyID:
      // Do nothing. Cannot handle double floating-point instruction.
      break;
    case Type::X86_FP80TyID:
    case Type::FP128TyID:
    case Type::PPC_FP128TyID:
      // Do nothing. Cannot handle floating-point instruction.
      break;
    case Type::IntegerTyID:
      Result = GetAbstractIntFromAPInt(cast<ConstantInt>(C)->getValue());
      break;
    case Type::PointerTyID:
      if (isa<ConstantPointerNull>(C)) {
        Result = Result.of_const(0); // 0 address is NULL
      }
      else if (/*const Function *F = */dyn_cast<Function>(C)) {
        // Do nothing. Cannot handle function pointers.
      }
      else if (/*const GlobalVariable *GV = */dyn_cast<GlobalVariable>(C)) {
        // Do nothing. Cannot handle function pointers.
      }
      else
        llvm_unreachable("Unknown constant pointer type!");
      break;
    case Type::VectorTyID:
      // Do nothing. Cannot handle vector types.
      break;

    default:
      assert(false);
      //OS << "ERROR: Constant unimplemented for type: " << *C->getType();
    }

    return Result;
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::getOperandValue(Value *V) const {
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)) {
      WrappedDomain_Int i = getConstantExprValue(CE);
      i.UpdateIntSignedness();
      return i;
    } else if (Constant *CPV = dyn_cast<Constant>(V)) {
      WrappedDomain_Int i = getConstantValue(CPV);
      i.UpdateIntSignedness();
      return i;
      /*  } else if (GlobalValue *GV = dyn_cast<GlobalValue>(V)) { // Nothing special needs to be done for globals
          return top;*/ // GetValue is keeping track of global values as well
    } else {
      return GetValue(V);
    }
    return WrappedDomain_Int(GetBitsizeFromType(V->getType()), av_prevoc_);
  }

  std::pair<bool, mpz_class> WrappedDomainBBAbsTransCreator::getConstantOperandValue(Value *V) const {
    WrappedDomain_Int val = getOperandValue(V);
    mpz_class v;
    bool b = val.is_constant(v);
    return std::make_pair(b, v);
  }

  WrappedDomain_Int WrappedDomainBBAbsTransCreator::GetAbstractIntFromAPInt(const llvm::APInt C) const {
    switch(C.getBitWidth()) {
    case 64: {
      std::ostringstream ss;
      ss << C.getLimitedValue();
      std::string C_64 = ss.str();
      mpz_class mpz_val(C_64);
      WrappedDomain_Int wint(utils::sixty_four, av_prevoc_);
      return wint.of_const(mpz_val);
    }
    case 32: {
      mpz_class mpz_val = (unsigned int)C.getLimitedValue();
      WrappedDomain_Int wint(utils::thirty_two, av_prevoc_);
      return wint.of_const(mpz_val);
    }
    case 16: {
      mpz_class mpz_val = (unsigned short)C.getLimitedValue();
      WrappedDomain_Int wint(utils::sixteen, av_prevoc_);
      return wint.of_const(mpz_val);
    }
    case 8: {
      mpz_class mpz_val = (unsigned char)C.getLimitedValue();
      WrappedDomain_Int wint(utils::eight, av_prevoc_);
      return wint.of_const(mpz_val);
    }
    case 1:
      mpz_class mpz_val = C.getBoolValue();
      WrappedDomain_Int wint(utils::one, av_prevoc_);
      return wint.of_const(mpz_val);
    }
    return WrappedDomain_Int(utils::thirty_two, av_prevoc_);
  }

  std::ostream& WrappedDomainBBAbsTransCreator::print(std::ostream& o) const {
    state_->print(std::cout) << std::endl;
    std::cout << "\nBool store:";
    for(std::map<llvm::Value*, BoolVal_T>::const_iterator it = val_to_abs_bool_store_.begin();
        it != val_to_abs_bool_store_.end(); it++) {
      std::cout << "\n\nValue:" << getValueName(it->first);
      it->second.first->print(std::cout << "\nBool Int True:");
      it->second.second->print(std::cout << "\nBool Int False:");
    }
    return o;
  }

}
