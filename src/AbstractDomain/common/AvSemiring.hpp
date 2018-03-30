#ifndef src_AbstractDomain_common_AvSemiring_hpp
#define src_AbstractDomain_common_AvSemiring_hpp

#include <map>
#include "external/wali/SemElem.hpp"
#include "AbstractValue.hpp"
using wali::SemElem;
using wali::sem_elem_t;

// WideningType is used to determine whether widening need to be called.
// This streategy doesn't need the WALi solver to change. It is all done in the domain.
// Widening rule is added by the client whenever there is a loop
// Type rules:
//   1) one, zero, bottom are regular weights
//   2) equality checks ignore widening type
//   3) extend: The first argument cannot be a widening rule and the second argument cannot be widening weight
//         *              | Regular         | Widening Rule
//        Regular         | Regular         | Widening Weight
//        Widening Weight | Widening Weight | Widening Weight
//   4) combine: Neither of the parameterms can be widening rule
//         +              | Regular              | Widening Wight       
//        Regular         | Regular              | Regular (Call widen)
//        Widening Weight | Regular (call widen) | Regular (call widen)
enum WideningType {REGULAR_WEIGHT, WIDENING_WEIGHT, WIDENING_RULE};

class AvSemiringStats
{
public:
  enum AvSemiringOperation { 
    JOIN,
    WIDEN,
    EXTEND,
    IS_EQUAL,
    NONE
  };

  /// Constructor
  AvSemiringStats() // Initializer; set default values.
  {
    reset();
  }

  void reset()
  {
    num_join_calls_ = 0;
    num_widen_calls_ = 0;
    num_extend_calls_ = 0;
    num_equal_calls_ = 0;

    time_join_ = 0;
    time_widen_ = 0;
    time_extend_ = 0;
    time_equal_ = 0;
  }

  // Used to print in stats file readable by SCons scripts in tools/tests, which uses it for reporting
  std::ostream &print(std::ostream &out) {
    out << std::dec << "Num_join_calls_=" << num_join_calls_ << std::endl;
    out << std::dec << "time_join_=" << time_join_ << std::endl;
    out << std::dec << "Num_widen_calls_=" << num_widen_calls_ << std::endl;
    out << std::dec << "time_widen_=" << time_widen_ << std::endl;
    out << std::dec << "Num_extend_calls_=" << num_extend_calls_ << std::endl;
    out << std::dec << "time_extend_=" << time_extend_ << std::endl;
    out << std::dec << "Num_equal_calls_=" << num_equal_calls_ << std::endl;
    out << std::dec << "time_equal_=" << time_equal_ << std::endl;
    return out;
  }

  // members
  unsigned num_join_calls_;
  unsigned num_widen_calls_;
  unsigned num_extend_calls_;
  unsigned num_equal_calls_;

  double time_join_;
  double time_widen_;
  double time_extend_;
  double time_equal_;
};

// TODO: Have Value as an Abstract class proving normal
// arithmetic operations as expected of Value.
class AvSemiring : public wali::SemElem
{
  typedef abstract_domain::AbstractValue AbstractValue;
public:
  // Statistics in AvQfbvSemiring for debugging purposes
  static AvSemiringStats av_semiring_stats_;

  AvSemiring(ref_ptr<AbstractValue> av, std::string from = "", std::string to = "", WideningType = REGULAR_WEIGHT, bool is_one = false);
  AvSemiring(const AvSemiring& a);
  virtual ~AvSemiring();
  virtual sem_elem_t combine(SemElem * a);
  virtual sem_elem_t extend(SemElem * a);
  virtual sem_elem_t quasi_one() const;
  sem_elem_t one() const;
  sem_elem_t zero() const;
  sem_elem_t bottom() const;

  // This method is different from one as it uses the vocabulary and equality operation
  // from av_ to construct a one
  // Assumes that is_one_ is set
  void SetToSyntacticOne();

  virtual bool isEqual(SemElem* a) const;
  bool equal(SemElem* a) const;
  bool equal(sem_elem_t a) const;

  const ref_ptr<AbstractValue> GetAbstractValue() const;
  ref_ptr<AbstractValue> GetAbstractValue();

  virtual void prettyPrint(FILE *fp, unsigned nTabs = 0);
  std::ostream & print(std::ostream &out) const;

  void setFrom(std::string from);
  void setTo(std::string to);
  void setWideningType(WideningType t);

protected:
  // Data Members
  ref_ptr<AbstractValue> av_;
  std::string from_, to_;
  WideningType t_;
  bool is_one_; // Canonical representation of semantic one
};

#endif // src_AbstractDomain_common_AvSemiring_hpp
