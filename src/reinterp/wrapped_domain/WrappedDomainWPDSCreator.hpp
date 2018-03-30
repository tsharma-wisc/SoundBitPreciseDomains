#ifndef src_reinterp_wrapped_domain_WrappedDomainWPDSCreator_hpp
#define src_reinterp_wrapped_domain_WrappedDomainWPDSCreator_hpp

#include "src/reinterp/wrapped_domain/WrappedDomainBBAbsTransCreator.hpp"
#include "src/reinterp/wrapped_domain/LiveVariablePass.hpp"

#include "wali/wpds/WPDS.hpp"
#include "wali/wfa/WFA.hpp"

namespace llvm_abstract_transformer {

  // WrappedDomainWPDSCreator
  //
  // This class provides methods to create WPDS from a module <
  // 
  class WrappedDomainWPDSCreator {
  public:
    typedef std::map<llvm::Value*, DimensionKey> value_to_dim_t;

  private:
    WrappedDomainBBAbsTransCreator bb_abs_trans_cr_;

    // Used to initialize state_ and bool values. 
    // This value is mainly used to find the base class on which to calculated a correct bit-precise wrapped
    // representation of bool values and abstract transformers 
    // (for example, it can be pointset-powerset of polyhedra)
    // Its vocabulary is essentially ignored
    ref_ptr<abstract_value::AbstractValue> av_;

    std::string filename_;

    wali::Key program_;
    wali::wpds::WPDS* pds_;

    // This set is populated for the purpose of finding the number of asserts in the program
    std::set<wali::Key> unreachable_keys_;
    std::set<wali::Key> unreachable_array_bounds_check_keys_;
    bool add_array_bounds_check_;
    std::pair<size_t, size_t> min_max_voc_size;
    std::map<const llvm::Function*, std::shared_ptr<llvm::LoopInfo>> func_loop_infos_;

  public:
    // av is used to create initial state
    explicit WrappedDomainWPDSCreator(std::unique_ptr<llvm::Module> M, ref_ptr<abstract_value::AbstractValue> av, std::string& bcprinting_filename, bool add_array_bounds_check);
    ~WrappedDomainWPDSCreator();

    llvm::Module* getModule();
    std::set<wali::Key> GetUnreachableKeys() const;
    std::set<wali::Key> GetUnreachableArrayBoundsCheckKeys() const;
    wali::Key GetProgramKey() const;
    std::pair<size_t, size_t> GetMinMaxVocSize() const;

    bool isModel(const llvm::Function* f) const;

    wali::wpds::WPDS* createWPDS(bool is_fwpds);
    wali::wfa::WFA* BuildAutomaton(bool is_backward, std::ostream * os);
    void performReg2Mem();
    void performMem2Reg();

    // Abstract execution of BasicBlock and its helper functions
    // This is the code which does the main things
    void abstractExecuteBasicBlock
    (llvm::BasicBlock* bb,
     LiveVariableAnalysis* lva);

  private:  // Helper functions  
    // Helper functions for WPDS construction
    //wali::Key mk_wpds_key(llvm::BasicBlock* bb);
    wali::Key mk_wpds_key(llvm::BasicBlock::iterator start, llvm::BasicBlock* bb, llvm::Function* f);
    wali::Key mk_unique_wpds_key(llvm::BasicBlock* bb);
    wali::Key mk_wpds_unreachable_key(const llvm::CallSite& CS);
    wali::Key mk_exit_wpds_key(llvm::Function* bb);
    wali::Key mk_exit_wpds_dummy_key(llvm::Function* bb, llvm::CallInst* ci);
    bool is_llvm_backedge(const llvm::BasicBlock* bb, const llvm::BasicBlock* succ);

    // Helper functions needed to build the starting automaton for poststar
    ref_ptr<AvSemiring> GetStartingState(const Vocabulary& v, llvm::Function& F);
    llvm::BasicBlock::iterator findNonModelCall(llvm::BasicBlock* bb, 
                                                llvm::BasicBlock::iterator start, 
                                                llvm::CallInst*& ci) const;

    void UpdateMinMaxVocabularySize(size_t voc_size);
    void AddRuleToPds(ref_ptr<abstract_value::AbstractValue> state, wali::Key from_key, wali::Key to_key, WideningType wty, Vocabulary& voc);
    void AddDelta2RuleToPds(ref_ptr<abstract_value::AbstractValue> state, wali::Key from_key, wali::Key callee_entry_key, wali::Key to_key, WideningType wty, Vocabulary& voc, wali::IMergeFn* cf);
    void AddDelta0RuleToPds(ref_ptr<abstract_value::AbstractValue> state, wali::Key from_key, WideningType wty, Vocabulary& voc);
  };

} // End llvm_abstract_transformer namespace

#endif // src_reinterp_wrapped_domain_WrappedDomainWPDSCreator_hpp
