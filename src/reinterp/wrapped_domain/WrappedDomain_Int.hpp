#ifndef src_reinterp_wrapped_domain_WrappedDomain_Int_hpp
#define src_reinterp_wrapped_domain_WrappedDomain_Int_hpp

#include "src/AbstractDomain/common/BitpreciseWrappedAbstractValue.hpp" 

enum TVL_BOOL {
  BOTTOM,
  ZERO,
  ONE,
  MAYBE
};

// For now, the bitsize is assumed to be fixed and at thirty-two
class WrappedDomain_Int {
public:
  typedef abstract_domain::BitpreciseWrappedAbstractValue BitpreciseWrappedAbstractValue;
  typedef abstract_domain::DimensionKey DimensionKey;
  typedef abstract_domain::AbstractValue AbstractValue;
  typedef abstract_domain::Vocabulary Vocabulary;
  typedef std::map<DimensionKey, bool> VocabularySignedness;
  // ==========================
  // Constructor and Destructor
  // ==========================
  // by default, construct top.
  explicit WrappedDomain_Int(utils::Bitsize bitsize, const wali::ref_ptr<AbstractValue>& av);
  WrappedDomain_Int(const WrappedDomain_Int& that);

private:
  WrappedDomain_Int(wali::ref_ptr<BitpreciseWrappedAbstractValue> av, DimensionKey k);
public:
  ~WrappedDomain_Int();

  WrappedDomain_Int & operator= (const WrappedDomain_Int &);

  // =============================
  // Ways to create WrappedDomain_Int objects
  // =============================
  WrappedDomain_Int top() const;
  WrappedDomain_Int bottom() const;

  // of_const: Make the WrappedDomain_Int representing the value 'c'
  WrappedDomain_Int of_const(mpz_class c) const;

  // of_relation: Make the WrappedDomain_Int holding the constraints in 'state',
  // without any constraints on the represented value.
  static WrappedDomain_Int of_relation(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& state, utils::Bitsize b);

  static WrappedDomain_Int of_relation(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& state, utils::Bitsize b, Vocabulary prevoc);

  // of_variable: Make the Ks_Int holding the pre-state constraints of 'weight'
  // and representing variable 'var'.
  // ('weight' is assumed to be the 'rel' field of a Ks_Two_Voc element.)
  static WrappedDomain_Int of_variable(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& weight, const DimensionKey& var);

  static WrappedDomain_Int of_variable(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& weight, const DimensionKey& var, const Vocabulary& prevoc);

  // =====================
  // Vocabulary Operations
  // =====================
  Vocabulary GetVocabulary() const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> get_one_voc_relation() const;

  // Get the key represented by this Int
  DimensionKey GetIntKey() const;

  // assign_to: Return the Ks_Rel "var := this".
  wali::ref_ptr<BitpreciseWrappedAbstractValue> assign_to_one_voc(DimensionKey var) const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> assign_to_two_voc(DimensionKey var, const Vocabulary& target_voc) const;

  // =====================
  // Relational Operations
  // =====================
  void join(const WrappedDomain_Int& that);
  void widen(const WrappedDomain_Int& that);
  void meet(const WrappedDomain_Int& that);
  void wrap(bool is_signed); // wrap on k_ depending on the sign

  // is_equal tests for *relational equality*
  bool is_equal(const WrappedDomain_Int &) const;

  // is_value_equal tests for "value equality" on its values.
  // For instance, if 'this' can be {k_>=0} and 'that' is {k_>=0}, return MAYBE.
  // (because this and that could be different in the concrete state!)
  TVL_BOOL is_value_equal(const WrappedDomain_Int & that) const;

  bool is_subset(const WrappedDomain_Int & that) const;

  bool operator==(const WrappedDomain_Int &) const;
  bool operator!=(const WrappedDomain_Int &) const;

  TVL_BOOL get_value() const; 

  bool is_bottom() const;
  bool is_top() const;

  // True if this value has exactly one concretization, if it returned true, then it sets the val to that
  // one concrete value
  bool is_constant(mpz_class& val) const; 
  WrappedDomain_Int any_value() const;

  // Add the given constraint to the Int
  void AddConstraint(const BitpreciseWrappedAbstractValue::affexp_type& ae, BitpreciseWrappedAbstractValue::OpType op);
  void AddConstraint(const BitpreciseWrappedAbstractValue::affexp_type& lhs, const BitpreciseWrappedAbstractValue::affexp_type& rhs, BitpreciseWrappedAbstractValue::OpType op);

  // ==================
  // Numeric Operations
  // ==================
  WrappedDomain_Int plus(const WrappedDomain_Int& that) const;
  WrappedDomain_Int plus(const mpz_class& that) const;

  WrappedDomain_Int minus(const WrappedDomain_Int& that) const;
  WrappedDomain_Int minus(const mpz_class& that) const;

  WrappedDomain_Int times(const WrappedDomain_Int& that) const;
  WrappedDomain_Int times(const mpz_class& that) const;

  WrappedDomain_Int sdiv(const WrappedDomain_Int& that) const;
  WrappedDomain_Int sdiv(const mpz_class& that) const;

  WrappedDomain_Int smod(const WrappedDomain_Int& that) const;
  WrappedDomain_Int smod(const mpz_class& that) const;

  WrappedDomain_Int udiv(const WrappedDomain_Int& that) const;
  WrappedDomain_Int udiv(const mpz_class& that) const;

  WrappedDomain_Int umod(const WrappedDomain_Int& that) const;
  WrappedDomain_Int umod(const mpz_class& that) const;

  WrappedDomain_Int negate() const;

  WrappedDomain_Int lshift(const WrappedDomain_Int & that) const;
  WrappedDomain_Int lshift(const mpz_class& that) const;

  WrappedDomain_Int rshift(const WrappedDomain_Int& that) const;
  WrappedDomain_Int rshift(const mpz_class& that) const;

  WrappedDomain_Int rshift_arith(const WrappedDomain_Int & that) const;

  WrappedDomain_Int bitwise_and(const WrappedDomain_Int & that) const;
  WrappedDomain_Int bitwise_or(const WrappedDomain_Int & that) const;
  WrappedDomain_Int bitwise_xor(const WrappedDomain_Int & that) const;
  WrappedDomain_Int bitwise_not() const;

  // abs_equal: Return an abstract boolean representing "this == that".
  wali::ref_ptr<BitpreciseWrappedAbstractValue> abs_equal(const WrappedDomain_Int & that) const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> abs_not_equal(const WrappedDomain_Int & that) const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> less_than_signed(const WrappedDomain_Int & that) const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> less_than_unsigned(const WrappedDomain_Int & that) const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> less_than_or_equal_signed(const WrappedDomain_Int & that) const;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> less_than_or_equal_unsigned(const WrappedDomain_Int & that) const;

  static wali::ref_ptr<BitpreciseWrappedAbstractValue> boolean_and(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b1, const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b2);
  static wali::ref_ptr<BitpreciseWrappedAbstractValue> boolean_or(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b1, const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b2);
  static wali::ref_ptr<BitpreciseWrappedAbstractValue> boolean_xor(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b1, const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b2);
  static wali::ref_ptr<BitpreciseWrappedAbstractValue> boolean_not(const wali::ref_ptr<BitpreciseWrappedAbstractValue>& b1);

  // These operations are currently not supported as the only bitsize currently expressible is 32 bits
  WrappedDomain_Int zero_extend(utils::Bitsize b) const;
  WrappedDomain_Int sign_extend(utils::Bitsize b) const;
  WrappedDomain_Int trunc(utils::Bitsize b) const;
  WrappedDomain_Int zero_extend_or_trunc(utils::Bitsize b) const;

  void UpdateIntSignedness();

  // ======
  // Output
  // ======
  std::ostream & print(std::ostream & out) const;
  std::string str() const;

private:
  WrappedDomain_Int lin_combine(const WrappedDomain_Int& that, 
                                mpz_class new_coeff, mpz_class this_coeff,
                                mpz_class that_coeff, mpz_class constant) const;
  WrappedDomain_Int lin_transform(mpz_class new_coeff, mpz_class old_coeff, mpz_class constant) const;

  // These functions are used both by less_than and less_than_or_equal operations after they have perform wrapping
  // on their arguments. Note that this function can modify that and this
  wali::ref_ptr<BitpreciseWrappedAbstractValue> less_than_or_equal_no_wrapping(WrappedDomain_Int & that, bool is_signed);
  WrappedDomain_Int div_helper(const mpz_class& that, bool is_signed) const;
  WrappedDomain_Int mod_helper(const mpz_class& that, bool is_signed) const;

  // Handle to the dimension k_ in av_ to which this value refers to
  DimensionKey k_;
  bool wrapped_;
  bool wrapped_is_signed_;
  wali::ref_ptr<BitpreciseWrappedAbstractValue> wav_;
};

#endif // src_reinterp_wrapped_domain_WrappedDomain_Int_hpp
