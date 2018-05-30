#ifndef src_AbstractDomain_PointsetPowerset_pointset_powerset_wrap_assign_h
#define src_AbstractDomain_PointsetPowerset_pointset_powerset_wrap_assign_h

#include "ppl.hh"
#include "utils/debug/DebugOptions.hpp"

// This file borrows implementation of external/ppl-1.1/src/wrap_assign.hh to 
// implement wrap operation for Pointset_Powerset that doesn't call any upper 
// bound operation. Instead of calling upper bound, these method just collect 
// them as a separate disjuncts in a Point_Powerset class
namespace abstract_domain {

  template <typename PSET>
  void wrap_assign_pointset_list_ind
    (std::list<PSET>& pointset_list,
     Parma_Polyhedra_Library::Variables_Set& vars,
     Parma_Polyhedra_Library::Implementation::Wrap_Translations::const_iterator first,
     Parma_Polyhedra_Library::Implementation::Wrap_Translations::const_iterator end,
     Parma_Polyhedra_Library::Bounded_Integer_Type_Width w,
     Parma_Polyhedra_Library::Coefficient_traits::const_reference min_value,
     Parma_Polyhedra_Library::Coefficient_traits::const_reference max_value,
     const Parma_Polyhedra_Library::Constraint_System& cs,
     Parma_Polyhedra_Library::Coefficient& tmp1,
     Parma_Polyhedra_Library::Coefficient& tmp2) {

    typedef Parma_Polyhedra_Library::dimension_type dimension_type;
    typedef Parma_Polyhedra_Library::Implementation::Wrap_Translations Wrap_Translations;
    typedef Parma_Polyhedra_Library::Implementation::Wrap_Dim_Translations Wrap_Dim_Translations;
    typedef Parma_Polyhedra_Library::Variable Variable;
    typedef Parma_Polyhedra_Library::Coefficient Coefficient;
    typedef Parma_Polyhedra_Library::Constraint_System Constraint_System;


    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nIn wrap_assign_pointset_list_ind:\npointset_list";
                   for(typename std::list<PSET>::const_iterator it = pointset_list.begin(); it != pointset_list.end(); it++) {
                     Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n*it:", *it);
                   });

    const dimension_type space_dim = pointset_list.begin()->space_dimension();
    for (Wrap_Translations::const_iterator i = first; i != end; ++i) {
      const Wrap_Dim_Translations& wrap_dim_translations = *i;
      const Variable x(wrap_dim_translations.var);
      const Coefficient& first_quadrant = wrap_dim_translations.first_quadrant;
      const Coefficient& last_quadrant = wrap_dim_translations.last_quadrant;
      Coefficient& quadrant = tmp1;
      Coefficient& shift = tmp2;

      unsigned num_iterations = pointset_list.size();
      typename std::list<PSET>::iterator it = pointset_list.begin();
      for(unsigned j = 0; j < num_iterations; j++, it++) {
        for (quadrant = first_quadrant; quadrant <= last_quadrant; ++quadrant) {
          PSET p(*it);
          if (quadrant != 0) {
            Parma_Polyhedra_Library::mul_2exp_assign(shift, quadrant, w);
            p.affine_image(x, x - shift, 1);
          }
          // `x' has just been wrapped.
          vars.erase(x.id());

          // Refine `p' with all the constraints in `cs' not depending
          // on variables in `vars'.
          if (vars.empty())
            p.refine_with_constraints(cs);
          else {
            for (Constraint_System::const_iterator j = cs.begin(),
                 cs_end = cs.end(); j != cs_end; ++j)
              if (j->expression().all_zeroes(vars))
                // `*j' does not depend on variables in `vars'.
                p.refine_with_constraint(*j);
          }
          p.refine_with_constraint(min_value <= x);
          p.refine_with_constraint(x <= max_value);

          if(quadrant == 0)
            it->m_swap(p);
          else
            pointset_list.push_back(p);
        }
      }
    }

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nFinal value of pointset_list:";
                   for(typename std::list<PSET>::const_iterator it = pointset_list.begin(); it != pointset_list.end(); it++) {
                     Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n*it:", *it);
                   });
  }


  template <typename PSET>
  void wrap_assign_pointset_list_col
    (std::list<PSET>& dest,
     const PSET& src,
     const Parma_Polyhedra_Library::Variables_Set& vars,
     Parma_Polyhedra_Library::Implementation::Wrap_Translations::const_iterator first,
     Parma_Polyhedra_Library::Implementation::Wrap_Translations::const_iterator end,
     Parma_Polyhedra_Library::Bounded_Integer_Type_Width w,
     Parma_Polyhedra_Library::Coefficient_traits::const_reference min_value,
     Parma_Polyhedra_Library::Coefficient_traits::const_reference max_value,
     const Parma_Polyhedra_Library::Constraint_System* cs_p,
     Parma_Polyhedra_Library::Coefficient& tmp) {

    typedef Parma_Polyhedra_Library::Implementation::Wrap_Dim_Translations Wrap_Dim_Translations;
    typedef Parma_Polyhedra_Library::Variable Variable;
    typedef Parma_Polyhedra_Library::Variables_Set Variables_Set;
    typedef Parma_Polyhedra_Library::Coefficient Coefficient;

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nIn wrap_assign_pointset_list_col:\nsrc:";
                   Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\nsrc:", src););

    if (first == end) {
      PSET p(src);
      if (cs_p != 0)
        p.refine_with_constraints(*cs_p);
      for (Variables_Set::const_iterator i = vars.begin(),
           vars_end = vars.end(); i != vars_end; ++i) {
        const Variable x(*i);
        p.refine_with_constraint(min_value <= x);
        p.refine_with_constraint(x <= max_value);
      }
      dest.push_back(p);
    }
    else {
      const Wrap_Dim_Translations& wrap_dim_translations = *first;
      const Variable x(wrap_dim_translations.var);
      const Coefficient& first_quadrant = wrap_dim_translations.first_quadrant;
      const Coefficient& last_quadrant = wrap_dim_translations.last_quadrant;
      Coefficient& shift = tmp;
      PPL_DIRTY_TEMP_COEFFICIENT(quadrant);
      for (quadrant = first_quadrant; quadrant <= last_quadrant; ++quadrant) {
        if (quadrant != 0) {
          Parma_Polyhedra_Library::mul_2exp_assign(shift, quadrant, w);
          PSET p(src);
          p.affine_image(x, x - shift, 1);
          wrap_assign_pointset_list_col(dest, p, vars, first+1, end, w, min_value, max_value,
                          cs_p, tmp);
        }
        else
          wrap_assign_pointset_list_col(dest, src, vars, first+1, end, w, min_value, max_value,
                          cs_p, tmp);
      }
    }
    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\ndest:";
                   for(typename std::list<PSET>::const_iterator it = dest.begin(); it != dest.end(); it++) {
                     Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n*it:", *it);
                   });                   
  }

  template <typename PSET>
  Parma_Polyhedra_Library::Pointset_Powerset<PSET> wrap_pointset_as_pointset_powerset(
              const PSET& pointset,
              const Parma_Polyhedra_Library::Variables_Set& vars,
              const Parma_Polyhedra_Library::Bounded_Integer_Type_Width w,
              const Parma_Polyhedra_Library::Bounded_Integer_Type_Representation r,
              const Parma_Polyhedra_Library::Bounded_Integer_Type_Overflow o,
              const Parma_Polyhedra_Library::Constraint_System* cs_p,
              const unsigned complexity_threshold,
              const bool wrap_individually) {

    typedef Parma_Polyhedra_Library::dimension_type dimension_type;
    typedef Parma_Polyhedra_Library::Implementation::Wrap_Translations Wrap_Translations;
    typedef Parma_Polyhedra_Library::Implementation::Wrap_Dim_Translations Wrap_Dim_Translations;
    typedef Parma_Polyhedra_Library::Variable Variable;
    typedef Parma_Polyhedra_Library::Variables_Set Variables_Set;
    typedef Parma_Polyhedra_Library::Coefficient Coefficient;
    typedef Parma_Polyhedra_Library::Constraint_System Constraint_System;
    typedef Parma_Polyhedra_Library::Pointset_Powerset<PSET> Pointset_Powerset_PSET;

    // Start with pointset and add disjuncts to it as the situation demans
    std::list<PSET> ret_list;
    ret_list.push_back(pointset);
    const dimension_type space_dim = pointset.space_dimension();

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nwrap_pointset_as_pointset_powerset:";
                   Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\npointset:", pointset);
                   std::cout << "\nret_list in beginning:";
                   for(typename std::list<PSET>::const_iterator rit = ret_list.begin(); 
                       rit != ret_list.end(); rit++) {
                     Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n*rit:", *rit);
                   });

    // Set `min_value' and `max_value' to the minimum and maximum values
    // a variable of width `w' and signedness `s' can take.
    PPL_DIRTY_TEMP_COEFFICIENT(min_value);
    PPL_DIRTY_TEMP_COEFFICIENT(max_value);
    if (r == Parma_Polyhedra_Library::UNSIGNED) {
      min_value = 0;
      Parma_Polyhedra_Library::mul_2exp_assign(max_value, Parma_Polyhedra_Library::Coefficient_one(), w);
      --max_value;
    }
    else {
      PPL_ASSERT(r == Parma_Polyhedra_Library::SIGNED_2_COMPLEMENT);
      Parma_Polyhedra_Library::mul_2exp_assign(max_value, Parma_Polyhedra_Library::Coefficient_one(), w-1);
      Parma_Polyhedra_Library::neg_assign(min_value, max_value);
      --max_value;
    }

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, std::cout << "\nmin_value:" << min_value << ", max_value:" << max_value;);

    // If we are wrapping variables collectively, the ranges for the
    // required translations are saved in `translations' instead of being
    // immediately applied.
    Wrap_Translations translations;

    // Dimensions subject to translation are added to this set if we are
    // wrapping collectively or if `cs_p' is non null.
    Variables_Set dimensions_to_be_translated;

    // This will contain a lower bound to the number of abstractions
    // to be joined in order to obtain the collective wrapping result.
    // As soon as this exceeds `complexity_threshold', counting will be
    // interrupted and the full range will be the result of wrapping
    // any dimension that is not fully contained in quadrant 0.
    unsigned collective_wrap_complexity = 1;

    // This flag signals that the maximum complexity for collective
    // wrapping as been exceeded.
    bool collective_wrap_too_complex = false;

    if (!wrap_individually) {
      translations.reserve(space_dim);
    }

    // We use `full_range_bounds' to delay conversions whenever
    // this delay does not negatively affect precision.
    Constraint_System full_range_bounds;

    PPL_DIRTY_TEMP_COEFFICIENT(l_n);
    PPL_DIRTY_TEMP_COEFFICIENT(l_d);
    PPL_DIRTY_TEMP_COEFFICIENT(u_n);
    PPL_DIRTY_TEMP_COEFFICIENT(u_d);

    for (Variables_Set::const_iterator i = vars.begin(),
        vars_end = vars.end(); i != vars_end; ++i) {

      const Variable x(*i);

      std::list<PSET> new_ret_list;
      for(typename std::list<PSET>::iterator it = ret_list.begin(); it != ret_list.end(); it++) {
        bool extremum;
        if (!it->minimize(x, l_n, l_d, extremum)) {
        set_full_range:
          DEBUG_PRINTING(DBG_PRINT_EVERYTHING, std::cout << "\nIn set_full_range:";);
          it->unconstrain(x);
          full_range_bounds.insert(min_value <= x);
          full_range_bounds.insert(x <= max_value);
          new_ret_list = ret_list;
          continue;
        }
 
        if (!it->maximize(x, u_n, u_d, extremum))
          goto set_full_range;

        div_assign_r(l_n, l_n, l_d, Parma_Polyhedra_Library::ROUND_DOWN);
        div_assign_r(u_n, u_n, u_d, Parma_Polyhedra_Library::ROUND_DOWN);
        l_n -= min_value;
        u_n -= min_value;
        div_2exp_assign_r(l_n, l_n, w, Parma_Polyhedra_Library::ROUND_DOWN);
        div_2exp_assign_r(u_n, u_n, w, Parma_Polyhedra_Library::ROUND_DOWN);
        Coefficient& first_quadrant = l_n;
        const Coefficient& last_quadrant = u_n;

        DEBUG_PRINTING(DBG_PRINT_EVERYTHING, 
                       std::cout << "\nfirst_quadrant:" << first_quadrant << ", last_quadrant:" << last_quadrant;
                       Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n*it:", *it););

        // Special case: this variable does not need wrapping.
        if (first_quadrant == 0 && last_quadrant == 0) {
          new_ret_list = ret_list;
          continue;
        }

        // If overflow is impossible, try not to add useless constraints.
        if (o == Parma_Polyhedra_Library::OVERFLOW_IMPOSSIBLE) {
          if (first_quadrant < 0)
            full_range_bounds.insert(min_value <= x);
          if (last_quadrant > 0)
            full_range_bounds.insert(x <= max_value);
          new_ret_list = ret_list;
          continue;
        }

        if (o == Parma_Polyhedra_Library::OVERFLOW_UNDEFINED || collective_wrap_too_complex)
          goto set_full_range;

        Coefficient& quadrants = u_d;
        quadrants = last_quadrant - first_quadrant + 1;

        unsigned extension;
        Parma_Polyhedra_Library::Result res = assign_r(extension, quadrants, Parma_Polyhedra_Library::ROUND_IGNORE);
        if (result_overflow(res) != 0 || extension > complexity_threshold)
          goto set_full_range;

        if (!wrap_individually && !collective_wrap_too_complex) {
          res = Parma_Polyhedra_Library::mul_assign_r(collective_wrap_complexity,
                             collective_wrap_complexity, extension, Parma_Polyhedra_Library::ROUND_IGNORE);
          if (result_overflow(res) != 0
              || collective_wrap_complexity > complexity_threshold)
            collective_wrap_too_complex = true;
          if (collective_wrap_too_complex) {
            // Set all the dimensions in `translations' to full range.
            for (Wrap_Translations::const_iterator j = translations.begin(),
                 translations_end = translations.end();
               j != translations_end;
               ++j) {
              const Variable y(j->var);
              it->unconstrain(y);
              full_range_bounds.insert(min_value <= y);
              full_range_bounds.insert(y <= max_value);
            }
          }
        }

        if (wrap_individually && cs_p == 0) {
          Coefficient& quadrant = first_quadrant;
          // Temporary variable holding the shifts to be applied in order
          // to implement the translations.
          Coefficient& shift = l_d;
          for ( ; quadrant <= last_quadrant; ++quadrant) {
            PSET p(*it);
            if (quadrant != 0) {
              Parma_Polyhedra_Library::mul_2exp_assign(shift, quadrant, w);
              p.affine_image(x, x - shift, 1);
            }
            p.refine_with_constraint(min_value <= x);
            p.refine_with_constraint(x <= max_value);
            new_ret_list.push_back(p);
          }
        }
        else if (wrap_individually || !collective_wrap_too_complex) {
          PPL_ASSERT(!wrap_individually || cs_p != 0);
          dimensions_to_be_translated.insert(x);
          translations.push_back(Wrap_Dim_Translations(x, first_quadrant, last_quadrant));
        }
      }
      ret_list = new_ret_list;
    }

    if (!translations.empty()) {
      if (wrap_individually) {
        PPL_ASSERT(cs_p != 0);
        wrap_assign_pointset_list_ind(ret_list, dimensions_to_be_translated,
                                      translations.begin(), translations.end(),
                                      w, min_value, max_value, *cs_p, l_n, l_d);
      }
      else {
        ret_list.clear();
        wrap_assign_pointset_list_col(ret_list, pointset, dimensions_to_be_translated,
                                      translations.begin(), translations.end(),
                                      w, min_value, max_value, cs_p, l_n);
      }
    }

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING,
                   std::cout << "\nret_list:";
                   for(typename std::list<PSET>::const_iterator rit = ret_list.begin(); 
                       rit != ret_list.end(); rit++) {
                     Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n*rit:", *rit);
                   });

    // Create ret from ret_list using add_disjunct
    Pointset_Powerset_PSET ret(pointset.space_dimension(), Parma_Polyhedra_Library::EMPTY);
    for(typename std::list<PSET>::iterator it = ret_list.begin(); it != ret_list.end(); it++) {
      ret.add_disjunct(*it);
    }

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\nret:", ret););

    if (cs_p != 0)
      ret.refine_with_constraints(*cs_p);
    ret.refine_with_constraints(full_range_bounds);

    return ret;
  }


  template <typename PSET>
  void wrap_assign_pointset_powerset(Parma_Polyhedra_Library::Pointset_Powerset<PSET>& pointset,
                                     const Parma_Polyhedra_Library::Variables_Set& vars,
                                     const Parma_Polyhedra_Library::Bounded_Integer_Type_Width w,
                                     const Parma_Polyhedra_Library::Bounded_Integer_Type_Representation r,
                                     const Parma_Polyhedra_Library::Bounded_Integer_Type_Overflow o,
                                     const Parma_Polyhedra_Library::Constraint_System* cs_p,
                                     const unsigned complexity_threshold,
                                     const bool wrap_individually) {

    typedef Parma_Polyhedra_Library::dimension_type dimension_type;
    typedef Parma_Polyhedra_Library::Variables_Set Variables_Set;
    typedef Parma_Polyhedra_Library::Constraint_System Constraint_System;
    typedef Parma_Polyhedra_Library::Pointset_Powerset<PSET> Pointset_Powerset_PSET;

    DEBUG_PRINTING(DBG_PRINT_EVERYTHING, 
                   Parma_Polyhedra_Library::IO_Operators::operator<<(std::cout << "\n\nPointset in wrap_assign_pointset_powerset\n", pointset););

    // We must have cs_p->space_dimension() <= vars.space_dimension()
    //         and  vars.space_dimension() <= pointset.space_dimension().

    // Dimension-compatibility check of `*cs_p', if any.
    if (cs_p != 0) {
      const dimension_type vars_space_dim = vars.space_dimension();
      if (cs_p->space_dimension() > vars_space_dim) {
        std::ostringstream s;
        s << "abstract_domain::wrap_assign_pointset_powerset(..., cs_p, ...):"
          << std::endl
          << "vars.space_dimension() == " << vars_space_dim
          << ", cs_p->space_dimension() == " << cs_p->space_dimension() << ".";
        throw std::invalid_argument(s.str());
      }

#ifndef NDEBUG
      // Check that all variables upon which `*cs_p' depends are in `vars'.
      // An assertion is violated otherwise.
      const Constraint_System cs = *cs_p;
      const dimension_type cs_space_dim = cs.space_dimension();
      Variables_Set::const_iterator vars_end = vars.end();
      for (Constraint_System::const_iterator i = cs.begin(),
             cs_end = cs.end(); i != cs_end; ++i) {
        for (dimension_type d = cs_space_dim; d-- > 0; ) {
          PPL_ASSERT(*i.coefficient(Variable(d)) == 0
                     || vars.find(d) != vars_end);
        }
      }
#endif
    }

    // Wrapping no variable only requires refining with *cs_p, if any.
    if (vars.empty()) {
      if (cs_p != 0)
        pointset.refine_with_constraints(*cs_p);
      return;
    }

    // Dimension-compatibility check of `vars'.
    const dimension_type space_dim = pointset.space_dimension();
    if (vars.space_dimension() > space_dim) {
      std::ostringstream s;
      s << "abstract_domain::wrap_assign_pointset_powerset(vs, ...):" << std::endl
        << "this->space_dimension() == " << space_dim
        << ", required space dimension == " << vars.space_dimension() << ".";
      throw std::invalid_argument(s.str());
    }

    // Wrapping an empty polyhedron is a no-op.
    if (pointset.is_empty())
      return;


    Pointset_Powerset_PSET result(pointset.space_dimension(), Parma_Polyhedra_Library::EMPTY);
    for(typename Pointset_Powerset_PSET::const_iterator it = pointset.begin(); it != pointset.end(); it++) {
      Pointset_Powerset_PSET it_wrap = 
        wrap_pointset_as_pointset_powerset(it->pointset(), vars, w, r, o, cs_p, complexity_threshold, wrap_individually);
      result.upper_bound_assign(it_wrap);
    }

    pointset.m_swap(result);
  }


} // namespace abstract_domain

#endif // src_AbstractDomain_PointsetPowerset_pointset_powerset_wrap_assign_h
