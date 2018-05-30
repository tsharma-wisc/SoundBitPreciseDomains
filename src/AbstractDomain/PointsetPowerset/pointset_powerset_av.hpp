#ifndef src_AbstractDomain_PointsetPowerset_pointset_powerset_av_hpp
#define src_AbstractDomain_PointsetPowerset_pointset_powerset_av_hpp

#include <algorithm>
#include <sstream>

#include "src/AbstractDomain/common/AbstractValue.hpp"
#include "ppl.hh"

#include "utils/debug/DebugOptions.hpp"
#include "boost/bimap.hpp"

namespace abstract_domain
{
  /* PointsetPowersetAv: A wrapper over the pointset_powerset class specified in PPL.
     The key operation this class defines is the MergeHeuristic and the Wrap operation.
     This class is templatized over a base class PSET. PSET is a numerical abstract
     domain. As per Pointset_Powerset_defs.ff class, this class is well defined for
     template \p PSET as the simple pointset domains:
     <CODE>C_Polyhedron</CODE>,
     <CODE>NNC_Polyhedron</CODE>,
     <CODE>Grid</CODE>,
     <CODE>Octagonal_Shape\<T\></CODE>,
     <CODE>BD_Shape\<T\></CODE>,
     <CODE>Box\<T\></CODE>.

     Note that of these domains, only C_Polyhedron and Octogonal_Shape have been tested.
  */
  template <typename PSET>
  class PointsetPowersetAv : public AbstractValue {
  public:
    typedef Parma_Polyhedra_Library::dimension_type ppl_dimension_type;
    typedef boost::bimap<DimensionKey, ppl_dimension_type> bm_type;
    typedef std::map<DimensionKey, bool> VocabularySignedness;
    typedef std::pair<std::map<DimensionKey, mpz_class>, mpz_class> equality_type;

    // Parameter to control the maximum number of disjunctions
    static unsigned max_disjunctions;
    static unsigned use_extrapolation;

    // Constructors
    // ============
    // Default constructor builds top -- an empty matrix.
    PointsetPowersetAv(const Vocabulary& voc, bool is_universe = true, bool add_equalities_only = false);
    PointsetPowersetAv(const PointsetPowersetAv & orig);
    ~PointsetPowersetAv();

    PointsetPowersetAv & operator= (const PointsetPowersetAv& a);
    ref_ptr<AbstractValue> Copy() const;

    bool IsDistributive() const;

    // Other ways to create PointsetPowersetAv elements
    // ====================================
    ref_ptr<AbstractValue> Top() const;
    ref_ptr<AbstractValue> Bottom() const;
        
    // Comparison operations
    bool operator==  (const AbstractValue& that) const;
    bool operator== (const PointsetPowersetAv&that) const;
    bool Overapproximates(const ref_ptr<AbstractValue>&) const;

    // Abstract Domain operations
    bool IsBottom() const;
    bool IsTop() const;
    unsigned GetHeight () const;
    void Join(const ref_ptr<AbstractValue>&);
    void JoinSingleton(const ref_ptr<AbstractValue>& av) {
      Join(av);
    }
    void Meet(const ref_ptr<AbstractValue>&);
    void Widen(const ref_ptr<AbstractValue>&);
    void Wrap(const VocabularySignedness& voc_to_wrap);
    bool IsConstant(mpz_class& val) const;
    Vocabulary GetDependentVocabulary(DimensionKey& k) const;

    void AddEquality(const DimensionKey& v1, 
                     const DimensionKey& v2);

    void AddEqualities(const std::vector<equality_type>& eqs);

    // Havoc: Havoc out the vocabulary v
    ref_ptr<AbstractValue> Havoc (const Vocabulary & v) const;

    // PSET Operations
    void AddConstraint(affexp_type lhs, affexp_type rhs, OpType op);
    void AddConstraint(const Parma_Polyhedra_Library::Constraint & c);
    void AddConstraints(const Parma_Polyhedra_Library::Constraint_System& cs);
    unsigned num_disjuncts() const { return pp_.size(); }
    //In - place reduction
    void Reduce();
    void Reduce(const ref_ptr<AbstractValue>& that);

    const PSET& GetDisjunct(unsigned i) const {
      typename Parma_Polyhedra_Library::Pointset_Powerset<PSET>::const_iterator it = pp_.begin();
      std::advance(it, i);
      return it->pointset();
    }
    inline void AddDisjunct(const PSET& d) {
      pp_.add_disjunct(d);
    }

    // Vocabulary operations
    // =====================
    // project: project onto Vocabulary v.
    void Project(const Vocabulary&);
    void AddVocabulary(const Vocabulary&);
    void ReplaceVersions(const std::map<Version, Version >&);

    static bm_type GetVocabularyIndexMap(const Vocabulary&);


    // Input/output
    std::string ToString() const;
    std::ostream & PrintVocabularyIndexMap(std::ostream & o) const;

    bool equalities_only() const { return equalities_only_;}
  private:
    void Wrap(const Vocabulary& voc_to_wrap, bool is_signed);
    void Wrap(const Vocabulary& voc_to_wrap, bool is_signed, utils::Bitsize bitsize);

    // Calling this function ensures that the number of disjunctions
    // in the powerset don't go beyond max_disjunctions
    void MergeHeuristic();

    void raw_join(const ref_ptr<PointsetPowersetAv>&);

    ppl_dimension_type GetPplDimensionTypeFromDimensionKey(const DimensionKey&) const;
    static ppl_dimension_type GetPplDimensionTypeFromDimensionKey(const bm_type&, const DimensionKey&, bool& found);
    DimensionKey GetDimensionKeyAtPplDimensionType(const ppl_dimension_type&) const;
    static DimensionKey GetDimensionKeyAtPplDimensionType(const bm_type&, const ppl_dimension_type&, bool& found);
    Parma_Polyhedra_Library::Variables_Set GetVariableSetFromVocabulary(const Vocabulary& v) const;
    Parma_Polyhedra_Library::Bounded_Integer_Type_Width GetPplBoundedIntegerWidth(utils::Bitsize b) const;

    void WidenHomogeneous(const ref_ptr<AbstractValue>&);

    // Print helper functions
    std::ostream& PrintPSET(std::ostream& o, const PSET& p) const;

    // Data members
    Parma_Polyhedra_Library::Pointset_Powerset<PSET> pp_;
    bm_type key_index_bimap_;
    bool equalities_only_; // Flag that specifies the domain to only add equalities
  };
    
} // abstract_value

#include "pointset_powerset_av_impl.hpp"

#endif // src_AbstractDomain_PointsetPowerset_pointset_powerset_av_hpp
