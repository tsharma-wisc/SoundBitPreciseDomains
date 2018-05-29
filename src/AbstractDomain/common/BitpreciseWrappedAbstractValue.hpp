#ifndef src_AbstractDomain_common_BitpreciseWrappedAbstractValue_hpp
#define src_AbstractDomain_common_BitpreciseWrappedAbstractValue_hpp

#include "AbstractValue.hpp"
#include "utils/timer/timer.hpp"
#include "utils/debug/DebugOptions.hpp"

// Forward declaration of friend class
class WrappedDomain_Int;
namespace llvm_abstract_transformer { 
  class WrappedDomainBBAbsTransCreator;
  class WrappedDomainWPDSCreator;
}

namespace abstract_domain {

// An abstract value wrapper used to provide eager and lazy wrapping option
// It assumes usual interface of AbstractValue, and additionally it expects an
// "sound" implementation of (bitprecise) wrap operation.

// TODO: Add signed or unsigned information. By default it interprets wrapping as
// signed. Wrapping a 32-bit variable as signed or unsigned is not unsound, but
// it can be imprecise. For example, if a 32-bit variable is wrapped as a signed
// value but the concrete operations on it treat it as unsigned, it will lead to
// unsigned values.
class BitpreciseWrappedAbstractValue : public AbstractValue
{
  typedef AbstractValue BaseClass;
  typedef ref_ptr< AbstractValue > AbsValRefPtr;
public:
  // A map from Dimension key to the signedness of the vocabulary
  typedef std::map<DimensionKey, bool> VocabularySignedness;
  friend class ::WrappedDomain_Int;
  friend class ::llvm_abstract_transformer::WrappedDomainBBAbsTransCreator;
  friend class ::llvm_abstract_transformer::WrappedDomainWPDSCreator;

  // If lazy_wrapping is set to false, all of the vocabulary is wrapped by default
  static bool lazy_wrapping;

  // If disable_wrapping is set to true, none of the vocabulary gets wrapped and wrap operation
  // becomes nop
  static bool disable_wrapping;

  // Constructor
  BitpreciseWrappedAbstractValue (const AbsValRefPtr &a, const VocabularySignedness &voc_to_wrap) 
    : BaseClass(a->GetVocabulary()) {
    av_ = a->Copy();
    VocabularySignedness correct_voc_to_wrap = voc_to_wrap;
    // TODO: Enable choice to perform eager wrapping
    AddBoundingConstraints(correct_voc_to_wrap);
  }

  BitpreciseWrappedAbstractValue (const BitpreciseWrappedAbstractValue &wav) 
    : BaseClass(wav.GetVocabulary()), av_(wav.av_->Copy()), wrapped_voc_(wav.wrapped_voc_) {
  }

  virtual AbsValRefPtr Copy() const {
    ref_ptr<BitpreciseWrappedAbstractValue> cp = new BitpreciseWrappedAbstractValue(*this);
    return cp.get_ptr();
  }

  AbsValRefPtr av() const {
    return av_;
  }

  VocabularySignedness wrapped_voc() const {
    return wrapped_voc_;
  }

  // Find if the dimension key k is wrapped, and if yes get the signedness of wrap in is_signed
  bool IsWrapped(DimensionKey k, bool& is_signed) const {
    VocabularySignedness::const_iterator it = wrapped_voc_.find(k);
    if(it == wrapped_voc_.end())
      return false;

    is_signed = it->second;
    return true;
  }

private:
  void MarkWrapped(DimensionKey k, bool signedness) {
    wrapped_voc_.insert(VocabularySignedness::value_type(k, signedness));
  }
public:

  bool IsDistributive() const {
    return av_->IsDistributive();
  }

  // Other ways to create AbstractValue elements
  // ====================================
  virtual AbsValRefPtr Top() const {
    // Do not use wrapped_voc_ as second parameter as this a clean top used to get top
    return new BitpreciseWrappedAbstractValue(av_->Top(), VocabularySignedness());
  }

  virtual AbsValRefPtr Bottom () const {
    return new BitpreciseWrappedAbstractValue(av_->Bottom(), VocabularySignedness());
  }

private:
  std::ostream& printVocabularySignedness(std::ostream& o, const VocabularySignedness& vs) const {
    for(VocabularySignedness::const_iterator it = vs.begin(); it != vs.end(); it++) {
      o << "(" << it->first;
      o << ",";
      if(it->second) o << "s"; else o << "u";
      o << ")| ";
    }
    return o;
  }

public:
  virtual std::string ToString() const {
    std::ostringstream ss;
    ss << "(BitpreciseWrappedAbstractValue:\n";
    ss << "wrapped_voc_:";
    printVocabularySignedness(ss, wrapped_voc_);
    /*for(VocabularySignedness::const_iterator it = wrapped_voc_.begin(); it != wrapped_voc_.end(); it++) {
      ss << ",";
      if(it->second) ss << "s"; else ss << "u";
      ss << ")| ";
      }*/
    ss << "\nav_:\n";
    ss << av_->ToString();
    ss << ")";
    return ss.str();
  }

  virtual bool operator== (const BaseClass& that) const {
    const BitpreciseWrappedAbstractValue *that_wav = downcast(that);
    if(wrapped_voc() == that_wav->wrapped_voc()) {
      return *av_ == *that_wav->av_;
    }

    ref_ptr<BaseClass> this_copy = Copy();
    ref_ptr<BitpreciseWrappedAbstractValue> this_wav_copy = downcast(this_copy);
    ref_ptr<BaseClass> that_copy = that_wav->Copy();
    ref_ptr<BitpreciseWrappedAbstractValue> that_wav_copy = downcast(that_copy);
    this_wav_copy->Wrap(that_wav->wrapped_voc_);
    that_wav_copy->Wrap(wrapped_voc_);

    return *(this_wav_copy->av_) == *(that_wav_copy->av_);
  }


  // Abstract Domain operations
  virtual bool IsBottom() const {
    return av_->IsBottom();
  }

  virtual bool IsTop () const {
    ref_ptr<BitpreciseWrappedAbstractValue> tp = new BitpreciseWrappedAbstractValue(av_->Top(), wrapped_voc_);
    return Equal(tp.get_ptr());
  }
  
  // Does this overapproximates that?
  virtual bool Overapproximates(const AbsValRefPtr& that) const {
    const BitpreciseWrappedAbstractValue *that_wav = downcast(that);
    if(wrapped_voc() == that_wav->wrapped_voc()) {
      return av_->Overapproximates(that_wav->av());
    }

    ref_ptr<BaseClass> this_copy = Copy();
    ref_ptr<BitpreciseWrappedAbstractValue> this_wav_copy = downcast(this_copy);
    ref_ptr<BaseClass> that_copy = that_wav->Copy();
    ref_ptr<BitpreciseWrappedAbstractValue> that_wav_copy = downcast(that_copy);
    this_wav_copy->Wrap(that_wav->wrapped_voc_);
    that_wav_copy->Wrap(wrapped_voc_);

    return this_wav_copy->Overapproximates(that_wav_copy);
  }

  virtual void Join(const AbsValRefPtr & other) {
    const BitpreciseWrappedAbstractValue *wav = downcast(other);

    av_->Join(wav->av());
    wrapped_voc_ = IntersectVocabularySignedness(wrapped_voc_, wav->wrapped_voc_);
    this->voc_ = av_->GetVocabulary();
  }

  virtual void JoinSingleton(const AbsValRefPtr & other) {
    const BitpreciseWrappedAbstractValue *wav = downcast(other);
    av_->JoinSingleton(wav->av());
    wrapped_voc_ = IntersectVocabularySignedness(wrapped_voc_, wav->wrapped_voc_);
    this->voc_ = av_->GetVocabulary();
  }

  virtual void Widen(const AbsValRefPtr & other) {
    const BitpreciseWrappedAbstractValue *wav = downcast(other);
    av_->Widen(wav->av());
    wrapped_voc_ = IntersectVocabularySignedness(wrapped_voc_, wav->wrapped_voc_);

    // For each vocabulary in wrapped_voc_, add bounding constraints again as they might be lost by widen
    // This ensures a better widening operator, and it ensures termination as bounding constraints is the
    // entire range of allowed values for a given dimension
    AddBoundingConstraints(wrapped_voc_);
    this->voc_ = av_->GetVocabulary();
  }

  virtual void Meet(const AbsValRefPtr &that) {
    const BitpreciseWrappedAbstractValue *that_wav = downcast(that);

    // If the wrapped vocabulary are equal, then no need to make a copies
    if(this->wrapped_voc() == that_wav->wrapped_voc()) {
      av_->Meet(that_wav->av_);
    }
    else {
      AbsValRefPtr that_wav_cp_tmp = that_wav->Copy();
      ref_ptr<BitpreciseWrappedAbstractValue> that_wav_cp = downcast(that_wav_cp_tmp);

      Wrap(that_wav_cp->wrapped_voc());
      that_wav_cp->Wrap(wrapped_voc());
      assert(this->wrapped_voc() == that_wav_cp->wrapped_voc());

      av_->Meet(that_wav_cp->av_);
    }
    this->voc_ = av_->GetVocabulary();
    //UpdateVocabularySignedness(av_->GetVocabulary());
  }

  virtual bool ImplementsExtend() { return true;}

  ref_ptr<AbstractValue> Extend(const ref_ptr<AbstractValue> &op2) { 
    utils::Timer timer("\nBitpreciseWrappedAbstractValue Extend timer:", std::cout, true);
    DEBUG_PRINTING(DBG_PRINT_DETAILS, std::cout << "\nIn BitpreciseWrappedAbstractValue extend:";);

    std::map<Version, Version> map_0_to_1__1_to_2;
    map_0_to_1__1_to_2.insert(std::pair<Version, Version>(0, 1));
    map_0_to_1__1_to_2.insert(std::pair<Version, Version>(1, 2));

    ref_ptr<BitpreciseWrappedAbstractValue> _1_2_op2_av = 
      static_cast<BitpreciseWrappedAbstractValue*>(op2->Copy().get_ptr());
    _1_2_op2_av->ReplaceVersions(map_0_to_1__1_to_2);

    DEBUG_PRINTING(DBG_PRINT_DETAILS, std::cout << "\n_1_2_op2_av:";  _1_2_op2_av->print(std::cout););

    ref_ptr<BitpreciseWrappedAbstractValue> _3_voc_this_av = 
      static_cast<BitpreciseWrappedAbstractValue*>(Copy().get_ptr());

    // Calculate the pre-vocabulary in _1_2_op2_av for which wrapping constraints
    // need to be added to get precision while not sacrificing soundness
    VocabularySignedness _3_voc_this_wav_minus_1_2_op2_wav = 
      SubtractVocabularySignednessFromVocabularySignedness
      (_3_voc_this_av->wrapped_voc_, _1_2_op2_av->wrapped_voc_);

    VocabularySignedness voc_to_bound = IntersectVocabularySignednessWithVocabulary(_3_voc_this_wav_minus_1_2_op2_wav, _1_2_op2_av->GetVocabulary());

    _1_2_op2_av->AddBoundingConstraints(voc_to_bound);

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nTime elapsed after add bounding constraint:" << timer.elapsed();
                   _1_2_op2_av->print(std::cout << "\n_1_2_op2_av after adding Bounding Constraints:"););

    // Perform meet of 3-voc values
    _3_voc_this_av->Meet(_1_2_op2_av);
    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, std::cout << "\nTime elapsed after meet:" <<timer.elapsed(););
    std::cout << std::flush;
    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, _3_voc_this_av->print(std::cout << "\n_3_voc_this_av after meet:"););
    std::cout << std::flush;

    // Explicit call to Reduce.
    // Note Meet does not call Reduce.
    _3_voc_this_av->Reduce();

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, _3_voc_this_av->print(std::cout << "\n_3_voc_this_av after reduce:"););
    std::cout << std::flush;

    // Move back to 2-vocabulary value
    Vocabulary _3_voc = _3_voc_this_av->GetVocabulary();
    Vocabulary voc_to_prj;
    {
      Vocabulary this_pre_voc = getVocabularySubset(_3_voc, 0);
      Vocabulary that_post_voc = getVocabularySubset(_3_voc, 2);
      Vocabulary that_nonversion_voc = 
        getUnversionedVocabularySubset(_1_2_op2_av->GetVocabulary());
      Vocabulary temp_voc;
      UnionVocabularies(this_pre_voc, that_post_voc, temp_voc);
      UnionVocabularies(temp_voc, that_nonversion_voc, voc_to_prj);
    }

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\n_3_voc:" << _3_voc << "\nvoc_to_prj:" << voc_to_prj;);

    _3_voc_this_av->Project(voc_to_prj);

    // Rename version 2 to 1
    _3_voc_this_av->ReplaceVersion(Version(2), Version(1));

    DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS, 
                   _3_voc_this_av->print(std::cout << "\n_3_voc_this_av after replace version:"););

    assert(_3_voc_this_av->GetVocabulary() == _3_voc_this_av->av_->GetVocabulary());
    return _3_voc_this_av.get_ptr();
  }

  // Add equality constraint
  virtual void AddEquality(const DimensionKey& v1, const DimensionKey& v2) {
    av_->AddEquality(v1, v2);
    VocabularySignedness::const_iterator it = wrapped_voc_.find(v1);
    if(it != wrapped_voc_.end()) {
      wrapped_voc_.insert(std::make_pair(v2, it->second));
    } else {
      it = wrapped_voc_.find(v2);
      if(it != wrapped_voc_.end()) {
        wrapped_voc_.insert(std::make_pair(v1, it->second));
      }
    }  
  }

  //In - place reduction
  virtual void Reduce() {
    av_->Reduce();
  }

  // reduce this with constraints from that
  virtual void Reduce(const AbsValRefPtr& that) {
    const BitpreciseWrappedAbstractValue * that_wav = static_cast<const BitpreciseWrappedAbstractValue *>(that.get_ptr());
    this->Reduce (that_wav->av());
  }

  // project: project onto Vocabulary v.
  virtual void Project(const Vocabulary &v) {
    av_->Project(v);
    Vocabulary rem_v;
    SubtractVocabularies(this->voc_, v, rem_v);
    wrapped_voc_ = SubtractVocabularyFromVocabularySignedness(wrapped_voc_, rem_v);
    this->voc_ = av_->GetVocabulary();
  }

  // Havoc: Havoc out the vocabulary v, i.e. remove any constraints on them 
  virtual ref_ptr<AbstractValue> Havoc(const Vocabulary & v) const {
    AbsValRefPtr h_av = av_->Havoc(v);
    VocabularySignedness havoced_wrapped_voc = SubtractVocabularyFromVocabularySignedness(wrapped_voc_, v);
    ref_ptr<BitpreciseWrappedAbstractValue> ret = new BitpreciseWrappedAbstractValue(h_av, havoced_wrapped_voc, false/*dummy param to avoid copy*/);
    return ret.get_ptr();
  }

  // In - place Vocabulary manipulation operations
  // on AbstractValue
  virtual void AddVocabulary(const Vocabulary& v) {
    av_->AddVocabulary(v);
    this->voc_ = av_->GetVocabulary();
  }

  virtual void ReplaceVersions(const std::map<Version, Version>& vm) {
    VocabularySignedness new_wrapped_voc;
    for(VocabularySignedness::const_iterator voc_it = wrapped_voc_.begin(); voc_it != wrapped_voc_.end(); voc_it++) {
      DimensionKey new_k = replaceVersions(voc_it->first, vm);
      new_wrapped_voc.insert(VocabularySignedness::value_type(new_k, voc_it->second));
    }

    wrapped_voc_ = new_wrapped_voc;
    av_->ReplaceVersions(vm);
    this->voc_ = av_->GetVocabulary();
  }

  virtual unsigned GetHeight () const {
    return av_->GetHeight();
  }

  void Wrap(const DimensionKey& k,  bool is_signed) {
    if(!disable_wrapping) {
      VocabularySignedness vs; vs.insert(std::make_pair(k, is_signed));
      Wrap(vs);
    }
  }

  virtual void Wrap(const VocabularySignedness& v_cons) {
    DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS,
                   print(std::cout << "\nIn wrap:\nthis:");
                   printVocabularySignedness(std::cout << "\nv_cons:", v_cons););
   
    VocabularySignedness v = v_cons;
    for(VocabularySignedness::iterator vit = v.begin(); vit != v.end(); ) {
      VocabularySignedness::iterator wv_it = wrapped_voc_.find(vit->first);
      if(wv_it == wrapped_voc_.end()) {
        wrapped_voc_.insert(*vit);
        vit++;
      } else {
        if(wv_it->second == vit->second) {
          v.erase(vit++); // Remove vit because it is already wrapped as a proper sign
        } else {
          // Sign is different so adjust the sign for wv_it accordingly
          wv_it->second = vit->second;
          vit++;
        }
      }
    }
    if(!disable_wrapping)
      av_->Wrap(v);

    DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS, print(std::cout << "\nReturning from wrap:\nthis:"););
  }

  virtual bool IsConstant(mpz_class& val) const {
    return av_->IsConstant(val);
  }

  virtual void AddConstraint(affexp_type lhs, affexp_type rhs, OpType op) {
    av_->AddConstraint(lhs, rhs, op);

    // Vocabulary cnstr_voc;
    // for(linexp_type::const_iterator it = lhs.first.begin(), end = lhs.first.end(); it != end; it++) {
    //   cnstr_voc.insert(it->first);
    // }
    // for(linexp_type::const_iterator it = rhs.first.begin(), end = rhs.first.end(); it != end; it++) {
    //   cnstr_voc.insert(it->first);
    // }
    // UpdateVocabularySignedness(cnstr_voc);
  }

  virtual Vocabulary GetDependentVocabulary(DimensionKey& k) const {
    return av_->GetDependentVocabulary(k);
  }

  // UpdateVocabularySignedness: Update vocabulary signedness information for vocabulary voc
  //
  // If any dimension d in voc, that is not in wrapped_voc_, is bounded by bounding constraints,, then add it to
  // wrapped_voc_. Usually, this is performed after abstract domain operation such as meet or join.
  // The motivation behind this operation is to ensure that wrapped_voc_ is as true to the abstract
  // value as possible. The wrapped_voc_ is useful to get precise values after widening operation
  virtual void UpdateVocabularySignedness(Vocabulary voc);

private:
  // members
  AbsValRefPtr av_;
  VocabularySignedness wrapped_voc_; // Keeps track of the vocabulary that are wrapped

  // Private constructor that avoids copy and wrap operation on as. 
  // It should be used carefully, so that voc_to_wrap and a match in description
  BitpreciseWrappedAbstractValue (AbsValRefPtr &a, const VocabularySignedness& voc_to_wrap, bool dummy) 
    : BaseClass(a->GetVocabulary()), av_(a), wrapped_voc_(voc_to_wrap) {
  }

  BitpreciseWrappedAbstractValue *downcast( AbsValRefPtr &val) const {
    return downcast(*val);
  }

  BitpreciseWrappedAbstractValue *downcast( BaseClass &val) const {
    return static_cast<BitpreciseWrappedAbstractValue *>(&val);
  }

  const BitpreciseWrappedAbstractValue *downcast( const AbsValRefPtr &val) const {
    return downcast(*val);
  }

  const BitpreciseWrappedAbstractValue *downcast( const BaseClass &val) const {
    return static_cast<const BitpreciseWrappedAbstractValue *>(&val);
  }

  // Add constraints that bound the vocabulary v as bit-vectors integers
  // is_signed determines the sign of the operation
  virtual void AddBoundingConstraints(const VocabularySignedness& v);

  // Perform vanilla meet of the two values without worrying about bounding constraints
  // Warning: Blind use of these function might be unsound.
  virtual void VanillaMeet(const AbsValRefPtr &that) {
    const BitpreciseWrappedAbstractValue *that_wav = downcast(that);
    av_->Meet(that_wav->av_);
    wrapped_voc_ = UnionVocabularySignedness(wrapped_voc(), that_wav->wrapped_voc());
  }

  std::pair<std::pair<affexp_type, affexp_type>, OpType> 
  GetMinBoundingConstraint(const DimensionKey& k, bool is_signed);

  std::pair<std::pair<affexp_type, affexp_type>, OpType> 
  GetMaxBoundingConstraint(const DimensionKey& k, bool is_signed);

  // Returns a subset 's' of wrapped_voc such that for each (k,b) entry in 's', the dimension 'k' is in v
  static VocabularySignedness IntersectVocabularySignednessWithVocabulary(const VocabularySignedness& wrapped_voc, const Vocabulary& v) {
    VocabularySignedness result;
    // Only add those entries from wrapped_voc to result, which are in v
    for(VocabularySignedness::const_iterator it = wrapped_voc.begin(); it != wrapped_voc.end(); it++) {
      if(std::find(v.begin(), v.end(), it->first) != v.end())
        result.insert(*it);
    }
    return result;
  }

  static VocabularySignedness SubtractVocabularyFromVocabularySignedness(const VocabularySignedness& wrapped_voc, const Vocabulary& v) {
    VocabularySignedness result;
    // Only add those entries from wrapped_voc to result, which are not in v
    for(VocabularySignedness::const_iterator it = wrapped_voc.begin(); it != wrapped_voc.end(); it++) {
      if(std::find(v.begin(), v.end(), it->first) == v.end())
        result.insert(*it);
    }
    return result;
  }

  static VocabularySignedness SubtractVocabularySignednessFromVocabularySignedness(const VocabularySignedness& v1, const VocabularySignedness& v2) {
    VocabularySignedness result;
    // Only add those entries from v1 for which there is no entry in v2 or if there is it has a different sign
    VocabularySignedness::const_iterator it1 = v1.begin();
    VocabularySignedness::const_iterator it2 = v2.begin();
    for(;it1 != v1.end(); it1++) {
      VocabularySignedness::const_iterator it2_f = std::find(it2, v2.end(), *it1);
      if(it2_f == v2.end())
        result.insert(*it1);
      else
        it2 = it2_f;
    }
    return result;
  }

  static VocabularySignedness IntersectVocabularySignedness(const VocabularySignedness& v1, const VocabularySignedness& v2) {
    VocabularySignedness result;
    // Only add those entries which are both in v1  and v2 with the same exact sign
    VocabularySignedness::const_iterator it1 = v1.begin();
    VocabularySignedness::const_iterator it2 = v2.begin();
    for(;it1 != v1.end(); it1++) {
      VocabularySignedness::const_iterator it2_f = std::find(it2, v2.end(), *it1);
      if(it2_f != v2.end()) {
        it2 = it2_f;
        result.insert(*it1);
      }
    }
    return result;
  }

  static VocabularySignedness UnionVocabularySignedness(const VocabularySignedness& v1, const VocabularySignedness& v2) {
    VocabularySignedness result;
    // Only add those entries which are both in v1  and v2 with the same exact sign
    VocabularySignedness::const_iterator it1 = v1.begin();
    VocabularySignedness::const_iterator it2 = v2.begin();
    for(;it1 != v1.end(); it1++) {
      VocabularySignedness::const_iterator it2_f = std::find(it2, v2.end(), *it1);
      // Sanity check to ensure that the both vocabulary signedness are sound with respect to each other
      assert(it2_f == v2.end() || it1->second == it2_f->second);
      result.insert(*it1);
    }

    for(it2 = v2.begin(); it2 != v2.end(); it2++) {
      result.insert(*it2);
    }

    return result;
  }

  static Vocabulary GetVocabularyFromWrappedVoc(const VocabularySignedness& vs) {
    Vocabulary result;

    VocabularySignedness::const_iterator it = vs.begin();
    for(;it != vs.end(); it++) {
      result.insert(it->first);
    }
    return result;
  }
};

}
#endif // src_AbstractDomain_common_BitpreciseWrappedAbstractValue_hpp
