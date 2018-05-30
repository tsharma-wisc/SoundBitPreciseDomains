#include "src/reinterp/wrapped_domain/MergeFunction.hpp"

//------------------------------------------
/// The merge function for EWPDS
/// \param call The semiring value at call.
/// \param exit The semiring value at exit.
//------------------------------------------
/**
apply_f(call,exit) has access to the global variables, the return variable in callee and the dimension in caller
which receives the return value
*/
using namespace abstract_domain;
MergeFunction::MergeFunction(const Vocabulary& global_postvoc_callee,
                             const DimensionKey& ret_key_callee,
                             const DimensionKey& ret_key_caller,
                             sem_elem_t call_to_func_entry):
  global_postvoc_callee_(global_postvoc_callee),
  ret_key_callee_(ret_key_callee),
  ret_key_caller_(ret_key_caller),
  call_to_func_entry_(call_to_func_entry) {

  assert( (ret_key_callee == DUMMY_KEY) == (ret_key_caller == DUMMY_KEY) );
}

std::ostream & MergeFunction::print(std::ostream & out) const {
  out << "MergeFunction{\n";
  abstract_domain::print(out << "Global post-vocabulary callee:", global_postvoc_callee_);
  out << ", Return key callee:";
  abstract_domain::print(out, ret_key_callee_);
  out << ", Return key caller:";
  abstract_domain::print(out, ret_key_caller_);
  out <<"}";
  call_to_func_entry_->print(std::cout << "\ncall_to_func_entry:");
  return out;
}

/*
 */
sem_elem_t MergeFunction::apply_f(
  sem_elem_t call, sem_elem_t exit
) {
  if(call->equal(call->zero()) || exit->equal(exit->zero())) {
    return call->zero();
  }

  call->print(std::cout << "\ncall:");
  exit->print(std::cout << "\nexit:");
  call_to_func_entry_->print(std::cout << "\ncall_to_func_entry_:");

  wali::ref_ptr<AbstractValue> caller_sum =
    (static_cast<AvSemiring*>(call.get_ptr()))->GetAbstractValue()->Copy();
  sem_elem_t call_extend_call_to_func_entry_extend_exit =
    call->extend(call_to_func_entry_)->extend(exit);
  wali::ref_ptr<AbstractValue> callee_sum =
    (static_cast<AvSemiring*>(call_extend_call_to_func_entry_extend_exit.get_ptr()))->GetAbstractValue();

  caller_sum->print(std::cout << "\ncaller_sum:");
  callee_sum->print(std::cout << "\ncallee_sum:");

  // Calculate the vocabulary for which callee is incorrect
  Vocabulary callee_incorrect_voc;
  Vocabulary callee_correct_voc = global_postvoc_callee_;
  if(ret_key_callee_ != DUMMY_KEY)
    callee_correct_voc.insert(ret_key_callee_);
  Vocabulary callee_postvoc =
    abstract_domain::getVocabularySubset(callee_sum->GetVocabulary(), 1);
  SubtractVocabularies(callee_postvoc, callee_correct_voc, callee_incorrect_voc);

  // Calculate the vocabulary for which caller is incorrect
  Vocabulary caller_incorrect_voc = global_postvoc_callee_;
  if(ret_key_caller_ != DUMMY_KEY)
    caller_incorrect_voc.insert(ret_key_caller_);

  callee_sum->RemoveVocabulary(callee_incorrect_voc);
  caller_sum->RemoveVocabulary(caller_incorrect_voc);

  // Replace the ret_key_callee_ with ret_key_caller_ in callee_sum
  if(ret_key_callee_ != DUMMY_KEY) {
    callee_sum->AddDimension(ret_key_caller_);
    callee_sum->AddEquality(ret_key_caller_, ret_key_callee_);
    callee_sum->RemoveDimension(ret_key_callee_);
  }

  wali::ref_ptr<AbstractValue> ret_sum = callee_sum;
  ret_sum->Meet(caller_sum);

  ret_sum->print(std::cout << "\nret_sum:");

  return new AvSemiring(ret_sum);
}

bool MergeFunction::equal(wali::merge_fn_t _mf)
{
  const MergeFunction* mf = static_cast< const MergeFunction*>(_mf.get_ptr());
  return ((global_postvoc_callee_ == mf->global_postvoc_callee_)
          && (ret_key_callee_ == mf->ret_key_callee_)
          && (ret_key_caller_ == mf->ret_key_caller_));
}
