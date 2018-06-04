#include "src/reinterp/wrapped_domain/WrappedDomainWPDSCreator.hpp"
#include "src/reinterp/wrapped_domain/LlvmVocabularyUtils.hpp"
#include "llvm/IR/Function.h"
#include "src/AbstractDomain/common/AvSemiring.hpp"

#include "wali/wpds/ewpds/EWPDS.hpp"
#include "wali/wpds/fwpds/FWPDS.hpp"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/Analysis/LazyCallGraph.h"
#include "utils/timer/timer.hpp"

using namespace llvm;
using namespace abstract_domain;
using namespace llvm_abstract_transformer;

extern const char* cmdlineparam__filename;
extern bool cmdlineparam_allow_phis;

// Register live variable pass
namespace {
  char LiveVariableAnalysis::ID = 0;
  static llvm::RegisterPass<LiveVariableAnalysis> ref_live_variable_analysis("LiveVariableAnalysis", "Collects live variable information", true, true);
}

namespace {
class CollectLoopInfo: public FunctionPass {
private:
  std::map<const Function*, std::shared_ptr<LoopInfo> > loops_;
public:
  CollectLoopInfo() : FunctionPass(ID) {}

  static char ID;

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
  }

  void PrintLoop(std::ostream& o, const Loop* L) {
    o << "Loop header:" << L->getHeader()->getName().str();
    o << " depth:" << L->getLoopDepth();
    o << " numBackEdges:" << L->getNumBackEdges();
      
    // Go through subloops
    const std::vector<Loop*> subs = L->getSubLoops();
    if(subs.size() != 0) {
      o << "\n{Subloops:";
      for(std::vector<Loop*>::const_iterator it = subs.begin(); it != subs.end(); it++) {
        PrintLoop(o, *it);
      }
      if(debug_print_level >= DBG_PRINT_OVERVIEW)
        o << "}\n";
    }
  }

  virtual bool runOnFunction(Function& F) {
    std::shared_ptr<LoopInfo> LI(new LoopInfo());
    *LI = std::move(getAnalysis<LoopInfoWrapperPass>().getLoopInfo());
    loops_.insert(std::make_pair(&F, LI));
    for(LoopInfo::iterator it = LI->begin(); it != LI->end(); it++) {
      const Loop* L = *it;
      DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nFound an outer loop in collect loop info:\n";);
      DEBUG_PRINTING(DBG_PRINT_OVERVIEW, PrintLoop(std::cout, L););
    }
    return false;
  }

  std::map<const Function*, std::shared_ptr<LoopInfo>> getFunctionToLoopInfoMap() {
    return loops_;
  }
};
}

namespace {
char CollectLoopInfo::ID = 0;
static RegisterPass<CollectLoopInfo> ref_collect_loop_info("CollectLoopInfo", "Collects information on Loop", true, true);
}

WrappedDomainWPDSCreator::WrappedDomainWPDSCreator(std::unique_ptr<Module> M, ref_ptr<AbstractValue> av, std::string& filename, bool add_array_bounds_check) : bb_abs_trans_cr_(std::move(M), av, add_array_bounds_check), av_(av->Top()), filename_(filename), add_array_bounds_check_(add_array_bounds_check) {
  // State keys
  program_ = wali::getKey("program");
  min_max_voc_size = std::make_pair(100000u, 0u);
}

WrappedDomainWPDSCreator::~WrappedDomainWPDSCreator() {
}

llvm::Module* WrappedDomainWPDSCreator::getModule() {
  return bb_abs_trans_cr_.getModule();
}

std::set<wali::Key> WrappedDomainWPDSCreator::GetUnreachableKeys() const {
  return unreachable_keys_;
}

std::set<wali::Key> WrappedDomainWPDSCreator::GetUnreachableArrayBoundsCheckKeys() const {
  return unreachable_array_bounds_check_keys_;
}

std::pair<size_t, size_t> WrappedDomainWPDSCreator::GetMinMaxVocSize() const {
  return min_max_voc_size;
}

wali::Key WrappedDomainWPDSCreator::GetProgramKey() const {
  return program_;
}

bool WrappedDomainWPDSCreator::isModel(const Function* f) const {
  return bb_abs_trans_cr_.isModel(f);
}

wali::Key WrappedDomainWPDSCreator::mk_wpds_key(BasicBlock::iterator start, BasicBlock* bb, Function* f) {
  assert(start != bb->end());
  Instruction* start_ins = start;
  std::string func_name = getName(f);
  std::string bb_name = getName(bb);
  std::string ins_name = getName(start_ins);
  return wali::getKey(func_name + "_" + bb_name + "_" + ins_name);
}

wali::Key WrappedDomainWPDSCreator::mk_unique_wpds_key(BasicBlock* bb) {
  static unsigned unique_id = 0;
  unique_id++;
  std::string func_name = getName(bb->getParent());
  std::string bb_name = getName(bb);
  std::stringstream ss; ss << unique_id;
  return wali::getKey(func_name + "_" + bb_name + "_" + ss.str());
}

wali::Key WrappedDomainWPDSCreator::mk_wpds_unreachable_key(const CallSite& CS) {
  std::string func_name = getName(CS.getInstruction()->getParent()->getParent());
  std::string bb_name = getName(CS.getInstruction()->getParent());
  std::string cs_name = getName(CS.getInstruction());
  return wali::getKey("unreachable__" + func_name + "_" + bb_name + "_" + cs_name);
}

wali::Key WrappedDomainWPDSCreator::mk_exit_wpds_key(Function* f) {
  return wali::getKey("funcexit_" + getName(f));
}

wali::Key WrappedDomainWPDSCreator::mk_exit_wpds_dummy_key(Function* f, llvm::CallInst* ci) {
  return wali::getKey("funcexit_" + getName(f) + "_dummy_" + getName(ci));
}

bool is_loop_llvm_backedge(const Loop* L, const BasicBlock* bb, const BasicBlock* succ) {
  // (bb, succ) is a backedge for L if succ is the loop header of Land bb is in Loop L
  if(succ == L->getHeader() && L->contains(bb)) {
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                   std::cout << "Found llvm backedge (bb,succ): (" << getName(bb) << "," << 
                   getName(succ) << ")" << std::endl;);
    return true;
  }

  // Look into the subloops to see if it is a backedge for any of them
  const std::vector<Loop*> subs = L->getSubLoops();
  for(std::vector<Loop*>::const_iterator it = subs.begin(); it != subs.end(); it++) {
    if(is_loop_llvm_backedge(*it, bb, succ))
      return true;
  }
  return false;
}

bool WrappedDomainWPDSCreator::is_llvm_backedge(const BasicBlock* bb, const BasicBlock* succ) {
  std::shared_ptr<llvm::LoopInfo> linfo = func_loop_infos_.at(bb->getParent());
  for(LoopInfo::iterator lit = linfo->begin(); lit != linfo->end(); lit++) {
    const Loop* L = *lit;
    if(is_loop_llvm_backedge(L, bb, succ))
      return true;    
  }
  return false;
}

// This is a call basic block if the first instruction is a call instruction
// Returns the iterator just after the call node
BasicBlock::iterator WrappedDomainWPDSCreator::findNonModelCall(BasicBlock* bb, BasicBlock::iterator start, CallInst*& ci) const {
  ci = NULL;
  BasicBlock::iterator end = bb->end();
  while(start != end) {
    Instruction* I = start;
    CallInst* c = dynamic_cast<CallInst*>(I);
    if(c) {
      CallSite cs(c);
      if(!isModel(cs.getCalledFunction())) {
        ci = c;
        start++;
        return start;
      }
    }
    start++;
  }
  return start;
}

// Find if the basic block contains unreachable instructionThis is a call basic block if the first instruction is a call instruction
// Returns the iterator just after the call node
bool containsCallToTrap(BasicBlock* bb) {
  BasicBlock::iterator start = bb->begin();
  BasicBlock::iterator end = bb->end();
  while(start != end) {
    Instruction* I = start;
    CallInst* c = dynamic_cast<CallInst*>(I);
    if(c) {
      CallSite cs(c);
      Function* cf = cs.getCalledFunction();
      if(cf && cf->getName() == "llvm.trap")
        return true;
    }
    start++;
  }
  return false;
}

void WrappedDomainWPDSCreator::performReg2Mem() {
  std::cout << "\nPerforming reg2mem pass.";
  legacy::PassManager PM;
  PM.add(createDemoteRegisterToMemoryPass());
  PM.run(*getModule());
  std::cout << "......................................";
}
void WrappedDomainWPDSCreator::performMem2Reg() {
  std::cout << "\nPerforming mem2reg pass.";
  legacy::PassManager PM;
  PM.add(createPromoteMemoryToRegisterPass());
  PM.run(*getModule());
  std::cout << "......................................";
}

void WrappedDomainWPDSCreator::AddRuleToPds(ref_ptr<AbstractValue> state_cp, wali::Key from_key, wali::Key to_key, WideningType wty, Vocabulary& voc) {
  // bb_abs_trans_cr_.Reduce(state_cp);
  ref_ptr<BitpreciseWrappedAbstractValue> state_cp_bpw =
    bb_abs_trans_cr_.GetBitpreciseWrappedState(state_cp);
  state_cp_bpw->Project(voc);
  ref_ptr<AvSemiring> w = 
    new AvSemiring(state_cp_bpw.get_ptr(), wali::key2str(from_key), wali::key2str(to_key), wty);
  DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                 std::cout << std::endl << "Abstract transformer for (" << wali::key2str(from_key) << 
                 "," << wali::key2str(to_key) << ") is :";);
  DEBUG_PRINTING(DBG_PRINT_OVERVIEW, w->print(std::cout) << std::endl;);
  pds_->add_rule(program_, from_key, program_, to_key, w.get_ptr());
  UpdateMinMaxVocabularySize(voc.size());
}

void WrappedDomainWPDSCreator::AddDelta0RuleToPds(ref_ptr<AbstractValue> state_cp, wali::Key from_key, WideningType wty, Vocabulary& voc) {
  // bb_abs_trans_cr_.Reduce(state_cp);
  ref_ptr<BitpreciseWrappedAbstractValue> state_cp_bpw =
    bb_abs_trans_cr_.GetBitpreciseWrappedState(state_cp);
  state_cp_bpw->Project(voc);
  ref_ptr<AvSemiring> w = 
    new AvSemiring(state_cp_bpw.get_ptr(), wali::key2str(from_key), std::string("NULL"), wty);
  DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                 std::cout << std::endl << "Abstract transformer for bb (" << wali::key2str(from_key) << 
                 ",NULL) is :";);
  DEBUG_PRINTING(DBG_PRINT_OVERVIEW, w->print(std::cout) << std::endl;);
  pds_->add_rule(program_, from_key, program_, w.get_ptr());
  UpdateMinMaxVocabularySize(voc.size());
}

void WrappedDomainWPDSCreator::AddDelta2RuleToPds(ref_ptr<AbstractValue> state_cp, wali::Key from_key, wali::Key callee_entry_key, wali::Key to_key, WideningType wty, Vocabulary& voc, wali::IMergeFn* cf) {
  // bb_abs_trans_cr_.Reduce(state_cp);
  ref_ptr<BitpreciseWrappedAbstractValue> state_cp_bpw =
    bb_abs_trans_cr_.GetBitpreciseWrappedState(state_cp);
  state_cp_bpw->Project(voc);

  ref_ptr<AvSemiring> w_delta2 = new AvSemiring(state_cp_bpw, wali::key2str(from_key), wali::key2str(to_key) );

  DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                 std::cout << std::endl << "Abstract transformer for call bb (" << wali::key2str(from_key) << 
                 "," << wali::key2str(callee_entry_key) << " " << wali::key2str(to_key) << ") is :";);
  DEBUG_PRINTING(DBG_PRINT_OVERVIEW, w_delta2->print(std::cout) << std::endl;);
  
  wali::wpds::ewpds::EWPDS* ewpds = dynamic_cast<wali::wpds::ewpds::EWPDS*>(pds_);
  if(ewpds) {
    ewpds->add_rule(program_, from_key, program_, callee_entry_key, to_key, w_delta2.get_ptr(), cf); } else {
    wali::wpds::fwpds::FWPDS* fwpds = dynamic_cast<wali::wpds::fwpds::FWPDS*>(pds_);
    fwpds->add_rule(program_, from_key, program_, callee_entry_key, to_key, w_delta2.get_ptr(), cf);
  }
  UpdateMinMaxVocabularySize(w_delta2->GetAbstractValue()->GetVocabulary().size());
}

void WrappedDomainWPDSCreator::abstractExecuteBasicBlock
  (llvm::BasicBlock* bb,
   LiveVariableAnalysis* lva) {
  // Extract live variable analysis information from lva, and used it to determine the
  // vocabulary with which the BB will be called
  typedef LiveVariableAnalysis::BBtoVocMap BBtoVocMap;
  typedef LiveVariableAnalysis::InstrToVocMap InstrToVocMap;
  typedef LiveVariableAnalysis::AllocaMap AllocaMap;
  typedef LiveVariableAnalysis::value_to_dim_t value_to_dim_t;
  llvm::Function* f = bb->getParent();
  //std::map<llvm::Function*, BBtoVocMap>& liveBeforeMapMap = lva->liveBeforeMapMap;
  std::map<llvm::Function*, BBtoVocMap>& liveAfterMapMap = lva->liveAfterMapMap;
  std::map<llvm::Function*, InstrToVocMap>& insLiveBeforeMapMap = lva->insLiveBeforeMapMap;
  std::map<llvm::Function*, InstrToVocMap>& insLiveAfterMapMap = lva->insLiveAfterMapMap;
  std::map<llvm::Function*, AllocaMap>& allocaMapMap = lva->allocaMapMap;
  std::map<llvm::Function*, value_to_dim_t>& valueToDimMapMap = lva->valueToDimMapMap;

  //BBtoVocMap& liveBeforeMap = liveBeforeMapMap[f];
  BBtoVocMap& liveAfterMap = liveAfterMapMap[f];
  InstrToVocMap& insLiveBeforeMap = insLiveBeforeMapMap[f];
  InstrToVocMap& insLiveAfterMap = insLiveAfterMapMap[f];
  AllocaMap& allocaMap = allocaMapMap[f];
  value_to_dim_t& valueToDimMap = valueToDimMapMap[f];

  Vocabulary non_bb_pre_post_voc;
  TerminatorInst *t_inst = bb->getTerminator();

  // Put first non-phi instruction in crt_iter
  llvm::Instruction* first_non_phi_inst;
  llvm::BasicBlock::iterator crt_iter = bb->begin();
  llvm::BasicBlock::iterator bb_end = bb->end();
  while(crt_iter != bb_end) {
    llvm::Instruction* I = crt_iter;
    if(I->getOpcode() != llvm::Instruction::PHI) {   
      first_non_phi_inst = I;
      break;
    }
    crt_iter++;
  }
  assert(first_non_phi_inst != NULL);

  BasicBlock::iterator end_iter;
  wali::Key from_key = mk_wpds_key(crt_iter, bb, f);
  bool add_delta2_rule = false;
  bool initialize_state = true;
  llvm::CallInst* ci;
  wali::IMergeFn* cf;
  wali::Key callee_entry_key;
  while(true) {
    // Only abstract execute till the call instruction
    end_iter = findNonModelCall(bb, crt_iter, ci);
    Vocabulary pre_voc = insLiveBeforeMap[crt_iter];
    Vocabulary post_voc;
    if(end_iter == bb_end)
      post_voc = liveAfterMap[bb];
    else
      post_voc = insLiveAfterMap[end_iter];
    post_voc = abstract_domain::replaceVersion(post_voc, 0, 1);

    DEBUG_PRINTING(DBG_PRINT_DETAILS, abstract_domain::print(std::cout << "\npre_voc:", pre_voc););
    DEBUG_PRINTING(DBG_PRINT_DETAILS, abstract_domain::print(std::cout << "\npost_voc:", post_voc););
    Vocabulary pre_post_voc;
    UnionVocabularies(pre_voc, post_voc, pre_post_voc);

    bb_abs_trans_cr_.abstractExecuteBasicBlock(bb, crt_iter, end_iter, insLiveBeforeMap, insLiveAfterMap, pre_voc, post_voc, allocaMap, valueToDimMap, initialize_state);
    add_delta2_rule = false;

    // Handle the case when the basic block b contains a call instruction
    if(ci != NULL) {
      llvm::CallSite CS(ci);
      if(CS.getCalledFunction()->getName() != "__VERIFIER_assert") {
        // Use merge function to add delta 2 transitions
        BasicBlock *callee_entry_bb = &(CS.getCalledFunction()->getEntryBlock());
        callee_entry_key = mk_wpds_key(callee_entry_bb->begin(), callee_entry_bb, CS.getCalledFunction());

        cf = bb_abs_trans_cr_.obtainMergeFunc(CS);
        add_delta2_rule = true;
      } else {
        // Specially handle assert call nodes
        // Calls to assert are replaced by a delta-1 rule that goes from this bb to a 
        // unreachable node that says that the arg_val is zero, meaning
        // that unreachable node is reached only is this particular assert is false
        // Finally, the state is modified to assert that the arg_val is zero before handling rest of the instructions 
        CallSite::arg_iterator argit = CS.arg_begin();

        ref_ptr<AbstractValue> state_cp = bb_abs_trans_cr_.GetState()->Copy();
        WrappedDomain_Int arg_val = bb_abs_trans_cr_.getOperandValue(*argit);
        WrappedDomain_Int zero = arg_val.of_const(0);
        ref_ptr<AbstractValue> assume_cond = arg_val.abs_equal(zero);
        state_cp->Meet(assume_cond);

        wali::Key unreachable_key = mk_wpds_unreachable_key(CS);
        Vocabulary state_cp_voc = state_cp->GetVocabulary();
        AddRuleToPds(state_cp, from_key, unreachable_key, REGULAR_WEIGHT, pre_post_voc/*state_cp_voc*/);

        // Add this key to the list of unreachable keys
        unreachable_keys_.insert(unreachable_key);
      }

      if(add_delta2_rule && (end_iter != bb->end() && (Instruction*)end_iter != t_inst)) {
        // Add delta2 rule and set initialize_state to true
        wali::Key to_key = mk_wpds_key(end_iter, bb, f); 
        ref_ptr<AbstractValue> state_cp = bb_abs_trans_cr_.GetState();
        Vocabulary state_cp_voc = state_cp->GetVocabulary();
        AddDelta2RuleToPds(state_cp, from_key, callee_entry_key, to_key, REGULAR_WEIGHT, state_cp_voc, cf);

        from_key = to_key;
        initialize_state = true;
      } else {
        initialize_state = false;
      }
    } else {
      assert((Instruction*)end_iter == bb->end());
    }

    if((Instruction*)end_iter == bb->end() || (Instruction*)end_iter == t_inst)
      break;
    crt_iter = end_iter;
  }

  // Get new value for pre_voc
  assert(crt_iter != bb->end());
  Vocabulary pre_voc = insLiveBeforeMap[crt_iter];
  DEBUG_PRINTING(DBG_PRINT_DETAILS, abstract_domain::print(std::cout << "\npre_voc:", pre_voc););

  llvm::DataLayout TD = bb_abs_trans_cr_.getDataLayout();

  // Special case when there are no successors, ie., terminator instruction is ret
  if(t_inst->getNumSuccessors() == 0) {
    bb_abs_trans_cr_.SetSucc(nullptr);

    // Calculate vocabulary information
    Vocabulary post_voc = liveAfterMap[bb];
    post_voc = abstract_domain::replaceVersion(post_voc, 0, 1);

    Vocabulary return_voc = CreateReturnVocabulary(bb->getParent(), Version(1));
    post_voc.insert(return_voc.begin(), return_voc.end());
    DEBUG_PRINTING(DBG_PRINT_DETAILS, abstract_domain::print(std::cout << "\npost_voc:", post_voc););

    Vocabulary pre_post_voc;
    UnionVocabularies(pre_voc, post_voc, pre_post_voc);

    bb_abs_trans_cr_.GetState()->AddVocabulary(return_voc);

    // TODO: Do something special for unreachable inst. For now we just ignore creating 
    // anything for the BB containing Unreachable Instruction
    if(t_inst->getOpcode() == Instruction::Unreachable) {
      return;
    }

    assert(t_inst->getOpcode() == Instruction::Ret);
    bb_abs_trans_cr_.abstractExecuteInst(*t_inst, insLiveBeforeMap, insLiveAfterMap, pre_post_voc);

    ref_ptr<AbstractValue> state;
    if(add_delta2_rule) {
      // This is a bit awkward as the call is followed by a ret instruction.
      // So to deal with it we create a dummy return node that acts as a mid between the call and the ret instr
      wali::Key dummy_exit_key = mk_exit_wpds_dummy_key(bb->getParent(), ci);
      ref_ptr<AbstractValue> state_cp = bb_abs_trans_cr_.GetState()->Copy();
      AddDelta2RuleToPds(state_cp, from_key, callee_entry_key, dummy_exit_key, REGULAR_WEIGHT, pre_post_voc, cf);

      Vocabulary state_voc = bb_abs_trans_cr_.GetState()->GetVocabulary();
      Vocabulary state_post_voc = abstract_domain::getVocabularySubset(state_voc, Version(1));
      Vocabulary state_post_voc_as_pre = abstract_domain::replaceVersion(state_post_voc, Version(1), Version(0));
      Vocabulary state_post_voc_as_pre_plus_post_voc;
      UnionVocabularies(state_post_voc, state_post_voc_as_pre, state_post_voc_as_pre_plus_post_voc);

      ref_ptr<AbstractValue> state_tp = bb_abs_trans_cr_.GetState()->Top();
      state_tp->AddVocabulary(state_post_voc_as_pre);
      state_tp->Project(state_post_voc_as_pre_plus_post_voc);
      ref_ptr<AvSemiring> state_tp_av_sem = new AvSemiring(state_tp);
      ref_ptr<AvSemiring> state_one_av_sem = dynamic_cast<AvSemiring*>(state_tp_av_sem->one().get_ptr());
      state = state_one_av_sem->GetAbstractValue();
      from_key = dummy_exit_key;
    } else {
      state = bb_abs_trans_cr_.GetState()->Copy();
    }
 
    // Add delta 0 rule for this function
    AddDelta0RuleToPds(state, from_key, REGULAR_WEIGHT, pre_post_voc);
    return;
  } else {
    // Add rules for each of the successor
    ref_ptr<AbstractValue> state = bb_abs_trans_cr_.GetState();
    for (unsigned i = 0, n_succ = t_inst->getNumSuccessors(); i < n_succ; ++i) {
      llvm::BasicBlock* succ = t_inst->getSuccessor(i);
      DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nAnalyzing successor: " << getName(succ););

      bb_abs_trans_cr_.SetState(state->Copy());
      bb_abs_trans_cr_.SetSucc(succ);

      // Calculate post_voc as the live variable informaton just before the first non-phi instruction in succ
      llvm::BasicBlock::iterator start_succ = succ->begin(), end_succ = succ->end(), succ_last_phi_iter = succ->end();
      while(start_succ != end_succ) {
        Instruction* I = start_succ;
        bool non_phi_encountered = false;

        switch(I->getOpcode()) {
        case Instruction::PHI:
          succ_last_phi_iter = start_succ;
          break;
        default: non_phi_encountered = true; break;
        }

        if(non_phi_encountered)
          break;

        start_succ++;
      }

      Vocabulary post_voc;
      if(start_succ == end_succ)
        post_voc = liveAfterMap[succ];
      else
        post_voc = insLiveBeforeMap[start_succ];
      post_voc = abstract_domain::replaceVersion(post_voc, 0, 1);
      Vocabulary pre_post_voc;
      UnionVocabularies(pre_voc, post_voc, pre_post_voc);

      // Find out the extra vocabulary needed to handle terminator precisely
      Vocabulary term_voc = GetInstructionVocabulary(t_inst, *(bb->getParent()), TD, valueToDimMap, allocaMap);
      bb_abs_trans_cr_.GetState()->AddVocabulary(term_voc);

      switch(t_inst->getOpcode()) {
        // Visit branch and switch functions
      case Instruction::Br: 
      case Instruction::IndirectBr: 
      case Instruction::Switch: 
        bb_abs_trans_cr_.abstractExecuteInst(*t_inst, insLiveBeforeMap, insLiveAfterMap, pre_post_voc);
        break;
      default:
        // Cannot handle other kind of terminator instructions
        assert(false);
        bb_abs_trans_cr_.abstractExecuteInst(*t_inst, insLiveBeforeMap, insLiveAfterMap, pre_post_voc);
      }

      // Process phi if there exist any
      BasicBlock::iterator succ_first_non_phi_iter = succ->begin();

      if(succ_last_phi_iter != succ->end()) {
        succ_first_non_phi_iter = succ_last_phi_iter;
        succ_first_non_phi_iter++;

        // Find out the extra vocabulary needed to handle succ phi instructions precisely
        Vocabulary succ_phi_voc = GetBasicBlockVocabulary
          (succ, succ->begin(), succ_first_non_phi_iter, TD, valueToDimMap, allocaMap);
        bb_abs_trans_cr_.GetState()->AddVocabulary(succ_phi_voc);

        start_succ = succ->begin();
        end_succ = succ->end();
      
        while(start_succ != succ_first_non_phi_iter) {
          Instruction* I = start_succ;
          std::stringstream iss;
          iss << "AbsExecInst_" << I->getName().str();
          utils::Timer inst_timer(iss.str(), std::cout, true);

          bool non_phi_encountered = false;
          switch(I->getOpcode()) 
            {
            case Instruction::PHI:
              bb_abs_trans_cr_.abstractExecuteInst(*I, insLiveBeforeMap, insLiveAfterMap, pre_post_voc);
              break;
            default: non_phi_encountered = true; assert(false); break;
            }

          if(non_phi_encountered)
            break;

          start_succ++;
        }
      }

      // Widening Type is WIDENING_RULE if (bb, succ) is a backedge
      WideningType wty = REGULAR_WEIGHT;
      if(is_llvm_backedge(bb, succ)) {
        wty = WIDENING_RULE;
        DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                       std::cout << "Making this rule WIDENING_RULE as succ is:" << getName(succ) << std::endl;);
      }

      ref_ptr<AbstractValue> state = bb_abs_trans_cr_.GetState();
      wali::Key succ_key = mk_wpds_key(succ_first_non_phi_iter, succ, f);

      // Handle special case where succ contains unreachable
      // If it does create a unique id associated with succ
      if(add_array_bounds_check_ && containsCallToTrap(succ)) {
        succ_key = mk_unique_wpds_key(succ);
        unreachable_array_bounds_check_keys_.insert(succ_key);
      }

      if(add_delta2_rule) {
        AddDelta2RuleToPds(state, from_key, callee_entry_key, succ_key, wty, pre_post_voc, cf);
      } else {
        AddRuleToPds(state, from_key, succ_key, wty, pre_post_voc);
      }
    }
  }
}

wali::wpds::WPDS* WrappedDomainWPDSCreator::createWPDS(bool is_fwpds) {
  is_fwpds_ = is_fwpds;
  if(is_fwpds) {
    pds_ = new wali::wpds::fwpds::FWPDS();
  }
  else {
    pds_ = new wali::wpds::ewpds::EWPDS();
  }

  if(cmdlineparam_allow_phis) {
    //performMem2Reg();
  } else {
    performReg2Mem();
    performMem2Reg();
    performReg2Mem();
  }

  // Perform printing
  {
    legacy::PassManager PM;
    std::unique_ptr<tool_output_file> bcprinting_out;
    std::error_code bc_ec;
    bcprinting_out.reset(new tool_output_file(filename_ + std::string("_human_readable.txt"), bc_ec, sys::fs::F_None));
    bcprinting_out->keep();

    bcprinting_out->os() << "\n\nPrinting module after reg2mem pass:";
    PM.add(createPrintModulePass(bcprinting_out->os(), "", true));
    PM.run(*getModule());
  }

  // Step 1: Find loops in the program so that widening weights can be added accordingly
  legacy::PassManager PM;
  LoopInfoWrapperPass* loopinfopass = new LoopInfoWrapperPass();
  PM.add(loopinfopass);
  CollectLoopInfo* collectloopinfo = new CollectLoopInfo();
  PM.add(collectloopinfo);
  PM.run(*getModule());

  func_loop_infos_ = collectloopinfo->getFunctionToLoopInfoMap();

  // Step 2: Perform live variable analysis
  llvm::DataLayout TD = bb_abs_trans_cr_.getDataLayout();
  legacy::PassManager PMLV;
  LiveVariableAnalysis* lva = new LiveVariableAnalysis();
  lva->setDataLayout(&TD);
  PMLV.add(lva);
  PMLV.run(*getModule());


  for(Module::FunctionListType::iterator fit = getModule()->getFunctionList().begin(); fit != getModule()->getFunctionList().end(); fit++) {
    Function* f = fit;
    if (!f || (f && f->isDeclaration()))
      continue;

    for(Function::BasicBlockListType::iterator bbit = fit->getBasicBlockList().begin(); bbit != fit->getBasicBlockList().end(); bbit++) {
      BasicBlock* bb = bbit;
      DEBUG_PRINTING(DBG_PRINT_OVERVIEW,
                     std::cout << "\nAnalyzing BB:" << getName(bb););
      abstractExecuteBasicBlock(bb, lva);
    }
  }
  return pds_;
}

// Return the query automaton (input to poststar)
wali::wfa::WFA* WrappedDomainWPDSCreator::BuildAutomaton(bool is_backward, std::ostream * os) {
  wali::wfa::WFA* fa_prog = new wali::wfa::WFA(wali::wfa::WFA::INORDER, NULL);

  // State keys
  wali::Key accepting_state = wali::getKey("accepting_state");

  // Find the root set of the call graph's SCC graph.
  // For each SCC, select a representative, and mark its entry (for forward) or exit (for backward) point
  // as an accepting state.

  // FIXME: We still get coverage holes if a root CFG is unconditionally
  //        recursive (see, 118.far and 132.recursiveMain - the former
  //        is due to bad IR, the latter is contrived to demonstrate this).
  // TODO: Solution (per DM/TR conversation) seems to be to make a
  //       stack-qualified query matching sigma* (maybe limiting sigma
  //       to the set of pushed call-site keys?).

  value_to_dim_t global_instr_value_to_dim;
  Vocabulary global_voc = CreateGlobalVocabulary(getModule(), Version(0), global_instr_value_to_dim);
  llvm::DataLayout TD = bb_abs_trans_cr_.getDataLayout();

  LazyCallGraph lcallgraph(*getModule());

  for(LazyCallGraph::iterator it = lcallgraph.begin(); it != lcallgraph.end(); it++) {
    LazyCallGraph::Node n = *it;
    Function& f = n.getFunction();

    if (os != NULL) {
      (*os) << "-root:" << getName(&f) << "\n";
    }

    // Find the exit basic block if there is a unique one else throw assertion
    BasicBlock* exit_bb = NULL;
    if(is_backward) {
      for (BasicBlock &bb : f) {
        if (isa<ReturnInst>(bb.getTerminator())) {
          if(exit_bb != NULL)
            assert(false);
          exit_bb = &bb;
        }
      }
    }

    value_to_dim_t temp_instr_value_to_dim;
    temp_instr_value_to_dim = global_instr_value_to_dim;
    Vocabulary arg_voc = CreateFunctionArgumentVocabulary(&f, Version(0), temp_instr_value_to_dim);
    Vocabulary voc0;
    UnionVocabularies(global_voc, arg_voc, voc0);

    ref_ptr<AvSemiring> start_state = GetStartingState(voc0, f);
    wali::Key startKey = mk_wpds_key(is_backward? exit_bb->begin() : f.getEntryBlock().begin(), &(f.getEntryBlock()), &f);
    fa_prog->addTrans(program_, startKey, accepting_state, start_state.get_ptr());
  }

  ref_ptr<AvSemiring> st = new AvSemiring(bb_abs_trans_cr_.GetState(), "", "", REGULAR_WEIGHT);
  fa_prog->addState(program_, st->zero());
  fa_prog->setInitialState(program_);
    
  fa_prog->addState(accepting_state, st->zero());
  fa_prog->addFinalState(accepting_state);
  
  return fa_prog;
}

ref_ptr<AvSemiring> WrappedDomainWPDSCreator::GetStartingState(const Vocabulary& voc, Function& f) {
  Vocabulary double_voc;
  Vocabulary post_voc = abstract_domain::replaceVersion(voc, 0, 1);
  UnionVocabularies(voc, post_voc, double_voc);

  // Use av_ to get a handle on the abstract value that BitpreciseWrappedAbstractValue needs
  ref_ptr<AbstractValue> av_dvoc_top = av_->Top(); 
  av_dvoc_top->AddVocabulary(double_voc);
  av_dvoc_top->Project(double_voc); // Ensure av_dvoc_top's vocabulary is exactly double_voc

  std::map<DimensionKey, bool> vocab_sign;
  ref_ptr<BitpreciseWrappedAbstractValue> top_wav = new BitpreciseWrappedAbstractValue(av_dvoc_top, vocab_sign);
  ref_ptr<AvSemiring> av = new AvSemiring(top_wav, "", "", REGULAR_WEIGHT);

  ref_ptr<BitpreciseWrappedAbstractValue> start_wav = dynamic_cast<BitpreciseWrappedAbstractValue*>(dynamic_cast<AvSemiring*>(av->one().get_ptr())->GetAbstractValue().get_ptr());
  
  // TODO: Get signedness information by type inference. For now, we assume everything is signed
  for(Vocabulary::const_iterator it = voc.begin(); it != voc.end(); it++) {
    vocab_sign.insert(std::pair<DimensionKey, bool>(*it, true/*is_signed*/));
  }

  start_wav->AddBoundingConstraints(vocab_sign);
  std::string start_from = "start_" + getName(&f);
  BasicBlock::iterator begin_it = f.getEntryBlock().begin();
  return new AvSemiring(start_wav, start_from, wali::key2str(mk_wpds_key(begin_it, &(f.getEntryBlock()), &f)), REGULAR_WEIGHT);
}

void WrappedDomainWPDSCreator::UpdateMinMaxVocabularySize(size_t voc_size) {
  if(voc_size < min_max_voc_size.first)
    min_max_voc_size.first = voc_size;
  if(voc_size > min_max_voc_size.second)
    min_max_voc_size.second = voc_size;
}
