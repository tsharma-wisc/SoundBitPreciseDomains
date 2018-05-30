#ifndef src_AbstractDomain_PointsetPowerset_pointset_powerset_av_impl_hpp
#define src_AbstractDomain_PointsetPowerset_pointset_powerset_av_impl_hpp

#include "pointset_powerset_av.hpp"
#include "pointset_powerset_wrap_assign.h"
#include "utils/timer/timer.hpp"
#include <algorithm>
#include <sstream>
#include <memory>

// By default, max_disjunctions is 1, hence this class behaves like normal polyhedron
template <typename PSET>
unsigned abstract_domain::PointsetPowersetAv<PSET>::max_disjunctions = 1;

// By default, extrapolation is disabled
template <typename PSET>
unsigned abstract_domain::PointsetPowersetAv<PSET>::use_extrapolation = false;

namespace abstract_domain
{
  // Constructors
  // ============
  template <typename PSET>
  PointsetPowersetAv<PSET>::PointsetPowersetAv(const Vocabulary& voc, bool is_universe, bool equalities_only)
    : AbstractValue(voc), pp_(voc.size(), (is_universe? Parma_Polyhedra_Library::UNIVERSE : Parma_Polyhedra_Library::EMPTY)), equalities_only_(equalities_only) {
    key_index_bimap_ = GetVocabularyIndexMap(voc);
  }

  template <typename PSET>
  PointsetPowersetAv<PSET>::PointsetPowersetAv(const PointsetPowersetAv& orig)
    : AbstractValue(orig.voc_), pp_(orig.pp_), key_index_bimap_(orig.key_index_bimap_), equalities_only_(orig.equalities_only_) {
  } 

  template <typename PSET>
  PointsetPowersetAv<PSET>::~PointsetPowersetAv() {
  }

  template <typename PSET>
  PointsetPowersetAv<PSET> & PointsetPowersetAv<PSET>::operator=(const PointsetPowersetAv& a) { 
    if(this == &a)
      return *this;

    this->voc_ = a.voc_;
    this->pp_ = a.pp_;
    this->key_index_bimap_ = a.key_index_bimap_;
    this->equalities_only_ = this->equalities_only_;
    return *this; 
  }

  template <typename PSET>
  ref_ptr<AbstractValue> PointsetPowersetAv<PSET>::Copy() const {
    ref_ptr<PointsetPowersetAv> ret = new PointsetPowersetAv(*this);
    return ret.get_ptr();
  }

  template <typename PSET>
  bool PointsetPowersetAv<PSET>::IsDistributive() const {
    return false;
  }

  template <typename PSET>
  ref_ptr<AbstractValue> PointsetPowersetAv<PSET>::Top() const {
    return new PointsetPowersetAv(this->voc_, true, equalities_only_);
  }

  template <typename PSET>
  ref_ptr<AbstractValue> PointsetPowersetAv<PSET>::Bottom() const {
    return new PointsetPowersetAv(this->voc_, false, equalities_only_);
  }
        
  // Equality operations
  template <typename PSET>
  inline bool PointsetPowersetAv<PSET>::operator==  (const AbstractValue& that) const {
    const PointsetPowersetAv* that_p = static_cast<const PointsetPowersetAv*>(&that);
    assert(equalities_only_ == that_p->equalities_only_);
    return (*this == *that_p);
  }

  // Heterogeneous equality
  template <typename PSET>
  inline bool PointsetPowersetAv<PSET>::operator== (const PointsetPowersetAv&that) const {
    return (pp_ == that.pp_);
  }

  template <typename PSET>
  inline bool PointsetPowersetAv<PSET>::Overapproximates(const ref_ptr<AbstractValue>& that) const {
    const ref_ptr<PointsetPowersetAv> that_pp = static_cast<PointsetPowersetAv *>(that.get_ptr());
    assert(equalities_only_ == that_pp->equalities_only_);
    return pp_.contains(that_pp->pp_);
  }

  // Abstract Domain operations
  template <typename PSET>
  bool PointsetPowersetAv<PSET>::IsBottom() const {
    return (pp_.is_bottom());
  }

  template <typename PSET>
  bool PointsetPowersetAv<PSET>::IsTop() const {
    return (pp_.is_top());
  }

  // Since poly domain can be infinite this operation does not make sense. Hence the assert.
  template <typename PSET>
  unsigned PointsetPowersetAv<PSET>::GetHeight () const {
    assert(false);
    return 0;
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::Join (const ref_ptr<AbstractValue> & that) {
    ref_ptr<PointsetPowersetAv> that_pp = static_cast<PointsetPowersetAv *>(that.get_ptr());
    assert(equalities_only_ == that_pp->equalities_only_);
    raw_join(that_pp);
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::raw_join (const ref_ptr<PointsetPowersetAv> & that) {
    utils::Timer timer("raw_joim", std::cout, false);

    AddVocabulary(that->GetVocabulary());

    if(GetVocabulary() == that->GetVocabulary()) {
      pp_.upper_bound_assign(that->pp_);
    } else {
      ref_ptr<PointsetPowersetAv> that_pp = new PointsetPowersetAv(*that);
      that_pp->AddVocabulary(this->GetVocabulary());
      pp_.upper_bound_assign(that_pp->pp_);
    }

    // Merge so that the size of pp_ does not exceed max_disjunctions
    MergeHeuristic();
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nPointsetPowersetAv raw_join time:" << timer.elapsed() 
                   << ", voc_size:" << this->voc_.size(););
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::Meet(const ref_ptr<AbstractValue> & that) {
    utils::Timer timer("PointsetPowersetAv meet", std::cout, false);
    ref_ptr<PointsetPowersetAv> that_pp = static_cast<PointsetPowersetAv *>(that.get_ptr());
    assert(equalities_only_ == that_pp->equalities_only_);

    AddVocabulary(that->GetVocabulary());

    if(GetVocabulary() != that->GetVocabulary()) {
      that_pp = new PointsetPowersetAv(*that_pp);
      that_pp->AddVocabulary(this->GetVocabulary());
    }

    if(IsTop()) {
      *this = *that_pp;
      return;
    } else if(that_pp->IsTop()) {
      return;
    }

    DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS,
                   print(std::cout << "\nIn pointsetpowersetav meet, this:");
                   that->print(std::cout << "\nthat:"););

    pp_.meet_assign(that_pp->pp_);

    DEBUG_PRINTING(DBG_PRINT_MORE_DETAILS, print(std::cout << "\nResult meet:"););

    // Merge so that the size of pp_ does not exceed max_disjunctions
    MergeHeuristic();
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nPointsetPowersetAv meet time:" << timer.elapsed() 
                   << ", voc_size:" << this->voc_.size(););
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::Widen (const ref_ptr<AbstractValue> & that_av) {
    AddVocabulary(that_av->GetVocabulary());

    if(GetVocabulary() == that_av->GetVocabulary()) {
      WidenHomogeneous(that_av);
    } else {
      ref_ptr<AbstractValue> that_cp = that_av->Copy();
      that_cp->AddVocabulary(GetVocabulary());
      WidenHomogeneous(that_cp);
    }
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::WidenHomogeneous (const ref_ptr<AbstractValue> & that_av) {
   assert(false);
    // typedef Parma_Polyhedra_Library::BHRZ03_Certificate T1;
    // typedef typename Parma_Polyhedra_Library::Widening_Function<PSET> T2;
    // ref_ptr<PointsetPowersetAv> that = static_cast<PointsetPowersetAv *>(that_av.get_ptr());
    // T1 cert;
    // T2 widen_fun = Parma_Polyhedra_Library::widen_fun_ref(&PSET::widening_assign);
    // pp_.BHZ03_widening_assign(that->pp_, widen_fun);

    // // Merge so that the size of pp_ does not exceed max_disjunctions
    // MergeHeuristic();
  }

  // Template specializations for WidenHomogeneous
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron>::WidenHomogeneous (const ref_ptr<AbstractValue> & that_av);
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::WidenHomogeneous (const ref_ptr<AbstractValue> & that_av);

  template <typename PSET>
  void PointsetPowersetAv<PSET>::Wrap(const VocabularySignedness& vs) {
    utils::Timer timer("\nwrap_timer:", std::cout, false);

    // Wrap operation is too costly for C_Polyhedron equality values. Ignore it for now.
    // This can potentially cause unsoundness but we can probably find a way around it
    // in reduce if it becomes a problem
    if(equalities_only_)
      return;

    // Capture all the signed and unsigned wrap vocabulary separately
    Vocabulary signed_voc_to_wrap, unsigned_voc_to_wrap;
    for(VocabularySignedness::const_iterator it = vs.begin(); it != vs.end(); it++) {
      if(it->second)
        signed_voc_to_wrap.insert(it->first);
      else
        unsigned_voc_to_wrap.insert(it->first);
    }

    Vocabulary signed_voc_to_wrap_intersect_voc, unsigned_voc_to_wrap_intersect_voc;
    IntersectVocabularies(signed_voc_to_wrap, this->voc_, signed_voc_to_wrap_intersect_voc);
    IntersectVocabularies(unsigned_voc_to_wrap, this->voc_, unsigned_voc_to_wrap_intersect_voc);

    Wrap(signed_voc_to_wrap_intersect_voc, true);
    Wrap(unsigned_voc_to_wrap_intersect_voc, false);
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nWrap time:" << timer.elapsed() << ", voc_size:" << this->voc_.size(););
  }

  /* wrap: Perform wrap operation as per "Taming the arithmetic.." paper
   * Parameters: 
   *   voc_to_wrap: Vocabulary in this AbstractValue which needs to be wrapped
   *   is_signed: Is the wrapped vocabulary treated as signed or not?
   *   cs: Possibly null pointer to a constraint system whose variables are contained in vars. If *cs_p depends on 
   *       variables not in vars, the behavior is undefined. When non-null, the pointed-to constraint system is 
   *       assumed to represent the conditional or looping construct guard with respect to which wrapping is 
   *       performed. Since wrapping requires the computation of upper bounds and due to non-distributivity of 
   *       constraint refinement over upper bounds, passing a constraint system in this way can be more precise than 
   *       refining the result of the wrapping operation with the constraints in *cs_p.
   */
  template <typename PSET>
  void PointsetPowersetAv<PSET>::Wrap(const Vocabulary& voc_to_wrap, bool is_signed) {
    if(voc_to_wrap.size() == 0)
      return;

    // For each Bitsize, perform the Wrap operation separately
    // Handle bool values separately
    Vocabulary v1 = abstract_domain::getVocabularySubset(voc_to_wrap, utils::one);
    // For now, ignore boolean vocabulary
    // WrapBoolVoc(v1);

    utils::Bitsize bitsize = utils::eight;
    Vocabulary v8 = abstract_domain::getVocabularySubset(voc_to_wrap, bitsize);
    Wrap(v8, is_signed, bitsize);

    bitsize = utils::sixteen;
    Vocabulary v16 = abstract_domain::getVocabularySubset(voc_to_wrap, bitsize);
    Wrap(v16, is_signed, bitsize);

    bitsize = utils::thirty_two;
    Vocabulary v32 = abstract_domain::getVocabularySubset(voc_to_wrap, bitsize);
    Wrap(v32, is_signed, bitsize);

    bitsize = utils::sixty_four;
    Vocabulary v64 = abstract_domain::getVocabularySubset(voc_to_wrap, bitsize);
    Wrap(v64, is_signed, bitsize);
  }

  /* wrap: Perform wrap operation as per "Taming the arithmetic.." paper
   * Parameters: 
   *   voc_to_wrap: Vocabulary in this AbstractValue which needs to be wrapped
   *   is_signed: Is the wrapped vocabulary treated as signed or not?
   *   cs: Possibly null pointer to a constraint system whose variables are contained in vars. 
   *       If *cs_p depends on variables not in vars, the behavior is undefined. When non-null,
   *       the pointed-to constraint system is assumed to represent the conditional or looping 
   *       construct guard with respect to which wrapping is performed. Since wrapping requires
   *       the computation of upper bounds and due to non-distributivity of 
   *       constraint refinement over upper bounds, passing a constraint system in this way 
   *       can be more precise than  refining the result of the wrapping operation with the 
   *       constraints in *cs_p.
   */
  template <typename PSET>
  void PointsetPowersetAv<PSET>::Wrap(const Vocabulary& voc_to_wrap, bool is_signed, utils::Bitsize bitsize) {
    if(voc_to_wrap.size() == 0)
      return;

    Parma_Polyhedra_Library::Variables_Set vars_to_wrap = GetVariableSetFromVocabulary(voc_to_wrap);
    Parma_Polyhedra_Library::Bounded_Integer_Type_Width width = GetPplBoundedIntegerWidth(bitsize);
    Parma_Polyhedra_Library::Bounded_Integer_Type_Representation rep = is_signed? Parma_Polyhedra_Library::SIGNED_2_COMPLEMENT : Parma_Polyhedra_Library::UNSIGNED;

    // By default, assume that overflow wraps
    // Look at this link for more details on wrap_assign: http://bugseng.com/products/ppl/documentation//devref/ppl-devref-1.1-html/classParma__Polyhedra__Library_1_1Polyhedron.html#a6b987d283ce345c52b5d193c0688ad28
    abstract_domain::wrap_assign_pointset_powerset(pp_, vars_to_wrap, width, rep, Parma_Polyhedra_Library::OVERFLOW_WRAPS, NULL, 16/*complexity_threshold*/, true/*wrap_individually*/);

    MergeHeuristic();
  }

  template <typename PSET>
  bool PointsetPowersetAv<PSET>::IsConstant(mpz_class& val) const {
    // Assume that the vocabulary size is 1
    assert(this->GetVocabulary().size() == 1);

    if(pp_.size() != 1)
      return false;

    DimensionKey k = *(this->GetVocabulary().begin());

    unsigned num_constraints = 0;
    const Parma_Polyhedra_Library::Constraint_System cs = pp_.begin()->pointset().constraints();
    for (Parma_Polyhedra_Library::Constraint_System::const_iterator it = cs.begin(); it != cs.end(); it++, num_constraints++) {
      const Parma_Polyhedra_Library::Constraint c = *it;
      Parma_Polyhedra_Library::Variable v_i(GetPplDimensionTypeFromDimensionKey(k));
      const Parma_Polyhedra_Library::GMP_Integer coeff = c.coefficient(v_i);
      
      // coeff_int can be non-equal to 1 if the value is non-integer or the constraint is unsatisfiable
      if(coeff != 1) {
        return false;
      }

      const Parma_Polyhedra_Library::GMP_Integer const_coeff = c.inhomogeneous_term();
      val = -const_coeff;
    }

    if(num_constraints == 1)
      return true;

    return false;
  }

  // Get the vocabulary which directly or indirectly depends on k
  template <typename PSET>
  Vocabulary PointsetPowersetAv<PSET>::GetDependentVocabulary(DimensionKey& k) const {
    Vocabulary v;

    ppl_dimension_type k_idx = GetPplDimensionTypeFromDimensionKey(k);
    for (typename Parma_Polyhedra_Library::Pointset_Powerset<PSET>::const_iterator it = pp_.begin(); it != pp_.end(); it++) {
      // Start with empty vocabulary and keep adding dimensions to it unless they can be added no more
      Vocabulary v_it;
      v_it.insert(k);
      Parma_Polyhedra_Library::Constraint_System cs = it->pointset().constraints();
      bool reached_fixpoint = false;
      do {
        Vocabulary new_v_it = v_it;
        for(Parma_Polyhedra_Library::Constraint_System::const_iterator cs_it = cs.begin(); cs_it != cs.end(); cs_it++) {
          Vocabulary non_zero_voc_cs_it;
          for(ppl_dimension_type i = cs_it->space_dimension(); i-- > 0; ) {
            Parma_Polyhedra_Library::Variable v_i(i);
            const Parma_Polyhedra_Library::GMP_Integer coeff = cs_it->coefficient(v_i);
            if(coeff != 0) {
              DimensionKey k_i = GetDimensionKeyAtPplDimensionType(i);
              non_zero_voc_cs_it.insert(k_i);
            }
          }
          Vocabulary intersect;
          IntersectVocabularies(new_v_it, non_zero_voc_cs_it, intersect);
          // If intersection is non-empty then it means that the vocabulary with non-zero coefficient should be
          // added to new_v_it
          if(intersect.size() != 0)
            new_v_it.insert(non_zero_voc_cs_it.begin(), non_zero_voc_cs_it.end());
        }

        reached_fixpoint = (new_v_it.size() == v_it.size());
        v_it = new_v_it;
      }
      while(!reached_fixpoint);

      Vocabulary old_voc = v;
      UnionVocabularies(old_voc, v_it, v);
    }

    v.erase(k);
    return v;
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::AddEquality(const DimensionKey& k1, 
                                             const DimensionKey& k2) {
    // If k1 and k2 are equal, then equality stands by itself
    if(k1 == k2)
      return;

    Parma_Polyhedra_Library::Variable v_k1(GetPplDimensionTypeFromDimensionKey(k1));
    Parma_Polyhedra_Library::Variable v_k2(GetPplDimensionTypeFromDimensionKey(k2));

    Parma_Polyhedra_Library::Linear_Expression le_k1(v_k1);
    Parma_Polyhedra_Library::Linear_Expression le_k2(v_k2);

    Parma_Polyhedra_Library::Constraint eq_c = (le_k1 == le_k2);
    AddConstraint(eq_c);
  }

  // Havoc: Havoc out the vocabulary v, i.e. remove any constraints on them 
  template <typename PSET>
  ref_ptr<AbstractValue> PointsetPowersetAv<PSET>::Havoc(const Vocabulary & v) const {
    Parma_Polyhedra_Library::Variables_Set vars;
    for(Vocabulary::const_iterator it = v.begin(); it != v.end(); it++) {
      if(this->voc_.find(*it) != this->voc_.end()) {
        Parma_Polyhedra_Library::Variable var(GetPplDimensionTypeFromDimensionKey(*it));
        vars.insert(var);
      }
    }
    ref_ptr<PointsetPowersetAv> ret = new PointsetPowersetAv(*this);
    ret->pp_.unconstrain(vars);
    return ret.get_ptr();
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::AddConstraint(PointsetPowersetAv<PSET>::affexp_type lhs, PointsetPowersetAv<PSET>::affexp_type rhs, PointsetPowersetAv<PSET>::OpType op) {
    Parma_Polyhedra_Library::Linear_Expression le_exp_lhs(lhs.second);
    for(std::map<DimensionKey, mpz_class>::const_iterator it = lhs.first.begin(); it != lhs.first.end(); it++) {
      Parma_Polyhedra_Library::Variable v_it(GetPplDimensionTypeFromDimensionKey(it->first));
      Parma_Polyhedra_Library::Linear_Expression le_v_it(v_it);
      le_exp_lhs = le_exp_lhs + (it->second)*le_v_it;
    }

    Parma_Polyhedra_Library::Linear_Expression le_exp_rhs(rhs.second);
    for(std::map<DimensionKey, mpz_class>::const_iterator it = rhs.first.begin(); it != rhs.first.end(); it++) {
      Parma_Polyhedra_Library::Variable v_it(GetPplDimensionTypeFromDimensionKey(it->first));
      Parma_Polyhedra_Library::Linear_Expression le_v_it(v_it);
      le_exp_rhs = le_exp_rhs + (it->second)*le_v_it;
    }

    Parma_Polyhedra_Library::Constraint c;
    bool is_equality = false;
    switch(op) {
    case EQ:
      c = (le_exp_lhs == le_exp_rhs);
      is_equality = true;
      break;
    case GE:
      c = (le_exp_lhs >= le_exp_rhs);
      break;
    case LE:
      c = (le_exp_lhs <= le_exp_rhs);
      break;
    }

    if(equalities_only_ && !is_equality)
      return;
    AddConstraint(c);
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::AddConstraint(const Parma_Polyhedra_Library::Constraint & c) {
    pp_.add_constraint(c);
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::AddConstraints(const Parma_Polyhedra_Library::Constraint_System & cs) {
    pp_.add_constraints(cs);
  }

  // Template specializations for octagons
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::AddConstraints(const Parma_Polyhedra_Library::Constraint_System & cs);
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::AddConstraint(const Parma_Polyhedra_Library::Constraint & c);

  template <typename PSET>
  void PointsetPowersetAv<PSET>::Reduce() {
    pp_.omega_reduce();
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::Reduce(const ref_ptr<AbstractValue>& that) {
    // Default implementation does nothing
  }

  // =====================
  // Vocabulary operations
  // =====================
  // project: project onto Vocabulary v.
  template <typename PSET>
  void PointsetPowersetAv<PSET>::Project(const Vocabulary& v) {
    Vocabulary uni;
    UnionVocabularies(this->voc_, v, uni);
    if(v == uni)
      return; // Projecting on all the variables changes nothing


    Vocabulary proj_out_voc;
    SubtractVocabularies(this->voc_, v, proj_out_voc);

    //Find the indices of the vocabulary projected on in real_pvars
    Parma_Polyhedra_Library::Variables_Set proj_out_vars;
    for(Vocabulary::const_iterator it = proj_out_voc.begin(); it != proj_out_voc.end(); it++) {
      ppl_dimension_type dim_it = GetPplDimensionTypeFromDimensionKey(*it);
      Parma_Polyhedra_Library::Variable var(dim_it);
      proj_out_vars.insert(var);
    }

    Vocabulary inters;
    IntersectVocabularies(v, this->voc_, inters);
    bm_type key_index_bimap_inter = GetVocabularyIndexMap(inters);

    this->voc_ = inters;
    pp_.remove_space_dimensions(proj_out_vars);
    key_index_bimap_ = key_index_bimap_inter;
    return;
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::AddVocabulary(const Vocabulary& v) {
    Vocabulary union_voc;
    UnionVocabularies(this->voc_, v, union_voc);
    if(union_voc == this->voc_)
      return;

    Vocabulary added_voc;
    SubtractVocabularies(v, this->voc_, added_voc);

    ppl_dimension_type old_poly_space_dims = pp_.space_dimension();

    // Add new dimension at the end
    pp_.add_space_dimensions_and_embed(added_voc.size());

    bm_type key_index_bimap_union = GetVocabularyIndexMap(union_voc);
    bm_type key_index_bimap_added_voc = GetVocabularyIndexMap(added_voc);

    // Specify the permutation of the poly_ as per bm_type key_index_bimap_union
    Parma_Polyhedra_Library::Partial_Function permutation;
    ppl_dimension_type num_new_dims_mapped = 0;
    for(Vocabulary::const_iterator voc_it = union_voc.begin(); voc_it != union_voc.end(); voc_it++) {
      bool found = true;
      ppl_dimension_type from = GetPplDimensionTypeFromDimensionKey(key_index_bimap_, *voc_it, found);
      if(!found) {
        // If the key is not found it must be in the added_voc. Hence, determine it's location by
        // picking up one of the new added dimensions added in the poly_.
        from = old_poly_space_dims + GetPplDimensionTypeFromDimensionKey(key_index_bimap_added_voc, *voc_it, found);
        assert(found);
        num_new_dims_mapped++;
      }

      ppl_dimension_type to = GetPplDimensionTypeFromDimensionKey(key_index_bimap_union, *voc_it, found);
      assert(found);

      permutation.insert(from, to);
    }

    this->voc_ = union_voc;
    key_index_bimap_ = key_index_bimap_union;

    pp_.map_space_dimensions(permutation);
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::ReplaceVersions(const std::map<Version, Version>& map) {
    std::map<DimensionKey, DimensionKey> m;
    Vocabulary new_voc;
    Vocabulary::const_iterator voc_it;

    for(voc_it = this->voc_.begin(); voc_it != this->voc_.end(); voc_it++)
      {
        DimensionKey new_k = abstract_domain::replaceVersions(*voc_it, map);
        new_voc.insert(new_k);
        m.insert(std::pair<DimensionKey, DimensionKey>(*voc_it, new_k));
      }

    bm_type new_key_index_bimap = GetVocabularyIndexMap(new_voc);

    // This will keep track of the map that maps the old space dimensions to their new locations
    // by using map m
    Parma_Polyhedra_Library::Partial_Function permutation;
    for(voc_it = this->voc_.begin(); voc_it != this->voc_.end(); voc_it++)
      {
        bool found;
        ppl_dimension_type from = GetPplDimensionTypeFromDimensionKey(key_index_bimap_, *voc_it, found);
        assert(found);
        ppl_dimension_type to = GetPplDimensionTypeFromDimensionKey(new_key_index_bimap, m[*voc_it], found) ;
        assert(found);
        permutation.insert(from, to);
      }

    //Update the vocabulary with the new vocabulary
    this->voc_ = new_voc;
    pp_.map_space_dimensions(permutation);
    key_index_bimap_ = new_key_index_bimap;
  }

  // Input/output
  template <typename PSET>
  std::string PointsetPowersetAv<PSET>::ToString() const {
    std::stringstream o;
    o << "Equalities_only:" << equalities_only_;
    PrintVocabularyIndexMap(o);
    o << "PSET(num disj:" << pp_.size() << "), ptr is " << this << ":";
    for(typename Parma_Polyhedra_Library::Pointset_Powerset<PSET>::const_iterator it = pp_.begin(); it != pp_.end(); it++) {
      PrintPSET(o << "\n", it->pointset());
    }
    Parma_Polyhedra_Library::IO_Operators::operator<<(o << "\n", pp_);
    o << "\n";
    return o.str();
  }

  template <typename PSET>
  std::ostream& PointsetPowersetAv<PSET>::PrintPSET(std::ostream& o, const PSET& p) const {
    o << "Congruences:";
    Parma_Polyhedra_Library::Congruence_System cons = p.congruences();
    Parma_Polyhedra_Library::IO_Operators::operator<<(o, cons);
    o << "\tConstraints: ";
    Parma_Polyhedra_Library::Constraint_System cs = p.constraints();
    Parma_Polyhedra_Library::IO_Operators::operator<<(o, cs);
    return o;
  }

  template <>
  std::ostream& PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::PrintPSET(std::ostream& o, const Parma_Polyhedra_Library::Octagonal_Shape<mpz_class>& p) const;

  template <typename PSET>
  std::ostream & PointsetPowersetAv<PSET>::PrintVocabularyIndexMap(std::ostream & o) const {
    o << "\nVocabularyIndexMap (voc_size: " << this->voc_.size() << ") (Term, PPL val):";
    for(Vocabulary::const_iterator it = this->voc_.begin(); it != this->voc_.end(); it++) {
      o << "(";
      abstract_domain::print(o, *it);
      ppl_dimension_type d_it = GetPplDimensionTypeFromDimensionKey(*it);
      Parma_Polyhedra_Library::Variable v_it(d_it);
      o << ", ";
      Parma_Polyhedra_Library::IO_Operators::operator<<(o, v_it);
      o << ")|";
    }
    return o;
  }

  template <typename PSET>
  Parma_Polyhedra_Library::dimension_type PointsetPowersetAv<PSET>::GetPplDimensionTypeFromDimensionKey(const DimensionKey& k) const {
    assert(key_index_bimap_.size() == this->voc_.size());
    bool found;
    ppl_dimension_type ret = GetPplDimensionTypeFromDimensionKey(key_index_bimap_, k, found);
    if(!found) {
      abstract_domain::print(std::cout << "\nPplDimension not found! \nVocabulary:", GetVocabulary());
      abstract_domain::print(std::cout << "\nk is:", k);
      PrintVocabularyIndexMap(std::cout << "\nVocabularyIndexMap:");
      assert(false);
    }

    return ret;
  }

  template <typename PSET>
  Parma_Polyhedra_Library::dimension_type PointsetPowersetAv<PSET>::GetPplDimensionTypeFromDimensionKey(const PointsetPowersetAv<PSET>::bm_type& bimap, const DimensionKey& k, bool &found)
  {
    ppl_dimension_type index;
    try
      {
        index = bimap.left.at(k);
        found = true;
      }
    catch(std::out_of_range & e)
      {
        found = false;
        // Do  nothing
      }
    return index;
  }

  template <typename PSET>
  DimensionKey PointsetPowersetAv<PSET>::GetDimensionKeyAtPplDimensionType(const Parma_Polyhedra_Library::dimension_type& index) const
  {
    assert(key_index_bimap_.size() == this->voc_.size());
    bool found;
    DimensionKey ret = GetDimensionKeyAtPplDimensionType(key_index_bimap_, index, found);
    assert(found);
    return ret;
  }

  template <typename PSET>
  DimensionKey PointsetPowersetAv<PSET>::GetDimensionKeyAtPplDimensionType(const PointsetPowersetAv<PSET>::bm_type& key_index_bimap, const Parma_Polyhedra_Library::dimension_type& index, bool& found) {
    DimensionKey k;
    try
      {
        k = key_index_bimap.right.at(index);
        found = true;
      }
    catch(std::out_of_range & e)
      {
        found = false;
      }
    return k;
  }

  template <typename PSET>
  Parma_Polyhedra_Library::Variables_Set PointsetPowersetAv<PSET>::GetVariableSetFromVocabulary(const Vocabulary& voc) const {
    Parma_Polyhedra_Library::Variables_Set ret;
    for(Vocabulary::const_iterator it = voc.begin(); it != voc.end(); it++) {
      Parma_Polyhedra_Library::Variable v(GetPplDimensionTypeFromDimensionKey(*it));
      ret.insert(v);
    }
    return ret;
  }

  template <typename PSET>
  Parma_Polyhedra_Library::Bounded_Integer_Type_Width PointsetPowersetAv<PSET>::GetPplBoundedIntegerWidth(utils::Bitsize b) const {
    switch(b) {
    case utils::sixty_four:
      return Parma_Polyhedra_Library::BITS_64;
    case utils::thirty_two:
      return Parma_Polyhedra_Library::BITS_32;
    case utils::sixteen:
      return Parma_Polyhedra_Library::BITS_16;
    case utils::eight:
      return Parma_Polyhedra_Library::BITS_8;
    default:
      assert(false);
    }

    return Parma_Polyhedra_Library::BITS_32;
  }

  template <typename PSET>
  typename PointsetPowersetAv<PSET>::bm_type PointsetPowersetAv<PSET>::GetVocabularyIndexMap(const Vocabulary& v) {
    bm_type ret;
    static std::map<Vocabulary, bm_type> vocabulary_index_map_cache;
    std::map<Vocabulary, bm_type>::iterator it = vocabulary_index_map_cache.find(v);
    if(it != vocabulary_index_map_cache.end()) {
      return it->second;
    }

    ppl_dimension_type index = 0;
    for(Vocabulary::const_iterator it = v.begin(); it != v.end(); it++) {
      ret.insert(bm_type::value_type(*it, index));
      index++;
    }
    vocabulary_index_map_cache.insert(std::make_pair(v, ret));
    return ret;
  }

  // A comparator which compares throught the second parameter
  template <typename PSET>
  class DistFuncComp {
    typedef std::pair<std::pair<std::shared_ptr<PSET>, std::shared_ptr<PSET> >, std::pair<unsigned, mpz_class> > T;
  public:
    bool operator()(T a1, T a2) {
      std::pair<unsigned, mpz_class> a1_d = a1.second;
      std::pair<unsigned, mpz_class> a2_d = a2.second;

      // Compare the number of incompatible intervals for the distance
      // The distance which has more incompatible intervals is a bigger distance
      if(a1_d.first != a2_d.second)
        return a1_d.first > a2_d.second;

      return (a1_d.second > a2_d.second);
    }
  };

  template <typename PSET>
  std::ostream& PrintInterval(std::ostream& o, Parma_Polyhedra_Library::Rational_Interval itv) {
    o << "(";
    if(itv.lower_is_boundary_infinity())
      o << "inf";
    else
      o << itv.lower();
    o << ",";
    if(itv.upper_is_boundary_infinity())
      o << "inf";
    else
      o << itv.upper();
    o << ")";
    return o;
  }

  template <typename PSET>
  std::pair<unsigned, mpz_class> GetDistance(std::shared_ptr<PSET>& ps1, std::shared_ptr<PSET>& ps2) {
    typedef Parma_Polyhedra_Library::Rational_Interval ITV;
    typedef Parma_Polyhedra_Library::Box<ITV> TBox;

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nCalculating distance between ps1 and ps2:";
                   Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\nps1:", *ps1);
                   Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\nps2:", *ps2););

    std::pair<unsigned, mpz_class> dist(0, 0);

    // TODO: Cache so that the box from a pset is not created everytime
    std::shared_ptr<TBox> b1(new TBox(*ps1));
    std::shared_ptr<TBox> b2(new TBox(*ps2));

    for(Parma_Polyhedra_Library::dimension_type i = ps1->space_dimension(); i-- > 0; ) {
      Parma_Polyhedra_Library::Variable v(i);
      ITV itv1 = b1->get_interval(v);
      ITV itv2 = b2->get_interval(v);

      DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                     PrintInterval<PSET>(std::cout << "\nitv1:", itv1);
                     PrintInterval<PSET>(std::cout << "\nitv2:", itv2););

      // If one of the interval is undounded in one direction while another is not, the intervals are incompatible
      bool incompatible = (itv1.lower_is_boundary_infinity() ^ itv2.upper_is_boundary_infinity()) || 
                             (itv2.lower_is_boundary_infinity() ^ itv1.upper_is_boundary_infinity());

      if(incompatible) {
        dist.first++;
        continue;
      }

      // Calculate the minimum of the distance between lower_bound of itv1 and upper_bound of itv2
      //                      and the distance between upper_bound of itv1 and lower_bound of itv2
      // If the intervals have a non-empty intersection, then the distance between them is zero
      ITV intersect_itv(itv1);
      intersect_itv.intersect_assign(itv2);
      DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                     PrintInterval<PSET>(std::cout << "\nintersect_itv:", intersect_itv););

      bool intersect_empty = intersect_itv.is_empty();

      // If intersection is empty, then there is a non-zero distance between two intervals
      // Calculate that distance in itv_dist
      if(intersect_empty) {
        bool d1_inf = itv1.lower_is_boundary_infinity();
        mpz_class d1;
        if(!d1_inf) {
          assert(!itv2.upper_is_boundary_infinity());
          d1 = itv1.lower() - itv2.upper();
          if(d1 < 0) d1 = -d1;
        }

        bool d2_inf = itv2.lower_is_boundary_infinity();
        mpz_class d2;
        if(!d2_inf) {
          assert(!itv1.upper_is_boundary_infinity());
          d2 = itv2.lower() - itv1.upper();
          if(d2 < 0) d2 = -d2;
        }

        mpz_class itv_dist;
        if(d1_inf) {
          itv_dist = d2;
        } else {
          if(d2_inf) {
            itv_dist = d1;
          } else {
            itv_dist = (d1 <= d2)? d1 : d2;
          }
        }

        DEBUG_PRINTING(DBG_PRINT_EVERYTHING, std::cout << "\nitv_dist:" << itv_dist;);
        dist.second += itv_dist;
      }
    }

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, std::cout << "\ndist:(" << dist.first << "," << dist.second << ")";);
    return dist;
  }

  template <typename PSET>
  void PointsetPowersetAv<PSET>::MergeHeuristic() {
    typedef std::pair<std::pair<std::shared_ptr<PSET>, std::shared_ptr<PSET> >, std::pair<unsigned, mpz_class> > T;

    unsigned max_disjunctions_temp = max_disjunctions;
    if(equalities_only_)
      max_disjunctions_temp = 1;

    // Call reduce before performing Merge
    pp_.omega_reduce();

    if(pp_.size() <= max_disjunctions_temp)
      return;

    std::set<T, DistFuncComp<PSET> > dist_func;

    // Make a copy of pp as a vector of shared ptr, these are used 
    std::vector<std::shared_ptr<PSET> > pp_copy;
    for(typename Parma_Polyhedra_Library::Pointset_Powerset<PSET>::const_iterator it = pp_.begin(); it != pp_.end(); it++) {
      std::shared_ptr<PSET> it_sp(new PSET(it->pointset()));
      pp_copy.push_back(it_sp);
    }

    for(typename std::vector<std::shared_ptr<PSET> >::iterator it1 = pp_copy.begin(); it1 != pp_copy.end(); it1++) {
      typename std::vector<std::shared_ptr<PSET> >::iterator it2 = it1;
      it2++;
      for(; it2 != pp_copy.end(); it2++) {
        dist_func.insert(std::make_pair(std::make_pair(*it1, *it2), GetDistance(*it1, *it2)));
      }
    }

    // If the distance is negative, the abstract domains should be merged
    // even if the number of disjunctions doesn't exceed max_disjunctions
    while(pp_copy.size() > max_disjunctions) {
      // Get the pair of abstract values, with the minimum distance 
      T pair_with_min_dist = *(dist_func.begin());

      std::shared_ptr<PSET> ps1 = pair_with_min_dist.first.first;
      std::shared_ptr<PSET> ps2 = pair_with_min_dist.first.second;
      std::pair<unsigned, mpz_class> dist = pair_with_min_dist.second;

      // Remove these two values from pp_copy
      typename std::vector<std::shared_ptr<PSET> >::iterator ps1_pos = std::find(pp_copy.begin(), pp_copy.end(), ps1);
      assert(ps1_pos != pp_copy.end());
      pp_copy.erase(ps1_pos);

      typename std::vector<std::shared_ptr<PSET> >::iterator ps2_pos = std::find(pp_copy.begin(), pp_copy.end(), ps2);
      assert(ps2_pos != pp_copy.end());
      pp_copy.erase(ps2_pos);

      // Remove all those values in dist_func which have either ps1 or ps2 in them
      for(typename std::set<T, DistFuncComp<PSET> >::iterator it = dist_func.begin(); it != dist_func.end(); ) {
        if(it->first.first == ps1 || it->first.first == ps2 || it->first.second == ps1 || it->first.second == ps2) {
          dist_func.erase(it++);
        } else {
          it++;
        }
      }

      // Find the least upper bound of ps1 and ps2 in ps1_ub_ps2
      std::shared_ptr<PSET> ps1_ub_ps2 = ps1;
      ps1_ub_ps2->upper_bound_assign(*ps2);

      for(typename std::vector<std::shared_ptr<PSET> >::iterator it = pp_copy.begin(); it != pp_copy.end(); it++) {
        dist_func.insert(std::make_pair(std::make_pair(*it, ps1_ub_ps2), GetDistance(*it, ps1_ub_ps2)));
      }

      pp_copy.push_back(ps1_ub_ps2);
    }

    pp_.clear();
    for(typename std::vector<std::shared_ptr<PSET> >::iterator it = pp_copy.begin(); it != pp_copy.end(); it++) {
      //Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n**it:", **it);
      pp_.add_disjunct(**it);
    }
    DEBUG_PRINTING(DBG_PRINT_DETAILS, std::cout << "\nnum disjunction after merge heuristic:" << pp_.size(););
  }
}


#endif // src_AbstractDomain_PointsetPowerset_pointset_powerset_av_impl_hpp
