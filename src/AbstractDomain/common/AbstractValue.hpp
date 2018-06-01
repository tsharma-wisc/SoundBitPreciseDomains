#ifndef src_AbstractDomain_common_AbstractValue_hpp
#define src_AbstractDomain_common_AbstractValue_hpp

#include <cassert>
#include <memory>
#include <vector>
#include "dimension.hpp"
#include "wali/SemElem.hpp"
#include "wali/Countable.hpp"

template <typename T>
using ref_ptr = wali::ref_ptr<T>;

namespace abstract_domain {
class AbstractValue : public wali::Countable {

public:
  typedef std::map<DimensionKey, mpz_class> linexp_type;
  typedef std::pair<linexp_type, mpz_class> affexp_type;
  enum OpType {EQ, GE, LE};

  // constructors
  AbstractValue  ();
  AbstractValue  (Vocabulary v);

  // Deep copy
  virtual ref_ptr<AbstractValue> Copy() const = 0;

  // Destructor
  virtual ~ AbstractValue();

  virtual bool IsDistributive() const = 0;

  // Other ways to create AbstractValue elements
  // ====================================
  virtual ref_ptr<AbstractValue> Top    () const = 0;
  virtual ref_ptr<AbstractValue> Bottom () const = 0;

  //// I/O functions
  virtual std::string            ToString ()                    const = 0;

  //// Query operations
  virtual bool Equal            (const ref_ptr<AbstractValue> & av) const;
  virtual bool operator==       (const AbstractValue&)              const = 0;
  virtual bool operator!=       (const AbstractValue& av)           const;
  virtual bool IsBottom         ()                                  const = 0;
  virtual bool IsTop            ()                                  const = 0;
  // Does this overapproximates other?
  virtual bool Overapproximates (const ref_ptr<AbstractValue> &)    const = 0;
  Vocabulary   GetVocabulary    ()                                  const;

  //// Destructive updates
  virtual void Join            (const ref_ptr<AbstractValue> &) = 0;
  virtual void JoinSingleton   (const ref_ptr<AbstractValue> &) = 0;
  virtual void Meet            (const ref_ptr<AbstractValue> &) = 0;

  /// Widen operation should be overridden for domains with infinite lattices
  virtual void Widen           (const ref_ptr<AbstractValue> & that);

  // Each AbstractValue can implement its own extend to achieve more performance or precision
  // An example would be BitpreciseWrappedAbstractDomain that reimplment extend so that it can soundly
  // add bounding constraints to increase precision of the extend operation
  virtual bool ImplementsExtend();

  // Default implementation of extend returns an error
  virtual ref_ptr<AbstractValue> Extend(const ref_ptr<AbstractValue> &that) const;
  
  // Add equality constraint
  virtual void AddEquality     (const DimensionKey & v1, 
                                const DimensionKey & v2)   = 0;

  // In-place reduction
  virtual void Reduce() = 0;
  virtual void Reduce(const ref_ptr<AbstractValue>& that) = 0;

  // project: project onto Vocabulary v. 
  virtual void Project         (const Vocabulary & v) = 0;

  //Havoc: Havoc out the vocabulary v, i.e. remove any constraints on them 
  virtual ref_ptr<AbstractValue> Havoc           (const Vocabulary & v) const = 0;

  //// In - place Vocabulary manipulation operations
  //// on AbstractValue
  virtual size_t NumVars          () const;
  void AddDimension(const DimensionKey & k);
  virtual void     AddVocabulary    (const Vocabulary &)                   = 0;
  // Take all the key and replace their version numbers from old_ver to new_ver and then add
  // them to the abstract value. 
  void     AddVersion       (const Version & old_ver, const Version & new_ver);

  //Replace version a with b in the Abstract Value's vocabulary
  void ReplaceVersion   (const Version & o, const Version & n);

  virtual void     ReplaceVersions  (const std::map<Version, Version> &)   = 0;
  void     RemoveVocabulary (const Vocabulary & voc);
  void RemoveDimension  (const DimensionKey & k);

  // Get height of this AbstractValue in its lattice. If the lattice is infinite, return -1.
  virtual unsigned GetHeight () const {
    return (unsigned)-1;
  }

  std::ostream & print(std::ostream & out) const;

  /************** Interface used by Wrapped Domain LLVM reinterpretation layer ****************************/
  // Default implementation doesn't do anything
  // If your class intends to use BitpreciseWrappedAbstractValue to get correct
  // abstract domains, then this Wrap must be overloaded
  // Bool determines if the the key needs to be wrapped as signed or unsigned
  virtual void Wrap(const std::map<DimensionKey, bool>& vs) {
  }

  // Add constraint lhs op rhs
  // This function AddConstraint is used also by llvm_reinterpretation layer
  virtual void AddConstraint(affexp_type lhs, affexp_type rhs, OpType op) = 0;

  // This is equivalent to (lhs op 0)
  void AddConstraintNorhs(affexp_type lhs, OpType op) {
    affexp_type zero_affexp(linexp_type(), mpz_class(0));
    AddConstraint(lhs, zero_affexp, op);
  }

  // Is this an abstraction of a singleton value?
  // Default implementation raises assertion
  virtual bool IsConstant(mpz_class& val) const {
    assert(false);
    return false;
  }

  // Default implementation returns all the vocabulary
  // This is an overapproximation.
  // TODO: Change it to use quadratic number of havoc calls to determine
  // the minimum vocabulary that key k depends on
  // virtual Vocabulary GetDependentVocabulary(DimensionKey& k, Version ver) const {
  //   return getVocabularySubset(GetDependentVocabulary(k), ver);
  // }

  virtual Vocabulary GetDependentVocabulary(DimensionKey& k) const = 0;
  /*******************************************************************************************************/
protected:

  // members
  Vocabulary voc_;
};

}

std::ostream & operator << (std::ostream & out, const abstract_domain::AbstractValue& a);

#endif // src_AbstractDomain_common_AbstractValue_hpp
