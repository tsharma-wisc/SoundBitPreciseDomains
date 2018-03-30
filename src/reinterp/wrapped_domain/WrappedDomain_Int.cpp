#include "src/reinterp/wrapped_domain/WrappedDomain_Int.hpp" 
#include "utils/timer/timer.hpp"

DimensionKey GetTempKey(unsigned numbits, unsigned num) {
  std::stringstream ss;
  ss << "tmp_" << num;
  return DimensionKey(ss.str(), 0, numbits);
}

// Constructors and destructors
// av specifies the domain and the vocabulary that this Int value would be representing
WrappedDomain_Int::WrappedDomain_Int
(utils::Bitsize bitsize, 
 const ref_ptr<abstract_value::AbstractValue>& av) 
  : k_(GetTempKey(bitsize, 0)), wrapped_(false), wrapped_is_signed_(true) {
  ref_ptr<AbstractValue> av_cp = av->Copy();
  av_cp->AddDimension(k_);
  wav_ = new BitpreciseWrappedAbstractValue(av_cp, VocabularySignedness()); // Do not wrap anything
}

WrappedDomain_Int::WrappedDomain_Int(ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> wav, DimensionKey k) : k_(k), wrapped_(false), wrapped_is_signed_(true), wav_(wav) {
}

WrappedDomain_Int::WrappedDomain_Int(const WrappedDomain_Int& that) : k_(that.k_), wrapped_(that.wrapped_), wrapped_is_signed_(that.wrapped_is_signed_)  {
  this->wav_ = new BitpreciseWrappedAbstractValue(*(that.wav_));
}

WrappedDomain_Int::~WrappedDomain_Int() {
}

WrappedDomain_Int& WrappedDomain_Int::operator=(const WrappedDomain_Int& that) {
  if(this != &that) {
    this->wav_ = new BitpreciseWrappedAbstractValue(*(that.wav_));
    this->k_ = that.k_;
    this->wrapped_ = that.wrapped_;
    this->wrapped_is_signed_ = that.wrapped_is_signed_;
  }
  return *this;
}

WrappedDomain_Int WrappedDomain_Int::top() const {
  ref_ptr<BitpreciseWrappedAbstractValue> tp = dynamic_cast<BitpreciseWrappedAbstractValue*>(wav_->Top().get_ptr());
  return WrappedDomain_Int(tp, k_);
}

WrappedDomain_Int WrappedDomain_Int::bottom() const {
  ref_ptr<BitpreciseWrappedAbstractValue> btm = dynamic_cast<BitpreciseWrappedAbstractValue*>(wav_->Bottom().get_ptr());
  return WrappedDomain_Int(btm, k_);
}

WrappedDomain_Int WrappedDomain_Int::of_const(mpz_class c) const {
  WrappedDomain_Int tmp_top = top();

  // Add constraint 1*k_ = c
  AbstractValue::linexp_type const_constr_linexp_lhs;
  const_constr_linexp_lhs.insert(AbstractValue::linexp_type::value_type(tmp_top.k_, 1));
  AbstractValue::affexp_type const_constr_lhs(const_constr_linexp_lhs, 0);
  AbstractValue::affexp_type const_constr_rhs(AbstractValue::linexp_type(), c);
  tmp_top.wav_->AddConstraint(const_constr_lhs, const_constr_rhs, AbstractValue::EQ);
  return tmp_top;
}

WrappedDomain_Int WrappedDomain_Int::of_relation(const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& state, utils::Bitsize bitsize) {
  DimensionKey k = GetTempKey(bitsize, 0);
  ref_ptr<BitpreciseWrappedAbstractValue> state_with_k = new BitpreciseWrappedAbstractValue(*state);
  state_with_k->AddDimension(k);
  WrappedDomain_Int ret(state_with_k, k);
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::of_variable(const ref_ptr<BitpreciseWrappedAbstractValue>& weight, const DimensionKey& var) {
  // This option is disabled as using of_variable with specifying correct prevoc will be unsound
  // when wrap is called, as it will add bounding constraints to variables that are not in 
  // prevoc
  assert(false);
  // Set prevoc to the (weight voculary - postvoc)
  // In presence of unversioned variables, it will have them in prevoc as well
  // This is the most precise, but costly, valuation for prevoc (unless client provides live var
  // info in prevoc, then client can use the overloaded of_variable) 
  Vocabulary voc = weight->GetVocabulary();
  Vocabulary voc_prime = abstract_domain::getVocabularySubset(voc, Version(1));
  Vocabulary prevoc;
  SubtractVocabularies(voc, voc_prime, prevoc);
  return of_variable(weight, var, prevoc);
}

WrappedDomain_Int WrappedDomain_Int::of_variable(const ref_ptr<BitpreciseWrappedAbstractValue>& weight_av, const DimensionKey& var, const Vocabulary& prevoc) {
  ref_ptr<BitpreciseWrappedAbstractValue> weight 
    = dynamic_cast<BitpreciseWrappedAbstractValue*>(weight_av->Copy().get_ptr());

  utils::Bitsize bitsize = var.GetBitsize();
  
  DimensionKey k = GetTempKey(bitsize, 0);
  weight->AddDimension(k);

  // Add constraint 1*k_ = 1*var
  AbstractValue::linexp_type constr_linexp_lhs;
  constr_linexp_lhs.insert(AbstractValue::linexp_type::value_type(k, 1));
  BitpreciseWrappedAbstractValue::affexp_type constr_lhs(constr_linexp_lhs, 0);
  BitpreciseWrappedAbstractValue::linexp_type constr_linexp_rhs;
  constr_linexp_rhs.insert(AbstractValue::linexp_type::value_type(var, 1));
  BitpreciseWrappedAbstractValue::affexp_type constr_rhs(constr_linexp_rhs, 0);
  weight->AddConstraint(constr_lhs, constr_rhs, BitpreciseWrappedAbstractValue::EQ);

  // Adding wrapping conditions to k only if var is wrapped
  bool is_signed;
  if(weight->IsWrapped(var, is_signed))
    weight->MarkWrapped(k, is_signed);

  Vocabulary prevoc_plus_k = prevoc; prevoc_plus_k.insert(k);
  ref_ptr<BitpreciseWrappedAbstractValue> weight_prevoc = weight;
  weight->Project(prevoc_plus_k);

  WrappedDomain_Int ret(weight, k);
  return ret;
}

Vocabulary WrappedDomain_Int::GetVocabulary() const {
  return wav_->GetVocabulary();
}

// assign_to: Return the abstract value with k_ projected out
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::get_one_voc_relation() const {
  ref_ptr<BitpreciseWrappedAbstractValue> ret = new BitpreciseWrappedAbstractValue(*wav_);
  ret->RemoveDimension(k_);
  return ret;
}

// assign_to: Return the abstract value with var assigned as "var := this".
// This function is the key operation in implementing variable updates
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::assign_to_one_voc(DimensionKey var) const {
  ref_ptr<BitpreciseWrappedAbstractValue> ret = new BitpreciseWrappedAbstractValue(*wav_);
  Vocabulary voc_var; voc_var.insert(var);
  ret = dynamic_cast<BitpreciseWrappedAbstractValue*>(ret->Havoc(voc_var).get_ptr());

  // Add constraint 1*k_ = 1*var
  AbstractValue::linexp_type constr_linexp_lhs;
  constr_linexp_lhs.insert(AbstractValue::linexp_type::value_type(k_, 1));
  AbstractValue::affexp_type constr_lhs(constr_linexp_lhs, 0);
  AbstractValue::linexp_type constr_linexp_rhs;
  constr_linexp_rhs.insert(AbstractValue::linexp_type::value_type(var, 1));
  AbstractValue::affexp_type constr_rhs(constr_linexp_rhs, 0);
  ret->AddConstraint(constr_lhs, constr_rhs, AbstractValue::EQ);

  // Adding wrapping conditions to k only if var is wrapped
  bool is_signed;
  if(ret->IsWrapped(k_, is_signed))
    ret->MarkWrapped(var, is_signed);

  ret->RemoveDimension(k_);
  return ret;
}

ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::assign_to_two_voc(DimensionKey var, const Vocabulary& target_voc) const {
  ref_ptr<BitpreciseWrappedAbstractValue> ret = new BitpreciseWrappedAbstractValue(*wav_);
  ret->AddVocabulary(target_voc);

  Vocabulary voc_var; voc_var.insert(var);
  ret = dynamic_cast<BitpreciseWrappedAbstractValue*>(ret->Havoc(voc_var).get_ptr());

  // Add constraint 1*k_ = 1*var
  AbstractValue::linexp_type constr_linexp_lhs;
  constr_linexp_lhs.insert(AbstractValue::linexp_type::value_type(k_, 1));
  BitpreciseWrappedAbstractValue::affexp_type constr_lhs(constr_linexp_lhs, 0);
  BitpreciseWrappedAbstractValue::linexp_type constr_linexp_rhs;
  constr_linexp_rhs.insert(AbstractValue::linexp_type::value_type(var, 1));
  BitpreciseWrappedAbstractValue::affexp_type constr_rhs(constr_linexp_rhs, 0);
  ret->AddConstraint(constr_lhs, constr_rhs, BitpreciseWrappedAbstractValue::EQ);

  // Adding wrapping conditions to k only if var is wrapped
  bool is_signed;
  if(ret->IsWrapped(k_, is_signed))
    ret->MarkWrapped(var, is_signed);

  ret->Project(target_voc);
  return ret;
}

// =====================
// Relational Operations
// =====================
void WrappedDomain_Int::join(const WrappedDomain_Int& that) {
  wav_->Join(that.wav_);
}

void WrappedDomain_Int::widen(const WrappedDomain_Int& that) {
  wav_->Widen(that.wav_);
}

void WrappedDomain_Int::meet(const WrappedDomain_Int& that) {
  wav_->Meet(that.wav_);
}

void WrappedDomain_Int::wrap(bool is_signed) {
  if(wrapped_ && wrapped_is_signed_ == is_signed)
    return;

  DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                 print(std::cout << "\nCalling wrap on:"););

  // Add bounding constraints to the vocabulary which k_ depends on.
  // Use is_signed as the signedness of their bounding
  Vocabulary v = wav_->GetDependentVocabulary(k_);
  BitpreciseWrappedAbstractValue::VocabularySignedness v_sign;
  for(Vocabulary::const_iterator it = v.begin(); it != v.end(); it++) {
    v_sign.insert(std::make_pair(*it, is_signed));
  }
  wav_->AddBoundingConstraints(v_sign);
  DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                 abstract_domain::print(std::cout << "\ndependent_voc:", v);
                 print(std::cout << "\nAfter adding bounding cnstrs:"););

  wav_->Wrap(k_, is_signed);
  wrapped_ = true;
  wrapped_is_signed_ = is_signed;
  DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                 print(std::cout << "\nAfter wrap:"););
}

// is_equal tests for *relational equality*
bool WrappedDomain_Int::is_equal(const WrappedDomain_Int& that) const {
  return (*wav_ == *that.wav_);
}

bool WrappedDomain_Int::operator==(const WrappedDomain_Int& that) const {
  return is_equal(that);
}

bool WrappedDomain_Int::operator!=(const WrappedDomain_Int& that) const {
  return !(*this == that);
}

// is_value_equal tests for "value equality" on its values.
// For instance, if 'this' can be {k_>=0} and 'that' is {k_>=0}, return MAYBE.
// (because this and that could be different in the concrete state!)
TVL_BOOL WrappedDomain_Int::is_value_equal(const WrappedDomain_Int & that) const {
  if(is_bottom() || that.is_bottom()) {
    return TVL_BOOL::BOTTOM;
  }
  else if(is_top() || that.is_top()) {
    // Atleast one of the values in uncontrained and hence,
    // they can be equal.
    return TVL_BOOL::MAYBE;
  }
  else if(is_equal(that)) {
    mpz_class c;
    if(is_constant(c)) {
      // In each state, these values are singular and equal.
      return TVL_BOOL::ONE;
    }
    else {
      return TVL_BOOL::MAYBE;
    }
  }
  else {
    ref_ptr<abstract_value::AbstractValue> m = wav_->Copy();
    m->Meet(that.wav_.get_ptr());
    if(m->IsBottom()) {
      return TVL_BOOL::ZERO;
    }
  }
  return TVL_BOOL::MAYBE;
}


bool WrappedDomain_Int::is_subset(const WrappedDomain_Int & that) const {
  return that.wav_->Overapproximates(wav_.get_ptr());
}


bool WrappedDomain_Int::is_bottom() const {
  return wav_->IsBottom();
}

bool WrappedDomain_Int::is_top() const {
  return wav_->IsTop();
}

// True if this value has exactly one concretization.
// Return the constant value in val if true.
bool WrappedDomain_Int::is_constant(mpz_class& val) const {
  if(wav_->IsBottom())
    return false;

  ref_ptr<BitpreciseWrappedAbstractValue> avcopy = new BitpreciseWrappedAbstractValue(*wav_);

  // Project on only k
  Vocabulary voc_k; voc_k.insert(k_);
  avcopy->Project(voc_k);

  return avcopy->IsConstant(val);
}

// Return this having havocing k_ in av_
WrappedDomain_Int WrappedDomain_Int::any_value() const {
  Vocabulary voc_k; voc_k.insert(k_);
  ref_ptr<BitpreciseWrappedAbstractValue> any_val_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(wav_->Havoc(voc_k).get_ptr());
 
  return WrappedDomain_Int(any_val_av, k_);
}

// Get the key represented by this Int
DimensionKey WrappedDomain_Int::GetIntKey() const {
  return k_;
}

void WrappedDomain_Int::AddConstraint(const AbstractValue::affexp_type& ae, AbstractValue::OpType op) {
  wav_->AddConstraintNorhs(ae, op);
}

void WrappedDomain_Int::AddConstraint(const AbstractValue::affexp_type& lhs, const AbstractValue::affexp_type& rhs, AbstractValue::OpType op) {
  wav_->AddConstraint(lhs, rhs, op);
}

#define MPZ_MINUS_ONE mpz_class(-1)
#define MPZ_ONE mpz_class(1)
#define MPZ_ZERO mpz_class(0)

// ==================
// Numeric Operations
// ==================
WrappedDomain_Int WrappedDomain_Int::plus(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) {
    return plus(c);
  } else if(is_constant(c)) {
    return that.plus(c);
  }
   
  return lin_combine(that, MPZ_MINUS_ONE, MPZ_ONE, MPZ_ONE, MPZ_ZERO);
}

WrappedDomain_Int WrappedDomain_Int::plus(const mpz_class& that) const {
  return lin_transform(MPZ_MINUS_ONE, MPZ_ONE, that);
}

WrappedDomain_Int WrappedDomain_Int::minus(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) {
    return minus(c);
  } else if(is_constant(c)) {
    return that.minus(c);
  }
  return lin_combine(that, MPZ_MINUS_ONE, MPZ_ONE, MPZ_MINUS_ONE, MPZ_ZERO);
}

WrappedDomain_Int WrappedDomain_Int::negate() const {
  return lin_transform(MPZ_ONE, MPZ_ONE, MPZ_ZERO);
}

WrappedDomain_Int WrappedDomain_Int::minus(const mpz_class& that) const {
  return lin_transform(MPZ_MINUS_ONE, MPZ_ONE, MPZ_ZERO - that);
}

WrappedDomain_Int WrappedDomain_Int::times(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(is_constant(c)) { return that.times(c); }
  else if(that.is_constant(c)) { return times(c); }
  else {
    // TODO: We should be able to do something clever if inequality information is present
    WrappedDomain_Int ret = any_value();
    ret.meet(that.any_value());
    return ret;
  }
  UWAssert::shouldNeverHappen();
  return top();
}

WrappedDomain_Int WrappedDomain_Int::times(const mpz_class& that) const {
  mpz_class c;
  if(is_constant(c))
    return of_const(c*that);
  WrappedDomain_Int ret = lin_transform(MPZ_MINUS_ONE, that, MPZ_ZERO);
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::udiv(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) {
    return udiv(c);
  }       
  WrappedDomain_Int ret = any_value();
  ret.meet(that.any_value());
  return ret;
} 

WrappedDomain_Int WrappedDomain_Int::udiv(const mpz_class& that) const {
  mpz_class c;
  if(is_constant(c)) {
    utils::Bitsize bitsize = k_.GetBitsize();
    utils::Value c_val = utils::convert_to_value(c, b, false/*is_signed*/);
    utils::Value that_val = utils::convert_to_value(that, b, false/*is_signed*/);
    utils::Value div = c_val / that_val;
    // Use false for is_signed
    return of_const(convert_to_mpz(div, false));
  }

  return div_helper(that, false/*is_signed*/);
}

WrappedDomain_Int WrappedDomain_Int::sdiv(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) {
    return sdiv(c);
  }       
  WrappedDomain_Int ret = any_value();
  ret.meet(that.any_value());
  return ret;
} 

WrappedDomain_Int WrappedDomain_Int::sdiv(const mpz_class& that) const {
  // Q: Why isn't all this just lin_transform(-1, that, 0)?
  // A: Because division in the matrix ring isn't the same as 'div'.
  // "division" in the ring is the inverse of multiplication in the ring.
  // for instance: (7 div 3) = 2, but 7x = 3 [mod 2^32] has solution x = 613566757.
  mpz_class c;
  if(is_constant(c)) {
    utils::Bitsize b = k_.GetBitsize();
    // Use true for is_signed
    utils::Value c_val = utils::convert_to_value(c, b, true/*is_signed*/);
    utils::Value that_val = utils::convert_to_value(that, b, true/*is_signed*/);
    utils::Value div = c_val.sdiv(that_val);
    return of_const(convert_to_mpz(div, true));
  }

  return div_helper(that, true/*is_signed*/);
}

WrappedDomain_Int WrappedDomain_Int::div_helper(const mpz_class& that_mpz, bool is_signed) const {
  WrappedDomain_Int ret(*this);
  ret.wrap(is_signed); // Wrapping is necessary for correctness

  utils::Bitsize b = k_.GetBitsize();
  utils::Value that_val = utils::convert_to_value(that_mpz, b, is_signed);
  mpz_class that = utils::convert_to_mpz(that_val, is_signed);
  
  // Create old_k
  DimensionKey old_k = GetTempKey(b, 1);
  ret.wav_->AddDimension(old_k);

  // Add assertion old_k <= that*k_ <= old_k + (that - 1)
  // The above inequality essentially assert that the integer division gives a floor value
  // of the actual rational which was obtained after divide

  // Add equality k_=old_k and then havoc k_, so that essentially the key k_ is replaced with old_k
  ret.wav_->AddEquality(k_, old_k);
  Vocabulary voc_k; voc_k.insert(k_);
  ret.wav_ = dynamic_cast<BitpreciseWrappedAbstractValue*>(ret.wav_->Havoc(voc_k).get_ptr());

  // Add constraint old_k <= that*k_
  AbstractValue::linexp_type le_lhs2, le_rhs2;
  le_lhs2.insert(std::make_pair(old_k, MPZ_ONE));
  le_rhs2.insert(std::make_pair(k_, that));
  AbstractValue::affexp_type ae_lhs2(le_lhs2, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs2(le_rhs2, MPZ_ZERO);
  ret.wav_->AddConstraint(ae_lhs2, ae_rhs2, AbstractValue::LE);

  // Add constraint that*new_k <= old_k + (that - 1)
  AbstractValue::linexp_type le_lhs3, le_rhs3;
  le_lhs3.insert(std::make_pair(k_, that));
  le_rhs3.insert(std::make_pair(old_k, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs3(le_lhs3, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs3(le_rhs3, that - MPZ_ONE);
  ret.wav_->AddConstraint(ae_lhs3, ae_rhs3, AbstractValue::LE);

  // Mark k_ as wrapped
  ret.wav_->MarkWrapped(k_, is_signed);

  // Project away old_k
  ret.wav_->RemoveDimension(old_k);
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::smod(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) {
    return smod(c);
  }

  WrappedDomain_Int ret = *this;
  ret.meet(that);
  return ret.any_value();
}

WrappedDomain_Int WrappedDomain_Int::smod(const mpz_class& that) const {
  mpz_class c;
  if(is_constant(c)) {
    utils::Bitsize b = k_.GetBitsize();
    // Use true for is_signed
    utils::Value c_val = utils::convert_to_value(c, b, true/*is_signed*/);
    utils::Value that_val = utils::convert_to_value(that, b, true/*is_signed*/);
    utils::Value mod = c_val.smod(that_val);
    return of_const(convert_to_mpz(mod, true));
  }

  return mod_helper(that, true/*is_signed*/);
}

WrappedDomain_Int WrappedDomain_Int::umod(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) {
    return umod(c);
  }

  WrappedDomain_Int ret = *this;
  ret.meet(that);
  return ret.any_value();
}

WrappedDomain_Int WrappedDomain_Int::umod(const mpz_class& that) const {
  mpz_class c;
  if(is_constant(c)) {
    utils::Bitsize b = k_.GetBitsize();
    // Use false for is_signed
    utils::Value c_val = utils::convert_to_value(c, b, false/*is_signed*/);
    utils::Value that_val = utils::convert_to_value(that, b, false/*is_signed*/);
    utils::Value mod = c_val%that_val;
    return of_const(convert_to_mpz(mod, false));
  }

  return mod_helper(that, false/*is_signed*/);
}

WrappedDomain_Int WrappedDomain_Int::mod_helper(const mpz_class& that_mpz, bool is_signed) const {
  WrappedDomain_Int ret(*this);
  ret.wrap(is_signed); // Wrapping is necessary for correctness

  utils::Bitsize b = k_.GetBitsize();
  utils::Value that_val = utils::convert_to_value(that_mpz, b, is_signed);
  mpz_class that = utils::convert_to_mpz(that_val, is_signed);
  

  // Create old_k
  DimensionKey old_k = GetTempKey(b, 1);
  ret.wav_->AddDimension(old_k);

  // Add assertion 0 <= k_ <= that - 1

  // Add equality k_ = old_k and then havoc k_, so that essentially the key k_ is replaced with old_k
  ret.wav_->AddEquality(k_, old_k);
  Vocabulary voc_k; voc_k.insert(k_);
  ret.wav_ = dynamic_cast<BitpreciseWrappedAbstractValue*>(ret.wav_->Havoc(voc_k).get_ptr());

  // Add constraint k_ >= 0
  AbstractValue::linexp_type le_lhs2;
  le_lhs2.insert(std::make_pair(k_, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs2(le_lhs2, MPZ_ZERO);
  ret.wav_->AddConstraintNorhs(ae_lhs2, AbstractValue::GE);

  // Add constraint k_ <= that - 1
  AbstractValue::linexp_type le_lhs3, le_rhs3;
  le_lhs3.insert(std::make_pair(k_, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs3(le_lhs3, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs3(le_rhs3, that - MPZ_ONE);
  ret.wav_->AddConstraint(ae_lhs3, ae_rhs3, AbstractValue::LE);

  // Mark k_ as wrapped
  ret.wav_->MarkWrapped(k_, is_signed);

  // Project away old_k
  ret.wav_->RemoveDimension(old_k);
  return ret;
}

// ========================
// Bit-twiddling operations
// ========================
WrappedDomain_Int WrappedDomain_Int::lshift(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) return lshift(c);

  WrappedDomain_Int ret = *this;
  ret.meet(that);
  return ret.any_value();
}

WrappedDomain_Int WrappedDomain_Int::lshift(const mpz_class& that) const {
  return times(utils::mpz_lshift(MPZ_ONE, that));
}

WrappedDomain_Int WrappedDomain_Int::rshift(const WrappedDomain_Int& that) const {
  mpz_class c;
  if(that.is_constant(c)) return rshift(c);

  WrappedDomain_Int ret = *this;
  ret.meet(that);
  return ret.any_value();
}

WrappedDomain_Int WrappedDomain_Int::rshift(const mpz_class& that) const {
  utils::Bitsize b = k_.GetBitsize();
  utils::Value that_val = utils::convert_to_value(that, b, false/*is_signed*/);
  that_val = utils::Value(b, 1ull) << that_val;
  return udiv(utils::convert_to_mpz(that_val, false/*is_signed*/));
}

WrappedDomain_Int WrappedDomain_Int::rshift_arith(const WrappedDomain_Int& that) const {
  utils::Bitsize b = k_.GetBitsize();
  mpz_class c;
  if (that.is_constant(c)) {
    mpz_class that_mpz = c;
    if (that_mpz == MPZ_ZERO) return WrappedDomain_Int(*this);
    if (is_constant(c)) {
      // this_mpz_trunc is the signed truncated representation of this_mpz
      utils::Value this_val = utils::convert_to_value(c, b, true/*is_signed*/);
      mpz_class this_mpz_trunc = utils::convert_to_mpz(this_val, true/*is_signed*/);
      mpz_class rshift_result = utils::mpz_rshift_arith(this_mpz_trunc, that_mpz);

      WrappedDomain_Int ret = any_value();
      ret.meet(that.any_value());
      ret.meet(of_const(rshift_result));
      return ret;
    }
    // WARNING: Assuming that rshift_arith is same as signed division for LLVM operators
    // This is not true for C-based operations
    else {
      return sdiv(utils::mpz_lshift(MPZ_ONE, that_mpz));
    }
  }
  else return any_value();
}

WrappedDomain_Int WrappedDomain_Int::bitwise_and(const WrappedDomain_Int& that) const {
  utils::Bitsize b = k_.GetBitsize();
  WrappedDomain_Int ret = any_value();
  ret.meet(that.any_value());

  mpz_class this_c, that_c;
  bool is_this_cons = is_constant(this_c);
  bool is_that_cons = that.is_constant(that_c);
  mpz_class this_c_trunc = utils::convert_to_mpz(utils::convert_to_value(this_c, b, false/*is_signed*/), false/*is_signed*/);
  mpz_class that_c_trunc = utils::convert_to_mpz(utils::convert_to_value(that_c, b, false/*is_signed*/), false/*is_signed*/);

  // TODO: More precision can be obtained if and was done with a non-zero constant with trailing zero bits
  // If that happens then a better lower bound on the other non-constant value can be obtained.
  // Note that this only makes sense when the values are treated as unsigned, which we currently cannot handle.

  // Handle special constant cases (ie. when one of the operand is a constant with bits all 0 or all 1
  if ( (is_this_cons && this_c_trunc == MPZ_ZERO) || (is_that_cons && that_c_trunc == MPZ_ZERO) ) {
    ret.meet(of_const(MPZ_ZERO));
  }
  else if(is_this_cons && this_c_trunc == MPZ_MINUS_ONE) {
    ret.meet(that);
  }
  else if(is_that_cons && that_c_trunc == MPZ_MINUS_ONE) {
    ret.meet(*this);
  }
  else if(is_this_cons && is_that_cons) {
    mpz_class result = utils::mpz_and(this_c_trunc, that_c_trunc);
    WrappedDomain_Int c = of_const(result); 
    ret.meet(c);
  }
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::bitwise_or(const WrappedDomain_Int& that) const {
  utils::Bitsize b = k_.GetBitsize();
  WrappedDomain_Int ret = any_value();
  ret.meet(that.any_value());

  mpz_class this_c, that_c;
  bool is_this_cons = is_constant(this_c);
  bool is_that_cons = that.is_constant(that_c);
  mpz_class this_c_trunc = utils::convert_to_mpz(utils::convert_to_value(this_c, b, false/*is_signed*/), false/*is_signed*/);
  mpz_class that_c_trunc = utils::convert_to_mpz(utils::convert_to_value(that_c, b, false/*is_signed*/), false/*is_signed*/);

  // TODO: More precision can be obtained if or was done with a non-zero constant with leading one bits
  // If that happens then a better lower bound on the other non-constant value can be obtained
  // Note that this only makes sense when the values are treated as unsigned, which we currently cannot handle.

  // Handle special constant cases (ie. when one of the operand is a constant with bits all 0 or all 1
  if ( (is_this_cons && this_c_trunc == MPZ_MINUS_ONE) || (is_that_cons && that_c_trunc == MPZ_MINUS_ONE) ) {
    ret.meet(of_const(MPZ_MINUS_ONE));
  }
  else if(is_this_cons && this_c_trunc == MPZ_ZERO) {
    ret.meet(that);
  }
  else if(is_that_cons && that_c_trunc == MPZ_ZERO) {
    ret.meet(*this);
  }
  else if(is_this_cons && is_that_cons) {
    mpz_class result = utils::mpz_or(this_c_trunc, that_c_trunc);
    WrappedDomain_Int c = of_const(result); 
    ret.meet(c);
  }

  return ret;
}

WrappedDomain_Int WrappedDomain_Int::bitwise_xor(const WrappedDomain_Int& that) const {
  utils::Bitsize b = k_.GetBitsize();
  WrappedDomain_Int ret = any_value();
  ret.meet(that.any_value());

  mpz_class this_c, that_c;
  bool is_this_cons = is_constant(this_c);
  bool is_that_cons = that.is_constant(that_c);
  mpz_class this_c_trunc = utils::convert_to_mpz(utils::convert_to_value(this_c, b, true/*is_signed*/), true/*is_signed*/);
  mpz_class that_c_trunc = utils::convert_to_mpz(utils::convert_to_value(that_c, b, false/*is_signed*/), true/*is_signed*/);

  // Handle special constant cases (ie. when one of the operand is a constant with bits all 0 or all 1
  if(is_this_cons && this_c_trunc == MPZ_MINUS_ONE) {
    ret.meet(that.bitwise_not());
  }
  else if(is_that_cons && that_c_trunc == MPZ_MINUS_ONE) {
    ret.meet(bitwise_not());
  }
  else if(is_this_cons && this_c_trunc == MPZ_ZERO) {
    ret.meet(that);
  }
  else if(is_that_cons && that_c_trunc == MPZ_ZERO) {
    ret.meet(*this);
  }
  else if(is_this_cons && is_that_cons) {
    mpz_class result = utils::mpz_xor(this_c_trunc, that_c_trunc);
    WrappedDomain_Int c = of_const(result); 
    ret.meet(c);
  }

  return ret;
}

WrappedDomain_Int WrappedDomain_Int::bitwise_not() const {
  mpz_class c;
  if(is_constant(c)) { 
    WrappedDomain_Int ret = any_value();
    WrappedDomain_Int not_int = of_const(~c);
    ret.meet(not_int);
    return ret;     
  }

  // Wrapping is neccessary to ensure soundness
  WrappedDomain_Int wrapped_this = *this;

  // TODO: Every variable is assumed to be signed, which is not unsound for this operation
  // but it can be imprecise in this operation
  wrapped_this.wrap(true/*is_signed*/);
  return wrapped_this.negate().plus(MPZ_MINUS_ONE); 
}

WrappedDomain_Int WrappedDomain_Int::zero_extend(utils::Bitsize b) const {
  // Wrapping (as unsigned) needs to be performed before zero extension can be done
  WrappedDomain_Int ret = *this;
  ret.wrap(false/*is_signed*/);
  DimensionKey k_b = GetTempKey(b, 0);
  ret.wav_->AddDimension(k_b);
  ret.wav_->AddEquality(k_, k_b);
  ret.wav_->RemoveDimension(k_);
  ret.k_ = k_b;
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::sign_extend(utils::Bitsize b) const {
  // Wrapping (as unsigned) needs to be performed before zero extension can be done
  WrappedDomain_Int ret = *this;
  ret.wrap(true/*is_signed*/);
  DimensionKey k_b = GetTempKey(b, 0);
  ret.wav_->AddDimension(k_b);
  ret.wav_->AddEquality(k_, k_b);
  ret.wav_->RemoveDimension(k_);
  ret.k_ = k_b;
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::zero_extend_or_trunc(utils::Bitsize b) const {
  UWAssert::shouldNeverHappen();
  WrappedDomain_Int ret = *this;
  return ret;
}

WrappedDomain_Int WrappedDomain_Int::trunc(utils::Bitsize b) const {
  WrappedDomain_Int ret = *this;
  DimensionKey k_b = GetTempKey(b, 0);
  ret.wav_->AddDimension(k_b);
  ret.wav_->AddEquality(k_, k_b);
  ret.wav_->RemoveDimension(k_);
  ret.k_ = k_b;
  return ret;
}

ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::abs_equal(const WrappedDomain_Int & that) const {
  // Calls to copy and wrap as wrap modifies the object (and loses precision), 
  // we want to keep this and that const and intact
  // Need to wrap both operation to either signed or unsigned. We choose signed as a matter of convention
  // TODO: Capture if either of this or that is wrapped and pick a sign according to that
  WrappedDomain_Int this_wrapped = *this; this_wrapped.wrap(true/*is_signed*/);
  WrappedDomain_Int that_wrapped = that; that_wrapped.wrap(true/*is_signed*/);

  // Essentially saying this_wrapped=that_wrapped as the key k_ in both this_wrapped and that_wrapped are equal
  this_wrapped.meet(that_wrapped);

  this_wrapped.wav_->RemoveDimension(k_);
  return this_wrapped.wav_;
}

/* abs_not_equal:
 *  1. Wrap both this and that into this_wrapped and that_wrapped respectively.
 *  2. Change the key k_ in this_wrapped.av_ value to temp1 and add new key k_
 *  3. Add the temp1 key to that_wrapped.av_
 *  4. Meet the two values in m1 and make a copy in m2
 *  5. Add constraint (k_ >= temp1 + 1) in m1, and (k_ <= temp1 - 1) in m2
 *  6. Join m1 and m2
 *  7. Project away k_ and temp1
 */
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::abs_not_equal(const WrappedDomain_Int & that) const {
  utils::Bitsize b = k_.GetBitsize();

  // Create temp1
  DimensionKey temp1 = GetTempKey(b, 1);
  Vocabulary voc_k; voc_k.insert(k_);

  // 1. Wrap both this and that into this_wrapped and that_wrapped respectively.
  // Need to wrap both operation to either signed or unsigned. We choose signed as a matter of convention
  WrappedDomain_Int this_wrapped = *this; this_wrapped.wrap(true/*is_signed*/);
  WrappedDomain_Int that_wrapped = that; that_wrapped.wrap(true/*is_signed*/);
 
  // 2. Change the key k_ in this_wrapped.av_ value to temp1 and add new key k_

  // Add constraint 1*k_ = 1*temp1 and then havoc k_, so that essentially the key k_ is replaced with temp1
  this_wrapped.wav_->AddDimension(temp1);

  AbstractValue::linexp_type le_lhs1, le_rhs1;
  le_lhs1.insert(std::make_pair(k_, MPZ_ONE));
  le_rhs1.insert(std::make_pair(temp1, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs1(le_lhs1, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs1(le_rhs1, MPZ_ZERO);
  this_wrapped.wav_->AddConstraint(ae_lhs1, ae_rhs1, AbstractValue::EQ);

  this_wrapped.wav_ = dynamic_cast<BitpreciseWrappedAbstractValue*>(this_wrapped.wav_->Havoc(voc_k).get_ptr());

  // 3. Add the temp1 key to that_wrapped.av_
  that_wrapped.wav_->AddDimension(temp1);

  // 4. Meet the two values in m1 and make a copy in m2
  ref_ptr<BitpreciseWrappedAbstractValue> m1 = this_wrapped.wav_;
  m1->Meet(that_wrapped.wav_.get_ptr());
  ref_ptr<BitpreciseWrappedAbstractValue> m2 = dynamic_cast<BitpreciseWrappedAbstractValue*>(m1->Copy().get_ptr());

  // 5. Add constraints (k_ >= temp1 + 1) in m1, and (k_ <= temp1 - 1) in m2
  AbstractValue::linexp_type le1;
  le1.insert(std::make_pair(k_, MPZ_ONE));
  le1.insert(std::make_pair(temp1, MPZ_MINUS_ONE));
  AbstractValue::affexp_type ae1(le1, MPZ_MINUS_ONE);
  m1->AddConstraintNorhs(ae1, AbstractValue::GE);

  AbstractValue::linexp_type le2;
  le2.insert(std::make_pair(k_, MPZ_ONE));
  le2.insert(std::make_pair(temp1, MPZ_MINUS_ONE));
  AbstractValue::affexp_type ae2(le2, MPZ_ONE);
  m2->AddConstraintNorhs(ae2, AbstractValue::LE);

  // 6. Join m1 and m2
  ref_ptr<BitpreciseWrappedAbstractValue> j = m1;
  j->Join(m2);

  // 7. Project away k_ and temp1
  Vocabulary voc_k_temp1; voc_k_temp1.insert(k_); voc_k_temp1.insert(temp1);
  j->RemoveVocabulary(voc_k_temp1);
  return j;
}

/* less_than_signed: 
 * 1) Wrap both this and that before adding less_than constraint.
 * 2) Add constraint by using this_wrapped_plus_1.less_than_equal_to_signed_no_wrapping(that_wrapped) operation
 * The call to no_wrapping function ensures that this_wrapped_plus_1 is not wrapped again.
 */
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::less_than_signed(const WrappedDomain_Int & that) const {
  // Calls to copy and wrap as wrap modifies the object (and loses precision), 
  // we want to keep this and that const and intact
  // Note that since this is signed less than, we need to make sure that wrap is called for signed numbers
  WrappedDomain_Int this_wrapped = *this; this_wrapped.wrap(true/*is_signed*/);
  WrappedDomain_Int that_wrapped = that; that_wrapped.wrap(true/*is_signed*/);

  // Note that this +1 can cause overflow, but we should not call wrap again as it will lead to
  // incorrect less_than_signed (for example, when this_wrapped is MAX_INT, less_than_signed should return 
  // ABSTRACT_FALSE (ie BOTTOM). But a call to wrap on this_wrapped_plus_1 will set to MIN_INT, thus giving
  // not ABSTRACT_FALSE with less_than_or_equal_signed_no_wrapping operation. 
  // That would not be just imprecise, but unsound as well. 
  WrappedDomain_Int this_wrapped_plus_1 = this_wrapped.plus(MPZ_ONE);
  ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> ret = this_wrapped_plus_1.less_than_or_equal_no_wrapping(that_wrapped, true/*is_signed*/);
  return ret;
}

/* less_than_unsigned: same as (this->plus(1)).less_than_equal_to_unsigned(that) operation.
 */
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::less_than_unsigned(const WrappedDomain_Int & that) const {
  // Calls to copy and wrap as wrap modifies the object (and loses precision), 
  // we want to keep this and that const and intact
  // Note that since this is unsigned less than, we need to make sure that wrap is called for signed numbers
  WrappedDomain_Int this_wrapped = *this; this_wrapped.wrap(false/*is_signed*/);
  WrappedDomain_Int that_wrapped = that; that_wrapped.wrap(false/*is_signed*/);

  // Note that this +1 can cause overflow, but we should not call wrap again as it will lead to
  // incorrect less_than_signed (for example, when this_wrapped is MAX_INT, less_than_signed should return 
  // ABSTRACT_FALSE (ie BOTTOM). But a call to wrap on this_wrapped_plus_1 will set to MIN_INT, thus giving
  // not ABSTRACT_FALSE with less_than_or_equal_signed_no_wrapping operation. 
  // That would not be just imprecise, but unsound as well. 
  WrappedDomain_Int this_wrapped_plus_1 = this_wrapped.plus(MPZ_ONE);

  return this_wrapped_plus_1.less_than_or_equal_no_wrapping(that_wrapped, false/*is_signed*/);
}


/* less_than_or_equal_signed: 
 * 1) Wrap both this and that before adding less_than constraint.
 * 2) Add constraint by using this_wrapped.less_than_equal_to_signed_no_wrapping(that_wrapped) operation
 */
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::less_than_or_equal_signed(const WrappedDomain_Int & that) const {
  // Calls to copy and wrap as wrap modifies the object (and loses precision), 
  // we want to keep this and that const and intact
  // Note that since this is signed less than, we need to make sure that wrap is called for signed numbers
  WrappedDomain_Int this_wrapped = *this; this_wrapped.wrap(true/*is_signed*/);
  WrappedDomain_Int that_wrapped = that; that_wrapped.wrap(true/*is_signed*/);

  ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> ret = this_wrapped.less_than_or_equal_no_wrapping(that_wrapped, true/*is_signed*/);
  return ret;
}

/* less_than_or_equal_unsigned: 
 * 1) Wrap both this and that before adding less_than constraint.
 * 2) Add constraint by using this_wrapped.less_than_equal_to_unsigned_no_wrapping(that_wrapped) operation
 */
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::less_than_or_equal_unsigned(const WrappedDomain_Int & that) const {
  // Calls to copy and wrap as wrap modifies the object (and loses precision), 
  // we want to keep this and that const and intact
  WrappedDomain_Int this_wrapped = *this; this_wrapped.wrap(false/*is_signed*/);
  WrappedDomain_Int that_wrapped = that; that_wrapped.wrap(false/*is_signed*/);
  return this_wrapped.less_than_or_equal_no_wrapping(that_wrapped, false/*is_signed*/);
}

/* less_than_or_equal_no_wrapping:
 *  1. Change the key k_ in this->wav_ value to temp1 and add new key k_
 *  2. Add the temp1 key to that.wav_
 *  3. Meet the two values in m
 *  4. Add constraint (temp1_ <= k_) in m 
 *  5. Project away k_ and temp1
 * 
 *  Note that this wrapping works for both signed and unsigned numbers, as it simply adds an inequality
 *  It's upto the client of this function to ensure that both numbers are in signed or unsigned range by
 *  calling appropriate wrapping function
 */
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::less_than_or_equal_no_wrapping(WrappedDomain_Int & that, bool is_signed) {
  // Create temp1
  utils::Bitsize b = k_.GetBitsize();
  DimensionKey temp1 = GetTempKey(b, 1);

  Vocabulary voc_k; voc_k.insert(k_);

  // 1. Change the key k_ in this->wav_ value to temp1 and add new key k_

  // Add constraint 1*k_ = 1*temp1 and then havoc k_, so that essentially the key k_ is replaced with temp1
  wav_->AddDimension(temp1); // No wrapping as k_ might not be wrapped
  AbstractValue::linexp_type le_lhs1, le_rhs1;
  le_lhs1.insert(std::make_pair(k_, MPZ_ONE));
  le_rhs1.insert(std::make_pair(temp1, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs1(le_lhs1, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs1(le_rhs1, MPZ_ZERO);
  wav_->AddConstraint(ae_lhs1, ae_rhs1, AbstractValue::EQ);
  wav_ = dynamic_cast<BitpreciseWrappedAbstractValue*>(wav_->Havoc(voc_k).get_ptr());

  // 2. Add the temp1 key to that.wav_
  that.wav_->AddDimension(temp1);

  // 3. Meet the two values in m
  ref_ptr<BitpreciseWrappedAbstractValue> m = wav_; // No need to copy wav_ as this function is non-const
  m->Meet(that.wav_.get_ptr());

  // 4. Add constraint (temp1_ <= k_) in m
  AbstractValue::linexp_type le1;
  le1.insert(std::make_pair(temp1, MPZ_ONE));
  AbstractValue::linexp_type le2;
  le2.insert(std::make_pair(k_, MPZ_ONE));
  AbstractValue::affexp_type ae1(le1, MPZ_ZERO);
  AbstractValue::affexp_type ae2(le2, MPZ_ZERO);
  m->AddConstraint(ae1, ae2, AbstractValue::LE);

  // 5. Project away k_ and temp1
  Vocabulary voc_k_temp1; voc_k_temp1.insert(k_); voc_k_temp1.insert(temp1);
  m->RemoveVocabulary(voc_k_temp1);

  return m;
}

// Boolean and is simply a meet operation
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::boolean_and(const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& b1, const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& b2) {
  ref_ptr<BitpreciseWrappedAbstractValue> ret = dynamic_cast<BitpreciseWrappedAbstractValue*>(b1->Copy().get_ptr());
  ret->Meet(b2.get_ptr());
  return ret;
}

// Boolean or is simply a join operation
ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::boolean_or(const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& b1, const ref_ptr<BitpreciseWrappedAbstractValue>& b2) {
  ref_ptr<BitpreciseWrappedAbstractValue> ret = dynamic_cast<BitpreciseWrappedAbstractValue*>(b1->Copy().get_ptr());
  ret->Join(b2.get_ptr());
  return ret;
}

ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::boolean_xor(const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& b1, const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& b2) {
  ref_ptr<BitpreciseWrappedAbstractValue> not_b1 = boolean_not(b1);
  ref_ptr<BitpreciseWrappedAbstractValue> not_b2 = boolean_not(b2);
  ref_ptr<BitpreciseWrappedAbstractValue> not_b1_and_b2 = boolean_and(not_b1, b2);
  ref_ptr<BitpreciseWrappedAbstractValue> b1_and_not_b2 = boolean_and(b1, not_b2);
  return boolean_or(not_b1_and_b2, b1_and_not_b2);
}

ref_ptr<abstract_value::BitpreciseWrappedAbstractValue> WrappedDomain_Int::boolean_not(const ref_ptr<abstract_value::BitpreciseWrappedAbstractValue>& b1) {
  // Cannot express complement operation
  return dynamic_cast<abstract_value::BitpreciseWrappedAbstractValue*>(b1->Top().get_ptr()); 
}

/* The algorithm for lin_combine:
 *  1. Change the key k_ in left value to temp1 and add new keys k_ and temp2
 *  2. Change the key k_ in right value to temp2 and add new keys k_ and temp1
 *  3. Meet the two values
 *  4. Add a constraint (new_coeff * k_ + this_coeff * temp1 + that_coeff * temp2 + constant = 0)
 *  5. Project away the temp1 and temp2
 */
WrappedDomain_Int WrappedDomain_Int::lin_combine (const WrappedDomain_Int& that, 
                                                  mpz_class new_coeff, mpz_class this_coeff,
                                                  mpz_class that_coeff, mpz_class constant) const {
  // Create temp1 and temp2
  utils::Bitsize b = k_.GetBitsize();
  DimensionKey temp1 = GetTempKey(b, 1);
  DimensionKey temp2 = GetTempKey(b, 2);

  Vocabulary voc_k; voc_k.insert(k_);
  Vocabulary voc_temp1_2; voc_temp1_2.insert(temp1); voc_temp1_2.insert(temp2);

  // 1. Change the key k_ in left value to temp1 and add new keys k_ and temp2
  // Create a copy of this_av as we will be modifying it
  ref_ptr<BitpreciseWrappedAbstractValue> this_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(wav_->Copy().get_ptr());
  this_av->AddVocabulary(voc_temp1_2);

  // Add constraint 1*k_ = 1*temp1 and then havoc k_, so that essentially the key k_ is replaced with temp1
  AbstractValue::linexp_type le_lhs1, le_rhs1;
  le_lhs1.insert(std::make_pair(k_, MPZ_ONE));
  le_rhs1.insert(std::make_pair(temp1, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs1(le_lhs1, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs1(le_rhs1, MPZ_ZERO);
  this_av->AddConstraint(ae_lhs1, ae_rhs1, AbstractValue::EQ);
  this_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(this_av->Havoc(voc_k).get_ptr());

  // 2. Change the key k_ in right value to temp2 and add new keys k_ and temp1
  // Create a copy of that_av as we will be modifying it
  ref_ptr<BitpreciseWrappedAbstractValue> that_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(that.wav_->Copy().get_ptr());
  that_av->AddVocabulary(voc_temp1_2);

  // Add constraint 1*k_ = 1*temp2 and then havoc k_, so that essentially the key k_ is replaced with temp2
  AbstractValue::linexp_type le_lhs2, le_rhs2;
  le_lhs2.insert(std::make_pair(k_, MPZ_ONE));
  le_rhs2.insert(std::make_pair(temp2, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs2(le_lhs2, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs2(le_rhs2, MPZ_ZERO);
  that_av->AddConstraint(ae_lhs2, ae_rhs2, AbstractValue::EQ);
  that_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(that_av->Havoc(voc_k).get_ptr());

  // 3. Meet the two values
  ref_ptr<BitpreciseWrappedAbstractValue> ret_av = this_av;
  ret_av->Meet(that_av.get_ptr());

  // 4. Add constraint (new_coeff * k_ + this_coeff * temp1 + that_coeff * temp2 + constant = 0)
  AbstractValue::linexp_type le_lincomb;
  le_lincomb.insert(std::make_pair(k_, new_coeff));
  le_lincomb.insert(std::make_pair(temp1, this_coeff));
  le_lincomb.insert(std::make_pair(temp2, that_coeff));
  AbstractValue::affexp_type ae_lincomb(le_lincomb, constant);
  ret_av->AddConstraintNorhs(ae_lincomb, AbstractValue::EQ);

  // 5. Project away the temp1 and temp2
  ret_av->RemoveVocabulary(voc_temp1_2);

  WrappedDomain_Int ret(ret_av, k_);
  return ret;
}

// Like lin_combine, but just
// impose the condition (new_coeff * new_k' + old_coeff * old_k + constant == 0),
// without using some other WrappedDomain_Int, and then project away i, leaving i'.
WrappedDomain_Int WrappedDomain_Int::lin_transform (mpz_class new_coeff, mpz_class old_coeff, mpz_class constant) const {
  // Create old_k
  utils::Bitsize b = k_.GetBitsize();
  DimensionKey old_k = GetTempKey(b, 1);

  Vocabulary voc_k; voc_k.insert(k_);

  // 1. Change the key k_ in this to old_k
  // Create a copy of this_av as we will be modifying it
  ref_ptr<BitpreciseWrappedAbstractValue> this_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(wav_->Copy().get_ptr());
  this_av->AddDimension(old_k);

  // Add constraint 1*k_ = 1*old_k and then havoc k_, so that essentially the key k_ is replaced with old_k
  AbstractValue::linexp_type le_lhs1, le_rhs1;
  le_lhs1.insert(std::make_pair(k_, MPZ_ONE));
  le_rhs1.insert(std::make_pair(old_k, MPZ_ONE));
  AbstractValue::affexp_type ae_lhs1(le_lhs1, MPZ_ZERO);
  AbstractValue::affexp_type ae_rhs1(le_rhs1, MPZ_ZERO);
  this_av->AddConstraint(ae_lhs1, ae_rhs1, AbstractValue::EQ);
  this_av = dynamic_cast<BitpreciseWrappedAbstractValue*>(this_av->Havoc(voc_k).get_ptr());

  // 2. Add constraint (new_coeff * new_k' + old_coeff * old_k + constant == 0)
  AbstractValue::linexp_type le_lincomb;
  le_lincomb.insert(std::make_pair(k_, new_coeff));
  le_lincomb.insert(std::make_pair(old_k, old_coeff));
  AbstractValue::affexp_type ae_lincomb(le_lincomb, constant);
  this_av->AddConstraintNorhs(ae_lincomb, AbstractValue::EQ);

  // 3. Project away old_k
  this_av->RemoveDimension(old_k);
  return WrappedDomain_Int(this_av, k_);
}

void WrappedDomain_Int::UpdateIntSignedness() {
  Vocabulary voc_k; voc_k.insert(k_);
  wav_->UpdateVocabularySignedness(voc_k);
}

// ======
// Output
// ======
std::ostream & WrappedDomain_Int::print(std::ostream & out) const {
  wav_->print(out << "WrappedDomain_Int:");
  return out;
}

std::string WrappedDomain_Int::str() const {
  std::string ret = std::string("WrappedDomain_Int:") + wav_->ToString();
  return ret;
}

#undef MPZ_MINUS_ONE
#undef MPZ_ONE
#undef MPZ_ZERO
