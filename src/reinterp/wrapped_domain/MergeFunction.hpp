#ifndef src_reinterp_wrapped_domain_mergefunction_hpp
#define src_reinterp_wrapped_domain_mergefunction_hpp

#include "wali/IMergeFn.hpp"
#include "src/AbstractDomain/common/AvSemiring.hpp"
#include "src/AbstractDomain/common/BitpreciseWrappedAbstractValue.hpp" 

using wali::SemElem;
using wali::sem_elem_t;

// Confluence Function.
class MergeFunction : public wali::IMergeFn {
public:  
  // Constructor: Expects DUMMY_KEY for ret_key_callee and ret_key_caller
  //              if the function call doesn't return any value.
  MergeFunction(const abstract_domain::Vocabulary& global_postvoc_callee, 
                const abstract_domain::DimensionKey& ret_key_callee, 
                const abstract_domain::DimensionKey& ret_key_caller, 
                sem_elem_t call_to_func_entry);

  ~MergeFunction() {}

  virtual std::ostream & print(std::ostream & out) const;

  virtual sem_elem_t apply_f(sem_elem_t w1, sem_elem_t w2);

  virtual bool equal(wali::merge_fn_t mf);

private:
  abstract_domain::Vocabulary global_postvoc_callee_;
  abstract_domain::DimensionKey ret_key_callee_, ret_key_caller_;
  sem_elem_t call_to_func_entry_;
};

#endif // src_reinterp_wrapped_domain_mergefunction_hpp


