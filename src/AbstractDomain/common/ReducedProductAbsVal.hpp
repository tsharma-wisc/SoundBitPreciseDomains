#ifndef src_AbstractDomain_common_ReducedProductAbstractValue_hpp
#define src_AbstractDomain_common_ReducedProductAbstractValue_hpp

#include "src/AbstractDomain/common/AbstractValue.hpp"

#include <cassert>
#include <map>
#include "src/AbstractDomain/common/dimension.hpp"

// TODO: Have Value as an Abstract class proving normal
// arithmetic operations as expected of Value.
namespace abstract_domain {
class ReducedProductAbsVal : public AbstractValue
{
  typedef AbstractValue BaseClass;
  typedef wali::ref_ptr< AbstractValue > AbsValRefPtr;

public:

  ReducedProductAbsVal (Vocabulary v) 
  : BaseClass(v)  {  }

  // Constructor
  ReducedProductAbsVal (const AbsValRefPtr &a1, const AbsValRefPtr &a2)
  {
    first_  = a1->Copy();
    second_ = a2->Copy();
    UnionVocabularies (a1->GetVocabulary(), a2->GetVocabulary(), 
		       BaseClass::voc_);
    //TODO The user of the abstract domain should call reduce explicitly.
    //     Such implicit calls to reduce in the constructor cause nested
    //     calls to alphaHat.
    //Reduce();
  }

  virtual AbsValRefPtr Copy() const 
  {
    return new ReducedProductAbsVal(first_->Copy(), second_->Copy());
  }

  AbsValRefPtr first() const
  {
    return first_;
  }

  AbsValRefPtr second() const
  {
    return second_;
  }

  bool IsDistributive() const 
  {
    return (first_->IsDistributive() && second_->IsDistributive());
  }

  // Other ways to create AbstractValue elements
  // ====================================
  virtual AbsValRefPtr Top() const 
  {
    return new ReducedProductAbsVal(first_->Top(), second_->Top());
  }

  virtual AbsValRefPtr Bottom () const
  {
    return new ReducedProductAbsVal(first_->Bottom(), second_->Bottom());
  }

  virtual bool operator== (const BaseClass& other) const
  {
    const ReducedProductAbsVal *red_prd = downcast(other);
    return first_->Equal(red_prd->first_) && second_->Equal(red_prd->second_);
  }

  virtual std::string ToString() const
  {
    std::ostringstream ss;
    ss << "\nReducedProductAbsVal:";
    ss << "\n\tFirst:\n";
    ss << first_->ToString();
    ss << "\n\tSecond:\n";
    ss << second_->ToString();
    return ss.str();
  }

  virtual wali::ref_ptr<AbstractValue > Parse(const std::string &) const
  {
    assert (false && "Parse not implemented");
    return NULL;
  }

  // Abstract Domain operations
  virtual bool IsBottom() const
  {
    // TODO: Reduce here?
    return first_->IsBottom() || second_->IsBottom();
  }

  virtual bool IsTop () const
  {
    return first_->IsTop() && second_->IsTop();
  }

  virtual void Join(const AbsValRefPtr & other)
  {
    const ReducedProductAbsVal *red_prd = downcast(other);
    first_->Join(red_prd->first_);
    second_->Join(red_prd->second_);
  }

  virtual void JoinSingleton(const AbsValRefPtr & other)
  {
    const ReducedProductAbsVal *red_prd = downcast(other);
    first_->JoinSingleton(red_prd->first_);
    second_->JoinSingleton(red_prd->second_);
  }

  virtual void Meet(const AbsValRefPtr &other)
  {
    const ReducedProductAbsVal *red_prd = downcast(other);
    first_->Meet(red_prd->first_);
    second_->Meet(red_prd->second_);
    //Reduce();
  }

  
  // Does this overapproximates other?
  virtual bool Overapproximates(const AbsValRefPtr& other) const
  {
    const ReducedProductAbsVal *red_prd = downcast(other);
    return first_->Overapproximates(red_prd->first_) &&  second_->Overapproximates(red_prd->second_);

  }
  // Add equality constraint
  virtual void AddEquality(const DimensionKey& v1, const DimensionKey& v2)
  {
    first_->AddEquality(v1, v2);
    second_->AddEquality(v1, v2);
    //Reduce();
  }

  // project: project onto Vocabulary v.
  virtual void Project(const Vocabulary &v) 
  {
    first_ ->Project(v);
    second_->Project(v);
    UnionVocabularies (first_->GetVocabulary(), second_->GetVocabulary(), 
	                    BaseClass::voc_);
  }

  //Havoc: Havoc out the vocabulary v, i.e. remove any constraints on them 
  virtual wali::ref_ptr<AbstractValue> Havoc(const Vocabulary & v) const
  {
    AbsValRefPtr ret_first = first_->Havoc(v);
    AbsValRefPtr ret_second = second_->Havoc(v);
    AbsValRefPtr ret = new ReducedProductAbsVal(ret_first, ret_second);
    return ret;
  }

  //In - place reduction
  virtual void Reduce()
  {
    first_  ->Reduce(second_);
    second_ ->Reduce(first_ );
  }

  // reduce this with constraints from that
  void Reduce(const AbsValRefPtr& that)
  {
    // Is that a ReducedProduct abstract value?
    const ReducedProductAbsVal * that_red_prd = static_cast<const ReducedProductAbsVal *>(that.get_ptr());
    if (that_red_prd != NULL) {
      // Call Reduce on each component of "that"
      this->Reduce (that_red_prd->first_);
      this->Reduce (that_red_prd->second_);
    }  else {
      // That is a base domain
      first_->Reduce (that);
      second_->Reduce(that);
    }

  }

  //In - place Vocabulary manipulation operations
  // on AbstractValue
  virtual void AddVocabulary(const Vocabulary& v)
  {
    // Ensures that the two abstract value don't mix the vocabularies
    Vocabulary voc_to_add;
    SubtractVocabularies(v, this->voc_, voc_to_add);

    first_ ->AddVocabulary(voc_to_add);
    second_->AddVocabulary(voc_to_add);
    UnionVocabularies (first_->GetVocabulary(), second_->GetVocabulary(), 
		       BaseClass::voc_);
  }

  virtual void ReplaceVersions(const std::map<Version, Version>& vm)
  {
    first_ ->ReplaceVersions(vm);
    second_->ReplaceVersions(vm);
    UnionVocabularies (first_->GetVocabulary(), second_->GetVocabulary(), 
		                 BaseClass::voc_);
  }

  Vocabulary GetVocabulary() const 
  {
    return BaseClass::voc_;
  }


  /************** Interface used by Wrapped Domain LLVM reinterpretation layer ****************************/
  // Default implementation doesn't do anything
  // If your class intends to use BitpreciseWrappedAbstractValue to get correct
  // abstract domains, then this Wrap must be overloaded
  // Bool determines if the the key needs to be wrapped as signed or unsigned
  virtual void Wrap(const std::map<DimensionKey, bool>& vs) {
    first_->Wrap(vs);
    second_->Wrap(vs);
    // Reduce(); ??
  }

  // Add constraint lhs op rhs
  // This function AddConstraint is used also by llvm_reinterpretation layer
  virtual void AddConstraint(affexp_type lhs, affexp_type rhs, OpType op) {
    first_->AddConstraint(lhs, rhs, op);
    second_->AddConstraint(lhs, rhs, op);
    Reduce();
  }

  // Is this an abstraction of a singleton value?
  // Default implementation raises assertion
  virtual bool IsConstant(mpz_class& val) const {
    if(first_->IsConstant(val))
      return true;
    return second_->IsConstant(val);
  }

  virtual Vocabulary GetDependentVocabulary(DimensionKey& k) const {
    Vocabulary first_voc = first_->GetDependentVocabulary(k);
    Vocabulary second_voc = second_->GetDependentVocabulary(k);
    Vocabulary ret_voc;
    UnionVocabularies(first_voc, second_voc, ret_voc);
    return ret_voc;
  }


private:
  // members
  AbsValRefPtr first_, second_;

  const ReducedProductAbsVal *downcast( const AbsValRefPtr &val) const
  {
    return downcast(*val);
  }

  const ReducedProductAbsVal *downcast( const BaseClass &val) const
  {
    return static_cast<const ReducedProductAbsVal *>(&val);
  }

};

}
#endif // src_AbstractDomain_common_ReducedProductAbstractValue_hpp
