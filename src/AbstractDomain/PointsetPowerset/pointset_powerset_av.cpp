#include "pointset_powerset_av.hpp"

#include "ppl.hh"

class Octagonal_Shape_Certificate {
public:
  //! Default constructor.
  Octagonal_Shape_Certificate() : affine_dim(0), num_constraints(0) {
    // This is the certificate for a zero-dim universe.
    PPL_ASSERT(OK());
  }

  //! Constructor: computes the certificate for \p ph.
  Octagonal_Shape_Certificate(const Parma_Polyhedra_Library::Octagonal_Shape<mpz_class>& ph) 
    : affine_dim(ph.affine_dimension()), num_constraints(0) {
    // It is assumed that `ph' is not an empty polyhedron.
    PPL_ASSERT(!ph.marked_empty());
    Parma_Polyhedra_Library::Constraint_System cs = ph.minimized_constraints();
    for(Parma_Polyhedra_Library::Constraint_System::const_iterator it = cs.begin(); it != cs.end(); it++) {
      num_constraints++;
    }
  }

  //! Copy constructor.
  Octagonal_Shape_Certificate(const Octagonal_Shape_Certificate& y)
    : affine_dim(y.affine_dim), num_constraints(y.num_constraints) {
  }

  //! Destructor.
  ~Octagonal_Shape_Certificate() {
  }

  //! The comparison function for certificates.
  /*!
  */
  int compare(const Octagonal_Shape_Certificate& y) const {
    PPL_ASSERT(OK() && y.OK());
    if (affine_dim != y.affine_dim)
      return (affine_dim > y.affine_dim) ? 1 : -1;
    if (num_constraints != y.num_constraints)
      return (num_constraints > y.num_constraints) ? 1 : -1;
    return 0;
  }

  //! Compares \p *this with the certificate for polyhedron \p ph.
  int compare(const Parma_Polyhedra_Library::Octagonal_Shape<mpz_class>& ph) const {
    Octagonal_Shape_Certificate y(ph);
    return compare(y);
  }

#ifdef PPL_DOXYGEN_INCLUDE_IMPLEMENTATION_DETAILS
  /*! \brief
    Returns <CODE>true</CODE> if and only if the certificate for
    octagon \p ph is strictly smaller than \p *this.
  */
#endif // defined(PPL_DOXYGEN_INCLUDE_IMPLEMENTATION_DETAILS)
  bool is_stabilizing(const Parma_Polyhedra_Library::Octagonal_Shape<mpz_class>& ph) const {
    return compare(ph) == 1;
  }


  //! A total ordering on BHRZ03 certificates.
  /*! \ingroup PPL_CXX_interface
    This binary predicate defines a total ordering on BHRZ03 certificates
    which is used when storing information about sets of polyhedra.
  */
  struct Compare {
    //! Returns <CODE>true</CODE> if and only if \p x comes before \p y.
    bool operator()(const Octagonal_Shape_Certificate& x,
                    const Octagonal_Shape_Certificate& y) const {
      // For an efficient evaluation of the multiset ordering based
      // on this LGO relation, we want larger elements to come first.
      return x.compare(y) == 1;
    }
  };

#ifdef PPL_DOXYGEN_INCLUDE_IMPLEMENTATION_DETAILS
  //! Check if gathered information is meaningful.
#endif // defined(PPL_DOXYGEN_INCLUDE_IMPLEMENTATION_DETAILS)
  bool OK() const;

private:
  //! Affine dimension of the polyhedron.
  Parma_Polyhedra_Library::dimension_type affine_dim;
  //! Cardinality of a non-redundant constraint system for the polyhedron.
  Parma_Polyhedra_Library::dimension_type num_constraints;
};

namespace abstract_domain
{
  // Widen specialization for C_Polyhedron
  // that_av must definitely entail *this
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron>::WidenHomogeneous (const ref_ptr<AbstractValue> & that_av) {
    typedef Parma_Polyhedra_Library::BHRZ03_Certificate T1;
    const PointsetPowersetAv* that = static_cast<const PointsetPowersetAv *>(that_av.get_ptr());
    typename Parma_Polyhedra_Library::Widening_Function<Parma_Polyhedra_Library::Polyhedron> widen_fun = 
      Parma_Polyhedra_Library::widen_fun_ref(&Parma_Polyhedra_Library::Polyhedron::BHRZ03_widening_assign);

    if(use_extrapolation) {
      unsigned max_disjunctions_local = max_disjunctions;
      pp_.BGP99_extrapolation_assign(that->pp_, widen_fun, max_disjunctions_local);
    } else {
      pp_.BHZ03_widening_assign<T1>(that->pp_, widen_fun);
    }

    // Merge so that the size of pp_ does not exceed max_disjunctions
    MergeHeuristic();
  }

  // Widen specialization for Octagons
  // Convert Octogons to Polyhedron, call that widen operation and then convert back
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::WidenHomogeneous (const ref_ptr<AbstractValue> & that_av) {
    typedef Octagonal_Shape_Certificate T1;
    const PointsetPowersetAv* that = static_cast<const PointsetPowersetAv *>(that_av.get_ptr());
    typename Parma_Polyhedra_Library::Widening_Function<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> > 
      widen_fun = Parma_Polyhedra_Library::widen_fun_ref(&Parma_Polyhedra_Library::Octagonal_Shape<mpz_class>::widening_assign);

    if(use_extrapolation) {
      unsigned max_disjunctions_local = max_disjunctions;
      pp_.BGP99_extrapolation_assign(that->pp_, widen_fun, max_disjunctions_local);
    } else {
      pp_.BHZ03_widening_assign<T1>(that->pp_, widen_fun);
    }

    // Merge so that the size of pp_ does not exceed max_disjunctions
    MergeHeuristic();
  }

  // Add constraint specialization for octagons
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::AddConstraints(const Parma_Polyhedra_Library::Constraint_System & cs) {
    typedef Parma_Polyhedra_Library::C_Polyhedron POLY_T;
    typedef Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> OCT_T;

    Parma_Polyhedra_Library::Constraint_System oct_cs;
    ppl_dimension_type non_oct_cs_size = 0;
    Parma_Polyhedra_Library::Constraint_System non_oct_cs;
    for(Parma_Polyhedra_Library::Constraint_System::const_iterator it = cs.begin(); it != cs.end(); it++) {
      bool is_octogonal_constraint = true;
      unsigned num_non_zero_coeffs = 0;
      for(ppl_dimension_type i = it->space_dimension(); i-- > 0; ) {
        Parma_Polyhedra_Library::Variable v_i(i);
        const Parma_Polyhedra_Library::GMP_Integer coeff = it->coefficient(v_i);
        if(coeff != 0 && coeff != 1 && coeff != -1) {
          is_octogonal_constraint = false;
          break;
        }
        if(coeff != 0) {
          num_non_zero_coeffs++;
        }
      }
      if(num_non_zero_coeffs > 2)
        is_octogonal_constraint = false;

      if(is_octogonal_constraint) {
        oct_cs.insert(*it);
      } else {
        non_oct_cs.insert(*it);
        non_oct_cs_size++;
      }
    }

    pp_.add_constraints(oct_cs);

    // Performance optimization
    if(false) {
    if(non_oct_cs_size != 0) {
      // Convert pp_ to a polyhedron representing an octagon
      Parma_Polyhedra_Library::Pointset_Powerset<POLY_T> pp_poly(pp_.space_dimension(), Parma_Polyhedra_Library::EMPTY);
      for(Parma_Polyhedra_Library::Pointset_Powerset<OCT_T>::const_iterator it = pp_.begin(); it != pp_.end(); it++) {
        POLY_T poly_it(it->pointset());
        poly_it.add_constraints(cs);
        pp_poly.add_disjunct(poly_it);
      }

      pp_.clear();
      for(Parma_Polyhedra_Library::Pointset_Powerset<POLY_T>::const_iterator it = pp_poly.begin(); it != pp_poly.end(); it++) {
        OCT_T p(it->pointset());
        pp_.add_disjunct(p);
      }
    }
    }
  }

  // Add constraint specialization for octagons
  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::AddConstraint(const Parma_Polyhedra_Library::Constraint & c) {
    Parma_Polyhedra_Library::Constraint_System cs;
    cs.insert(c);
    AddConstraints(cs);
  }


  template <>
  std::ostream& PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::PrintPSET(std::ostream& o, const Parma_Polyhedra_Library::Octagonal_Shape<mpz_class>& p) const {
    o << "Congruences:";
    Parma_Polyhedra_Library::Congruence_System cons = p.congruences();
    Parma_Polyhedra_Library::IO_Operators::operator<<(o, cons);
    o << "\tConstraints: ";
    Parma_Polyhedra_Library::Constraint_System cs = p.constraints();
    Parma_Polyhedra_Library::IO_Operators::operator<<(o, cs);
    return o;
  }

  template <>
  void PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> >::Reduce
  (const ref_ptr<AbstractValue>& that) {
    utils::Timer reduce_timer("Reduce:", std::cout, false);
    DEBUG_PRINTING(DBG_PRINT_DETAILS, std::cout << "\nIn reduce:";);
    ref_ptr<abstract_domain::PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> > eqav = 
      static_cast<abstract_domain::PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> *>(that.get_ptr());
    if(eqav != NULL && eqav->equalities_only()) {
      assert(eqav->num_disjuncts() == 1u);
      const Parma_Polyhedra_Library::C_Polyhedron eqs = eqav->GetDisjunct(0);
      DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                     eqav->print(std::cout << "\neqav:");
                     print(std::cout << "\nthis before addEqualities:"););
      AddConstraints(eqs.constraints());
      DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                     print(std::cout << "\nthis after addEqualities:"););
    }
    DEBUG_PRINTING(DBG_PRINT_OVERVIEW, std::cout << "\nReduce:" << reduce_timer.elapsed(););
  }

}
