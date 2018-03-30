#include "AvSemiring.hpp"
#include "utils/timer/timer.hpp"

AvSemiringStats AvSemiring::av_semiring_stats_;

AvSemiring::AvSemiring(ref_ptr<AbstractValue> av, std::string from, std::string to, WideningType t, bool is_one)
  : av_(av), from_ (from), to_(to), t_ (t), is_one_(is_one)  { 
  assert(av_.get_ptr() != NULL);
}

AvSemiring::AvSemiring(const AvSemiring& a) : av_(a.av_->Copy()), from_(a.from_), to_(a.to_), t_(a.t_), is_one_(a.is_one_) {
}

AvSemiring::~AvSemiring() {
}

// Widening rules for combine: Neither of the parameterms can be widening rule
//         +              | Regular              | Widening Wight       
//        Regular         | Regular              | Regular (Call widen)
//        Widening Weight | Regular (call widen) | Regular (call widen)
sem_elem_t AvSemiring::combine(SemElem * op2_sem) {
  utils::Timer timer("\nCombineTimer", std::cout, true);
  if(op2_sem == this)
    return this;

  AvSemiring* op2 = dynamic_cast<AvSemiring*>(op2_sem);
  assert((t_ != WIDENING_RULE) && (op2->t_ != WIDENING_RULE));

  DEBUG_PRINTING(DBG_PRINT_OPERATIONS,
                 std::cout << "\n\nCombine called:" << std::endl;
                 print(std::cout << "this:") << std::endl;
                 op2->print(std::cout << "op2:") << std::endl;);

  ref_ptr<AvSemiring> result = new AvSemiring(*this);
  if(result->from_.size() == 0)
    result->from_ = op2->from_;
  if(result->to_.size() == 0)
    result->to_ = op2->to_;


  // Check for bottom
  if(av_->IsBottom())
    return op2_sem;
  if(op2->av_->IsBottom()) 
    return this;
 
  DEBUG_PRINTING(DBG_PRINT_OPERATIONS, std::cout << "\nPerforming Join.";);

  // Handle the cases where the vocabulary of the two values can be different
  ref_ptr<AvSemiring> op2_cp = op2;
  result->av_->AddVocabulary(op2_cp->av_->GetVocabulary());
  if(result->av_->GetVocabulary() != op2_cp->av_->GetVocabulary()) {
    op2_cp = new AvSemiring(*op2_cp);
    op2_cp->av_->AddVocabulary(result->av_->GetVocabulary());
  }


  // Check for one
  if(result->is_one_)
    result->SetToSyntacticOne();
  if(op2_cp->is_one_) {
    // Since SetToSyntacticOne changes the structure ensure that it is done on a copy of op2
    if(op2_cp == op2)
      op2_cp = new AvSemiring(*op2_cp);
    op2_cp->SetToSyntacticOne();
  }

  result->av_->Join(op2_cp->av_);
  av_semiring_stats_.num_join_calls_++;
  av_semiring_stats_.time_join_+=timer.elapsed();

  if((t_ == WIDENING_WEIGHT) || (op2->t_ == WIDENING_WEIGHT)) {
    // If either is widening weight then combine is not enough
    // Widen needs to be called as well
    DEBUG_PRINTING(DBG_PRINT_OPERATIONS, std::cout << "\nPerforming Widen.";);
    result->av_->Widen(this->av_);
    av_semiring_stats_.num_widen_calls_++;
    av_semiring_stats_.time_widen_+=timer.elapsed();
  }

  DEBUG_PRINTING(DBG_PRINT_OPERATIONS, result->print(std::cout << "\nresult:") << std::endl;);
  return result.get_ptr();
}

//  Widening rules for extend: The first argument cannot be a widening edge and the second argument cannot be widening weight
//         *              | Regular         | Widening Rule
//        Regular         | Regular         | Widening Weight
//        Widening Weight | Widening Weight | Widening Weight
sem_elem_t AvSemiring::extend(SemElem * op2_se) {
  utils::Timer timer("\nExtendTimer", std::cout, true);

  DEBUG_PRINTING(DBG_PRINT_OPERATIONS, 
                 std::cout << "\n\nExtend called:" << std::endl;
                 print(std::cout << "this:") << std::endl;
                 op2_se->print(std::cout << "op2_se:") << std::endl;);

  av_semiring_stats_.num_extend_calls_++;

  const AvSemiring* op2 = dynamic_cast<AvSemiring*>(op2_se);

  // Check for bottom
  if(av_->IsBottom() || op2->av_->IsBottom()) 
    return zero();

  // Check for special value of one
  if(is_one_)
    return op2_se;
  if(op2->is_one_)
    return this;

  const ref_ptr<AbstractValue> op2_av =  op2->GetAbstractValue();
  const ref_ptr<AbstractValue> this_av = this->GetAbstractValue();

  ref_ptr<AbstractValue> result_av;
  if(GetAbstractValue()->ImplementsExtend()) {
    result_av = this_av->Extend(op2_av);
  } else {
    std::map<Version, Version> map_0_to_1__1_to_2;
    map_0_to_1__1_to_2.insert(std::pair<Version, Version>(0, 1));
    map_0_to_1__1_to_2.insert(std::pair<Version, Version>(1, 2));

    ref_ptr<AbstractValue> _1_2_op2_av = op2_av->Copy();
    _1_2_op2_av->ReplaceVersions(map_0_to_1__1_to_2);

    // Extend this_av and _1_2_other_av to 3-voc
    ref_ptr<AbstractValue> _3_voc_this_av = this_av->Copy();

    // Perform meet of values
    _3_voc_this_av->Meet(_1_2_op2_av);    

    // Explicit call to Reduce.
    // Note Meet does not call Reduce.
    _3_voc_this_av->Reduce();

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
    Vocabulary v1 = getVocabularySubset(_3_voc, 1);

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, 
                   std::cout << "\n_3_voc:" << _3_voc << "\nvoc_to_prj:" << voc_to_prj;);

    _3_voc_this_av->Project(voc_to_prj);
    
    // Rename version 2 to 1
    _3_voc_this_av->ReplaceVersion(Version(2), Version(1));
    result_av = _3_voc_this_av;
  }

  assert((t_ != WIDENING_RULE) && (op2->t_ != WIDENING_WEIGHT));
  WideningType ret_t = WIDENING_WEIGHT;
  if(t_ == REGULAR_WEIGHT && op2->t_ == REGULAR_WEIGHT)
    ret_t = REGULAR_WEIGHT;

  sem_elem_t result = new AvSemiring(result_av, from_, op2->to_, ret_t);

  DEBUG_PRINTING(DBG_PRINT_OPERATIONS, result->print(std::cout << "extend result:") << std::endl;);

  av_semiring_stats_.time_extend_+=timer.elapsed();
  return result;
}

// Default implementation just returns one
sem_elem_t AvSemiring::quasi_one() const {
  return one();
}

bool AvSemiring::isEqual(SemElem* op2) const {
  utils::Timer timer("\nisEqualTimer", std::cout, true);
  DEBUG_PRINTING(DBG_PRINT_OPERATIONS, 
                 std::cout << "\nEquality called:" << std::endl;
                 print(std::cout << "this:") << std::endl;
                 op2->print(std::cout << "op2:") << std::endl;);

  av_semiring_stats_.num_equal_calls_++;

  if(this == op2) {
    DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                   std::cout << "\nPointers are same, returning true.";
                   std::cout << "\nequality result:1\n";);
    return true;
  }
  const AvSemiring* op2_av = dynamic_cast<const AvSemiring*>(op2);

  // Instilled in place so that FWPDS doesn' remove widening rules that happen to be identity
  if(t_ != op2_av->t_) {
    DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                   std::cout << "\nWeight types are different, returning false.";
                   std::cout << "\nequality result:0\n";);
    return false;
  }

  // Handle case where either of the value is one
  if(is_one_ && op2_av->is_one_)
    return true;
  if(is_one_ && !op2_av->is_one_)
    return false;
  if(!is_one_ && op2_av->is_one_)
    return false;

  bool result = (av_->Equal(op2_av->av_));
  DEBUG_PRINTING(DBG_PRINT_OPERATIONS, std::cout << "\nequality result:" << result << "\n";);

  av_semiring_stats_.time_equal_+=timer.elapsed();
  return result;
}

bool AvSemiring::equal(SemElem* a) const {
  return this->isEqual(a);
}

bool AvSemiring::equal(sem_elem_t a) const {
  return this->isEqual(a.get_ptr());
}

const ref_ptr<AvSemiring::AbstractValue> AvSemiring::GetAbstractValue() const {
  return av_;
}

ref_ptr<AvSemiring::AbstractValue> AvSemiring::GetAbstractValue() {
  return av_;
}

void AvSemiring::prettyPrint(FILE *fp, unsigned nTabs) {
  av_->prettyPrint(fp, nTabs);
  return;
}

std::ostream & AvSemiring::print(std::ostream &out) const {
  out << "from:" << from_ << ", to:" << to_;
  out << ", type:";
  switch(t_) {
  case REGULAR_WEIGHT: out << "regular weight"; break;
  case WIDENING_WEIGHT: out << "widening weight"; break;
  case WIDENING_RULE: out << "widening rule"; break;
  }
  out << std::endl;
  if(is_one_)
    out << "ONE";
  else
    av_->print(out);
  return out;
}

void AvSemiring::setFrom(std::string from) {
  from_ = from;
}

void AvSemiring::setTo(std::string to) {
  to_ = to;
}

void AvSemiring::setWideningType(WideningType t) {
  t_ = t;
}

//Ensure that this function is called only once
sem_elem_t AvSemiring::one() const {
  // Set is_one_ to true
  ref_ptr<AvSemiring> result = new AvSemiring(av_->Top(), std::string(""), std::string(""), REGULAR_WEIGHT, true);
  return result.get_ptr();
}

 // Set the abstract value to one
 void AvSemiring::SetToSyntacticOne() {
   assert(is_one_);
   is_one_ = false;
   av_ = av_->Top();
  Vocabulary voc = av_->GetVocabulary();

  //For each pair of vocabulary see if
  //their terms match when version 1 is
  //is replaced with 0 and if yes then 
  //add equality operation.
  for(std::set<DimensionKey>::const_iterator it1 = voc.begin(); it1 != voc.end(); it1++)  {
    for(std::set<DimensionKey>::const_iterator it2 = voc.begin(); it2 != voc.end(); it2++) {
      if(*it1 != *it2) {
        DimensionKey k1 = *it1;
        DimensionKey k2 = *it2;

        bool isVersion0Ink1 = isVersionInDimension(k1, 0);
        bool isVersion1Ink1 = isVersionInDimension(k1, 1);
        bool isVersion0Ink2 = isVersionInDimension(k2, 0);
        bool isVersion1Ink2 = isVersionInDimension(k2, 1);

        if(isVersion0Ink1 && !isVersion1Ink1 && !isVersion0Ink2 && isVersion1Ink2) {
          DimensionKey k1_rep = replaceVersion(k1,0,1);
          DimensionKey k2_rep = replaceVersion(k2,0,1);
          if(k1_rep == k2_rep) {
            av_->AddEquality(k1, k2);
          }
        }
      }
    }
  }
  DEBUG_PRINTING(DBG_PRINT_DETAILS, print(std::cout << "\nThis is syntactic one for the given voc:"););
}

//Ensure that this function is called only once
sem_elem_t AvSemiring::zero() const {
  return new AvSemiring(av_->Bottom());
}

//Ensure that this function is called only once
sem_elem_t AvSemiring::bottom() const {
  //Semiring bottom means top of abstract value
  return new AvSemiring(av_->Top());
}
