#ifndef src_reinterp_wrapped_domain_WrappedDomainBBAbsTransCreator_hpp
#define src_reinterp_wrapped_domain_WrappedDomainBBAbsTransCreator_hpp

// This file provides a class which creates an abstract transformer 'a' given a BB 'bb' and its successor BB 'succ',
// and the vocabulary it is abstracting to. The abstract transformer created is a BitpreciseWrappedAbstractValue
// abstract value.
// 'a' abstract the concrete semantics of the transformer bb->succ

#include "llvm/IR/CallSite.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Analysis/LoopInfo.h"

#include "src/reinterp/wrapped_domain/WrappedDomain_Int.hpp" 
#include "wali/wpds/WPDS.hpp"
#include "wali/wfa/WFA.hpp"
#include "src/AbstractDomain/common/AvSemiring.hpp"

#include "llvm/Analysis/LoopPass.h"

namespace llvm {
  template<typename T> class generic_gep_type_iterator;
  class ConstantExpr;
  typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;
}

namespace llvm_abstract_transformer { 
class WrappedDomainWPDSCreator;
}

namespace llvm_abstract_transformer {
  // WrappedDomainBBAbsTransCreator
  //
  // This class provides methods to create abstract transformers (as AbstractValue)
  // 
  class WrappedDomainBBAbsTransCreator : public llvm::ExecutionEngine {
  public:
    friend class ::llvm_abstract_transformer::WrappedDomainWPDSCreator;

    typedef std::pair<ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>, 
                      ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> > BoolVal_T;
    typedef std::map<llvm::Value*, DimensionKey> value_to_dim_t;

  private:
    // Data members shared among all BBs
    llvm::DataLayout TD;
    llvm::IntrinsicLowering *IL;

    // BB specific data members

    // Current values of bb_ and succ_ being abstractly executed
    llvm::BasicBlock* bb_;
    llvm::BasicBlock* succ_;

    // Used to initialize state_ and bool values. 
    // This value is mainly used to find the base class on which to calculated a 
    // correct bit-precise wrapped representation of bool values and abstract transformers 
    // (for example, it can be pointset-powerset of polyhedra)
    // It is initialized for each BB to talk about its from vocabulary
    ref_ptr<abstract_value::AbstractValue> av_prevoc_;

    // A map from Value* (which is a alloca instruction to <DimensionKey, unsigned> pair
    // The first part of the pair is the key to a symbolic value)
    // This global variable is set by CreateVocabularyForFunction
    std::map<llvm::Value*, std::pair<DimensionKey, unsigned> > alloca_map_;

    // A map from Value* to a corresponding dimension key
    value_to_dim_t instr_value_to_dim_;

    // This used to store the result of cmp instruction, so that it can be used precisely by a
    // jmp instruction later
    // The two AbstractValue store the true and false results respectively
    mutable std::map<llvm::Value*, BoolVal_T> val_to_abs_bool_store_;

    // Starts with one at the constructor and modifies it according to the instructions in
    // BasicBlock
    ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> state_;

    bool add_array_bounds_check_;
  public:
    // av is used to create initial state
    explicit WrappedDomainBBAbsTransCreator(std::unique_ptr<llvm::Module> M, ref_ptr<abstract_value::AbstractValue> av, bool add_array_bounds_check);
    ~WrappedDomainBBAbsTransCreator();

    llvm::Module* getModule();
    ref_ptr<abstract_value::AbstractValue> GetState();
    ref_ptr<abstract_value::AbstractValue> GetPreVocAv();
    ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> GetBitpreciseWrappedState(ref_ptr<abstract_value::AbstractValue> st) const;
    void SetState(ref_ptr<abstract_value::AbstractValue> st);
    void SetSucc(llvm::BasicBlock* succ);

    const llvm::DataLayout& getDataLayout() const {return TD;}

    bool isModel(const llvm::Function* f) const;
    wali::IMergeFn* obtainMergeFunc(llvm::CallSite& cs);

    // Abstractly Execute Basic Block b, except the terminator instruction, with the given vocabularies
    // start executing the BasicBlock from the start instruction
    void abstractExecuteBasicBlock
    (llvm::BasicBlock* bb, 
     llvm::BasicBlock::iterator start,
     llvm::BasicBlock::iterator end,
     const std::map<llvm::Instruction *, Vocabulary>& insLiveBeforeMap,
     const std::map<llvm::Instruction *, Vocabulary>& insLiveAfterMap,
     const Vocabulary& pre_voc, 
     const Vocabulary& post_voc, 
     const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map, 
     const value_to_dim_t& instr_value_to_dim,
     bool initialize_state);

    void abstractExecuteInst
    (llvm::Instruction& I, 
     const std::map<llvm::Instruction *, Vocabulary>& insLiveBeforeMap, 
     const std::map<llvm::Instruction *, Vocabulary>& insLiveAfterMap, 
     const Vocabulary& pre_post_voc);

    // Opcode Implementations
    // All of these instructions should be the end of this BasicBlock as they have multiple
    // successor or perform call, branch or return
    // They assume that the successor is succ
    void abstractExecuteRet(llvm::ReturnInst &I);
    void abstractExecuteSwitch(llvm::SwitchInst &I);
    void abstractExecuteBr(llvm::BranchInst &I);
    void abstractExecuteIndirectBr(llvm::IndirectBrInst &I);

    void abstractExecuteCallSite(llvm::CallSite CS);
    void abstractExecuteCall(llvm::CallInst &I) { abstractExecuteCallSite (llvm::CallSite (&I)); }

    void abstractExecuteAdd(llvm::BinaryOperator &I);
    void abstractExecuteSub(llvm::BinaryOperator &I);
    void abstractExecuteMul(llvm::BinaryOperator &I);
    void abstractExecuteFAdd(llvm::BinaryOperator &I);
    void abstractExecuteFSub(llvm::BinaryOperator &I);
    void abstractExecuteFMul(llvm::BinaryOperator &I);
    void abstractExecuteFDiv(llvm::BinaryOperator &I);
    void abstractExecuteFRem(llvm::BinaryOperator &I);
    void abstractExecuteUDiv(llvm::BinaryOperator &I);
    void abstractExecuteSDiv(llvm::BinaryOperator &I);
    void abstractExecuteURem(llvm::BinaryOperator &I);
    void abstractExecuteSRem(llvm::BinaryOperator &I);
    void abstractExecuteAnd(llvm::BinaryOperator &I);
    void abstractExecuteOr(llvm::BinaryOperator &I);
    void abstractExecuteXor(llvm::BinaryOperator &I);

    void abstractExecuteBinaryOperator(llvm::BinaryOperator &I);
    void abstractExecuteICmp(llvm::ICmpInst &I);
    void abstractExecuteFCmp(llvm::FCmpInst &I);

    void abstractExecuteAlloca(llvm::AllocaInst &I);
    void abstractExecuteLoad(llvm::LoadInst &I);
    void abstractExecuteStore(llvm::StoreInst &I);
    void abstractExecuteGetElementPtr(llvm::GetElementPtrInst &I);
    void abstractExecutePHI(llvm::PHINode &PN);
    void abstractExecuteTrunc(llvm::TruncInst &I);
    void abstractExecuteZExt(llvm::ZExtInst &I);
    void abstractExecuteSExt(llvm::SExtInst &I);
    void abstractExecuteFPTrunc(llvm::FPTruncInst &I);
    void abstractExecuteFPExt(llvm::FPExtInst &I);
    void abstractExecuteUIToFP(llvm::UIToFPInst &I);
    void abstractExecuteSIToFP(llvm::SIToFPInst &I);
    void abstractExecuteFPToUI(llvm::FPToUIInst &I);
    void abstractExecuteFPToSI(llvm::FPToSIInst &I);
    void abstractExecutePtrToInt(llvm::PtrToIntInst &I);
    void abstractExecuteIntToPtr(llvm::IntToPtrInst &I);
    void abstractExecuteBitCast(llvm::BitCastInst &I);
    void abstractExecuteSelect(llvm::SelectInst &I);

    void abstractExecuteInvoke(llvm::InvokeInst &I) { abstractExecuteCallSite (llvm::CallSite (&I)); }
    void abstractExecuteUnreachable(llvm::UnreachableInst &I);

    void abstractExecuteShl(llvm::BinaryOperator &I);
    void abstractExecuteLShr(llvm::BinaryOperator &I);
    void abstractExecuteAShr(llvm::BinaryOperator &I);

    void abstractExecuteVAArg(llvm::VAArgInst &I);
    void abstractExecuteExtractElement(llvm::ExtractElementInst &I);
    void abstractExecuteInsertElement(llvm::InsertElementInst &I);
    void abstractExecuteShuffleVector(llvm::ShuffleVectorInst &I);

    void abstractExecuteExtractValue(llvm::ExtractValueInst &I);
    void abstractExecuteInsertValue(llvm::InsertValueInst &I);

    // Miscellaneous Instructions (Not handled)
    void abstractExecuteFence(llvm::FenceInst &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteAtomicCmpXchg(llvm::AtomicCmpXchgInst &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteAtomicRMW(llvm::AtomicRMWInst &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteAddrSpaceCast(llvm::AddrSpaceCastInst &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteUserOp1(llvm::Instruction &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteUserOp2(llvm::Instruction &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteResume(llvm::ResumeInst &I) {
      UWAssert::shouldNeverHappen();
    }

    void abstractExecuteLandingPad(llvm::LandingPadInst &I) {
      UWAssert::shouldNeverHappen();
    }

    // Not supported Executionengine functions
    llvm::GenericValue runFunction(llvm::Function* f, llvm::ArrayRef<llvm::GenericValue> arr) override {
      UWAssert::shouldNeverHappen();
      return llvm::GenericValue(0);
    }
 
    void* getPointerToNamedFunction(llvm::StringRef str, bool b) override {
      UWAssert::shouldNeverHappen();
      return nullptr;
    }

    void *getPointerToFunction(llvm::Function *F) override {
      UWAssert::shouldNeverHappen();
      return F;
    }

    void InitializeMembers
    (llvm::BasicBlock* bb, 
     llvm::BasicBlock::iterator start,
     llvm::BasicBlock::iterator end,
     const Vocabulary& pre_voc, 
     const Vocabulary& post_voc, 
     const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map, 
     const value_to_dim_t& instr_value_to_dim,
     bool initialize_state);

    std::ostream& print(std::ostream& o) const;
private:  // Helper functions  

    WrappedDomain_Int abstractExecuteGEPOperation(llvm::Value *Ptr, llvm::gep_type_iterator I, llvm::gep_type_iterator E) const;

    WrappedDomain_Int getConstantExprValue(llvm::ConstantExpr *CE) const;
    WrappedDomain_Int getConstantValue(const llvm::Constant *C) const;

    WrappedDomain_Int getOperandValue(llvm::Value *V) const;
    std::pair<bool, mpz_class> getConstantOperandValue(llvm::Value *V) const;

    WrappedDomain_Int abstractExecuteTrunc(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteSExt(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteZExt(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteFPTrunc(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteFPExt(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteFPToUI(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteFPToSI(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteUIToFP(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteSIToFP(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecutePtrToInt(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteIntToPtr(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteBitCast(llvm::Value *SrcVal, llvm::Type *DstTy) const;
    WrappedDomain_Int abstractExecuteCastOperation(llvm::Instruction::CastOps opcode, llvm::Value *SrcVal, llvm::Type *Ty) const;
    WrappedDomain_Int abstractExecuteSelect(std::pair<ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>, ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> > Src1, WrappedDomain_Int Src2, WrappedDomain_Int Src3, const llvm::Type *Ty) const;
    WrappedDomain_Int abstractExecuteSelect(WrappedDomain_Int Src1, WrappedDomain_Int Src2, WrappedDomain_Int Src3, const llvm::Type *Ty) const;


    WrappedDomain_Int abstractExecuteFAdd(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, llvm::Type *Ty) const;
    WrappedDomain_Int abstractExecuteFSub(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, llvm::Type *Ty) const;
    WrappedDomain_Int abstractExecuteFMul(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, llvm::Type *Ty) const;
    WrappedDomain_Int abstractExecuteFDiv(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, llvm::Type *Ty) const;
    WrappedDomain_Int abstractExecuteFRem(WrappedDomain_Int& Src1, WrappedDomain_Int& Src2, llvm::Type *Ty) const;


    void SetValue(llvm::Value* v, const WrappedDomain_Int& val);
    void SetDimensionInState(DimensionKey k, const WrappedDomain_Int& IntVal);

    WrappedDomain_Int GetValue(llvm::Value* v) const;

    // Perform meet of state with val (val might have wrapped vocabulary, 
    // so this function tries to preserv precision by soundly adding 
    // BoundingConstraints on Meet if it can
    void Meet(const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& val);

    BoolVal_T GetBoolValue(llvm::Value* Cond, bool& found) const;
    void SetBoolValue(llvm::Value* Cond, BoolVal_T& val) const;
    WrappedDomain_Int GetBoolValueAsBitsize(BoolVal_T& bool_val, utils::Bitsize bitsize) const;

    bool LoadIntFromMemory(CommonConcRTG::QFBVTerm32RefPtr Src, unsigned LoadBytes, DimensionKey& k) const;
    WrappedDomain_Int LoadValueFromMemory(CommonConcRTG::QFBVTerm32RefPtr Ptr, llvm::Type *Ty) const;

    void StoreIntToMemory(const WrappedDomain_Int& IntVal, CommonConcRTG::QFBVTerm32RefPtr Dest, unsigned StoreBytes);
    void StoreValueToMemory(const WrappedDomain_Int& Val, CommonConcRTG::QFBVTerm32RefPtr Ptr, llvm::Type *Ty);

    WrappedDomain_Int GetAbstractIntFromLLVMConstant(const llvm::Constant *C);
    WrappedDomain_Int GetAbstractIntFromAPInt(const llvm::APInt C) const;
  };

} // End llvm_abstract_transformer namespace

#endif // src_reinterp_wrapped_domain_WrappedDomainBBAbsTransCreator_hpp
