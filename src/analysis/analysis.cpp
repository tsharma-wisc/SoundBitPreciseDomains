#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <iomanip>
#include <getopt.h>
#include <algorithm>
#include <memory>

#include "analysis.hpp"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"

#include "src/AbstractDomain/common/ReducedProductAbsVal.hpp"
#include "src/AbstractDomain/common/BitpreciseWrappedAbstractValue.hpp"
#include "src/AbstractDomain/PointsetPowerset/pointset_powerset_av.hpp"
#include "src/reinterp/wrapped_domain/WrappedDomainWPDSCreator.hpp"

#include "utils/timer/timer.hpp"
#include "wali/wfa/State.hpp"
#include "wali/wpds/RuleFunctor.hpp"
#include "wali/wpds/ewpds/EWPDS.hpp"
#include "wali/wpds/ewpds/ERule.hpp"
#include "wali/witness/Witness.hpp"
#include "wali/witness/WitnessCombine.hpp"
#include "wali/witness/WitnessExtend.hpp"
#include "wali/witness/WitnessWrapper.hpp"
#include "wali/witness/WitnessTrans.hpp"
#include "wali/witness/WitnessRule.hpp"
#include "wali/witness/WitnessMerge.hpp"
#include "wali/witness/WitnessMergeFn.hpp"

using namespace abstract_domain;
typedef PointsetPowersetAv<Parma_Polyhedra_Library::Octagonal_Shape<mpz_class> > PP_OCT_AV;
typedef PointsetPowersetAv<Parma_Polyhedra_Library::C_Polyhedron> PP_CPOLY_AV;

using namespace llvm;

int ProcessCommandLineArgs(int argc, char** argv);

// Lookup path summary for a key in a WFA
sem_elem_t GetPathSummary(wali::wfa::WFA* fa_prog, const wali::Key& program, const wali::Key& key) {
  assert(fa_prog != NULL);

  bool ispost = (fa_prog->getQuery() == wali::wfa::WFA::REVERSE);

  wali::wfa::TransSet pair = fa_prog->match(program, key);
  wali::wfa::TransSet::iterator tsiter = pair.begin();

  sem_elem_t V_b;

  for( ; tsiter != pair.end() ; tsiter++ ) {
    wali::wfa::ITrans* t( *tsiter );

    if(ispost) {
      sem_elem_t c1 = fa_prog->getState(t->to())->weight();
      sem_elem_t c2 = t->weight();
      sem_elem_t tmp(c1->extend(c2));

      if(V_b != NULL)
        V_b  =  V_b->combine(tmp);
      else
        V_b = tmp;
    } else {
      sem_elem_t tmp(t->weight()->extend(fa_prog->getState(t->to())->weight()));
      if(V_b != NULL)
        V_b  =  V_b->combine(tmp);
      else
        V_b = tmp;
    }
  }
  return V_b;
}

/*!
 * @class RuleCopierWithWitness
 *
 * Inserts a copy of the all rules to its member
 * var w, where the weight is wrapped into a witness weight. 
 *
 */
namespace wali {
  namespace wpds {
    class RuleCopierWithWitness : public ConstRuleFunctor
    {
    public:
      WPDS* pds;
      RuleCopierWithWitness(const WPDS* w) {
        // wali::wpds::Wrapper* wrapper = new wali::witness::WitnessWrapper();
        const ewpds::EWPDS* ew = dynamic_cast<const ewpds::EWPDS*>(w);
        assert(ew); // Cannot handle anything other than EWPDS atm
        pds = new ewpds::EWPDS(/*wrapper*/);
      }
      WPDS* getWPDS() { return pds;}
      virtual void operator()( const rule_t & r) {
        const ewpds::ERule* erule = dynamic_cast<const ewpds::ERule*>(r.get_ptr());
        assert(erule != NULL);

        ewpds::ERule erule_cp(*erule);
        sem_elem_t w = erule_cp.weight();
        ref_ptr<AvSemiring> av_w = dynamic_cast<AvSemiring*>(w.get_ptr());
        av_w->setWideningType(REGULAR_WEIGHT);

        sem_elem_t se = erule->weight();
        witness::witness_t wr = new wali::witness::WitnessRule(erule_cp);
        merge_fn_t mf = erule->merge_fn();
        merge_fn_t wmf;
        if(mf.is_valid()) {
          wmf = new witness::WitnessMergeFn(wr, mf);
        }

        ewpds::EWPDS* ewpds = dynamic_cast<ewpds::EWPDS*>(pds);
        ewpds->add_rule(
           erule->from_state(), erule->from_stack(),
           erule->to_state(), erule->to_stack1(), erule->to_stack2(),
           wr, wmf);
      }
    };
  } // end namespace wpds
} // end namespace wali

/*!
 * @class TransCopierWithWitness
 *
 * Inserts a copy of the passed in Trans into its member
 * var WFA, where the weight is wrapped into a witness weight. 
 *
 */
namespace wali {
  namespace wfa {
    class TransCopierWithWitness : public ConstTransFunctor
    {
      WFA* fa; // Client of this class is responsible for deleting fa
    public:
      TransCopierWithWitness( WFA & fa_ ) : fa(new WFA(fa_)) {
        const std::set< Key >& fa_states = fa->getStates();
        for(std::set< Key >::const_iterator it = fa_states.begin(); it != fa_states.end(); it++) {
          fa->getState(*it)->acceptWeight() = new wali::witness::Witness(fa->getState(*it)->acceptWeight());
          fa->getState(*it)->weight() = new wali::witness::Witness(fa->getState(*it)->weight());
        }
      }

      virtual ~TransCopierWithWitness() {}

      WFA* getWFA() { return fa;}

      virtual void operator()( const ITrans* t ) {
        fa->erase(t->from(), t->stack(), t->to());
        ITrans *tc = t->copy();
        tc->setWeight(new wali::witness::WitnessTrans(*t));
        fa->insert(tc);
      }
    }; // class TransCopierWithWitness
  }
}

/*!
 * @class PredSuccMapGenerator
 *
 * For each rule in WFA create its predeccor and successor by looking up
 * the WitnessRules used in the Witness weight for the rule.
 *
 * Input: Takes a WFA with appropriate witness weight
 */
namespace wali {
  namespace wfa {
    class PredSuccMapGenerator : public TransFunctor
    {
      const std::set<const ITrans*>& all_rules_;
      std::map<const ITrans*, std::set<const ITrans*> > pred_map_;
      std::map<const ITrans*, std::set<const ITrans*> > succ_map_;
    public:
      PredSuccMapGenerator(const std::set<const ITrans*>& all_rules): all_rules_(all_rules) {
        for(std::set<const ITrans*>::const_iterator it = all_rules_.begin(); it != all_rules_.end(); it++) {
          pred_map_[*it] = std::set<const ITrans*>();
          succ_map_[*it] = std::set<const ITrans*>();
        }
        print();
      }

      virtual ~PredSuccMapGenerator() {}

      std::map<const ITrans*, std::set<const ITrans*> > getPredMap() { return pred_map_;}
      std::map<const ITrans*, std::set<const ITrans*> > getSuccMap() { return succ_map_;}

      virtual void operator()( ITrans* t ) {
        sem_elem_t t_w = t->weight();
        witness::Witness* wit = dynamic_cast<witness::Witness*>(t_w.get_ptr());

        // Find root_t as the transition in all_rules_ that represents the same trans
        const ITrans* root_t;
        for(std::set<const ITrans*>::const_iterator it = all_rules_.begin(); it != all_rules_.end(); it++) {
          const ITrans* it_t = *it;
          if(   (it_t->from() == t->from())
             && (it_t->stack() == t->stack())
             && (it_t->to() == t->to()))
            root_t = it_t;
        }
        assert(root_t);
        InvestigateWitness(wit, root_t);
      }

      void print() {
        std::cout << "\n\nall_rules_:";
        for(std::set<const ITrans*>::const_iterator it = all_rules_.begin(); it != all_rules_.end(); it++) {
          const ITrans* t = *it;
          std::cout << t << ";";
          t->print(std::cout) << ", ";
        }
        std::cout << "\npred_map_:";
        for(std::map<const ITrans*, std::set<const ITrans*> >::const_iterator it = pred_map_.begin(); it != pred_map_.end(); it++) {
          std::cout << it->first << ";";
          it->first->print(std::cout) << ": ";
          for(std::set<const ITrans*>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            const ITrans* t = *it2;
            std::cout << t << ";";
            t->print(std::cout) << ", ";
          }
          std::cout << "\n";
        }

        std::cout << "\nsucc_map_:";
        for(std::map<const ITrans*, std::set<const ITrans*> >::const_iterator it = succ_map_.begin(); it != succ_map_.end(); it++) {
          it->first->print(std::cout) << ": ";
          for(std::set<const ITrans*>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            const ITrans* t = *it2;
            std::cout << t << ";";
            t->print(std::cout) << ", ";
          }
          std::cout << "\n";
        }
      }

      virtual void InvestigateWitness(witness::Witness* wit, const ITrans* root_t) {
        // Check if wit is a WitnessTrans, WitnessRule, WitnessExtend, WitnessCombine or WitnessMerge type
        witness::WitnessTrans* trans = dynamic_cast<witness::WitnessTrans*>(wit);
        if(trans != NULL) {
          std::cout << "\nInvestigateWitness:";
          const ITrans* child_t_in_wit = &(trans->getTrans());
          const ITrans* child_t;
          for(std::set<const ITrans*>::const_iterator it = all_rules_.begin(); it != all_rules_.end(); it++) {
            const ITrans* it_t = *it;
            if(   (it_t->from() == child_t_in_wit->from())
               && (it_t->stack() == child_t_in_wit->stack())
               && (it_t->to() == child_t_in_wit->to()))
                child_t = it_t;
          }

          if(child_t != root_t) {
            child_t->print(std::cout << " child_t:" << child_t << ";");
            root_t->print(std::cout << ", root_t:" << root_t << ";");
            {
              std::map<const ITrans*, std::set<const ITrans*> >::iterator pred_child_it = pred_map_.find(child_t);
              assert(pred_child_it != pred_map_.end());
              pred_child_it->second.insert(root_t);
            }
            {
              std::map<const ITrans*, std::set<const ITrans*> >::iterator succ_root_it = succ_map_.find(root_t);
              assert(succ_root_it != succ_map_.end());
              succ_root_it->second.insert(child_t);
            }
          }
          print();
          return;       
        }
 
        witness::WitnessRule* rule = dynamic_cast<witness::WitnessRule*>(wit);
        if(rule != NULL)
          return;        

        witness::WitnessCombine* comb = dynamic_cast<witness::WitnessCombine*>(wit);
        if(comb != NULL) {
          std::list<witness::witness_t>& children = comb->children();
          for(std::list<witness::witness_t>::iterator it = children.begin(); it != children.end(); it++) {
            InvestigateWitness((*it).get_ptr(), root_t);
          }
          return;
        }

        witness::WitnessExtend* xtend = dynamic_cast<witness::WitnessExtend*>(wit);
        if(xtend != NULL) {
          if(xtend->hasLeft())
            InvestigateWitness(xtend->left().get_ptr(), root_t);
          if(xtend->hasRight())
            InvestigateWitness(xtend->right().get_ptr(), root_t);
          return;
        }

        witness::WitnessMerge* merge = dynamic_cast<witness::WitnessMerge*>(wit);
        if(merge != NULL) {
          if(merge->hasCaller())
            InvestigateWitness(merge->caller().get_ptr(), root_t);
          if(merge->hasCallee())
            InvestigateWitness(merge->callee().get_ptr(), root_t);
          if(merge->hasRule())
            InvestigateWitness(merge->rule().get_ptr(), root_t);
          return;
        }


      }
    }; // class PredSuccMapGenerator
  }
}

/*!
 * @class GetAllRules
 *
 * Add each rule in WFA cto the vector
 */
namespace wali {
  namespace wfa {
    class GetAllRules : public ConstTransFunctor
    {
      std::set<const ITrans*> all_rules_;
    public:
      GetAllRules() {}
      virtual ~GetAllRules() {}
      std::set<const ITrans*> get() { return all_rules_;}
      virtual void operator()( const ITrans* t ) {
        all_rules_.insert(t);
      }
    };
  }
}

/*!
 * @class DownwardIterationSequencer
 *
 * Performs downward iteration sequence on this WFA by looking up
 * predecessor information from the given wfa with witnesses.
 * It takes a WPDS to look up rules to extend with.
 *
 */
namespace wali {
  namespace wfa {
    class DownwardIterationSequencer
    {
      WFA* fa_;
      WFA* fa_witness_;
      const wpds::WPDS* pds_;
      std::set<const ITrans*> worklist_;
      std::set<const ITrans*> all_rules_;
      std::map<const ITrans*, std::set<const ITrans*> > pred_map_;
      std::map<const ITrans*, std::set<const ITrans*> > succ_map_;

    public:
      DownwardIterationSequencer(WFA* fa, WFA* fa_witness, const wpds::WPDS* pds) : fa_(fa), fa_witness_(fa_witness), pds_(pds) {
        InitializeWorklist();
        createPredSuccMap();
      }


      void InitializeWorklist() {
        GetAllRules gar;
        fa_->for_each(gar);
        all_rules_ = gar.get();
        worklist_ = all_rules_;
      }

      void createPredSuccMap() {
        PredSuccMapGenerator psm(all_rules_);
        fa_witness_->for_each(psm);
        pred_map_ = psm.getPredMap();
        succ_map_ = psm.getSuccMap();
      }

      virtual ~DownwardIterationSequencer() {}

      WFA* getWFA() { return fa_;}

      sem_elem_t evaluate(ITrans* t) {
        sem_elem_t t_w = t->weight();
        witness::Witness* wit = dynamic_cast<witness::Witness*>(t_w.get_ptr());
        return evaluate(wit);
      }

      sem_elem_t evaluate(witness::Witness* wit) {
        // Check if wit is a WitnessTrans, WitnessRule, WitnessExtend, WitnessCombine or WitnessMerge type
        witness::WitnessTrans* trans = dynamic_cast<witness::WitnessTrans*>(wit);
        if(trans != NULL) {
          const ITrans* t = &(trans->getTrans());
          Trans crt_trans_wfa;
          bool found = fa_->find(t->from(), t->stack(), t->to(), crt_trans_wfa);
          assert(found);
          return crt_trans_wfa.weight();
        }
 
        witness::WitnessRule* rule = dynamic_cast<witness::WitnessRule*>(wit);
        if(rule != NULL) {
          witness::RuleStub rs = rule->getRuleStub();
          return rs.weight(); 
        }

        witness::WitnessCombine* comb = dynamic_cast<witness::WitnessCombine*>(wit);
        if(comb != NULL) {
          sem_elem_t result;
          std::list<witness::witness_t>& children = comb->children();
          for(std::list<witness::witness_t>::iterator it = children.begin(); it != children.end(); it++) {
            sem_elem_t child_w = evaluate(it->get_ptr());
            if(result.is_valid())
              result = result->combine(child_w);
            else
              result = child_w;
          }
          return result;
        }

        witness::WitnessExtend* xtend = dynamic_cast<witness::WitnessExtend*>(wit);
        if(xtend != NULL) {
          sem_elem_t left = evaluate(xtend->left().get_ptr());
          sem_elem_t right = evaluate(xtend->right().get_ptr());
          sem_elem_t result = left->extend(right);
          return result;
        }

        witness::WitnessMerge* merge = dynamic_cast<witness::WitnessMerge*>(wit);
        if(merge != NULL) {
          sem_elem_t result, caller, callee, rule;
          if(merge->hasCaller())
            caller = evaluate(merge->caller().get_ptr());
          if(merge->hasCallee())
            callee = evaluate(merge->callee().get_ptr());
          if(merge->hasRule())
            rule = evaluate(merge->rule().get_ptr());

          witness::witness_merge_fn_t witness_merge_fn; // = merge->merge_fn();
          assert("Wali doesn't support this operation" && false);
          merge_fn_t merge_fn = witness_merge_fn->get_user_merge();
          result = merge_fn->apply_f(caller, callee);
          return result;
        }
        assert(false);
        return NULL;
      }


      void performIteration() {
        while(worklist_.size() != 0) {
          const ITrans* crt_trans = *(worklist_.begin());
          worklist_.erase(worklist_.begin());

          Trans crt_trans_fa_witness;
          bool found_fa_witness = fa_witness_->find(crt_trans->from(), crt_trans->stack(), crt_trans->to(), crt_trans_fa_witness);
          assert(found_fa_witness);

          Trans crt_trans_fa;
          bool found_fa = fa_->find(crt_trans->from(), crt_trans->stack(), crt_trans->to(), crt_trans_fa);
          assert(found_fa);

          sem_elem_t old_val = crt_trans_fa.weight();

          // Evaluate the new weight for crt_trans
          witness::Witness* wit_fa = dynamic_cast<witness::Witness*>(crt_trans_fa_witness.weight().get_ptr());
          sem_elem_t new_val = evaluate(wit_fa);

          if(!new_val->equal(old_val)) {
            fa_->erase(crt_trans->from(), crt_trans->stack(), crt_trans->to());
            fa_->addTrans(crt_trans->from(), crt_trans->stack(), crt_trans->to(), new_val);

            std::set<const ITrans*> succs = succ_map_[crt_trans];
            for(std::set<const ITrans*>::const_iterator it = succs.begin(); it != succs.end(); it++) {
              worklist_.insert(*it);
            }
          }
        }
      }
    }; // class DownwardIterationSequencer
  }
}

// Perform abstract interpretation on a module
void abstractInterp(std::unique_ptr<Module>& M, unsigned max_disjunctions, std::string filename) {
  std::ofstream result_file(filename + std::string(".result"));

  std::stringstream ss_timer;
  ss_timer << "AbstractInterpt_timer_maxdisj_" << max_disjunctions;
  utils::Timer ai_timer(ss_timer.str(), std::cout, true);

  // Construct WPDS for the module via reinterpretation
  Vocabulary dum_voc;
  dum_voc.insert(DimensionKey(std::string("tmp"), 0, utils::thirty_two));

  BitpreciseWrappedAbstractValue::disable_wrapping = cmdlineparam_disable_wrapping;
  // Use pointset powerset of octagon or polyhedra domain to perform analysis
  std::cout << "\nUsing the base domain of ";
  ref_ptr<AbstractValue> av; 
  if(cmdlineparam_use_red_prod) {
    if(cmdlineparam_use_oct) {
      std::cout << "ReducedProduct<octagons, Equalities>";
      PP_OCT_AV::max_disjunctions = cmdlineparam_max_disjunctions;
      PP_OCT_AV::use_extrapolation = cmdlineparam_use_extrapolation;
      PP_CPOLY_AV::max_disjunctions = 1; // equality domain is not allowed to use disjunctions
      PP_CPOLY_AV::use_extrapolation = cmdlineparam_use_extrapolation;
      av = new ReducedProductAbsVal(new PP_OCT_AV(dum_voc), new PP_CPOLY_AV(dum_voc, true, true/*equalities_only*/));
    } else {
      std::cout << "ReducedProduct not needed for polyhedra. Using polyhedra.";
      PP_CPOLY_AV::max_disjunctions = cmdlineparam_max_disjunctions;
      PP_CPOLY_AV::use_extrapolation = cmdlineparam_use_extrapolation;
      av = new PP_CPOLY_AV(dum_voc);
    }
  } else {
    if(cmdlineparam_use_oct) {
      std::cout << "octagons";
      PP_OCT_AV::max_disjunctions = cmdlineparam_max_disjunctions;
      PP_OCT_AV::use_extrapolation = cmdlineparam_use_extrapolation;
      av = new PP_OCT_AV(dum_voc);
    } else {
      std::cout << "polyhedra";
      PP_CPOLY_AV::max_disjunctions = cmdlineparam_max_disjunctions;
      PP_CPOLY_AV::use_extrapolation = cmdlineparam_use_extrapolation;
      av = new PP_CPOLY_AV(dum_voc);
    }
  }
  std::cout << " to perform analysis with max_disjunctions:" << cmdlineparam_max_disjunctions;

  utils::Timer wpds_construct_timer("wpds_construction", std::cout, false);
  llvm_abstract_transformer::WrappedDomainWPDSCreator cr(std::move(M), av, filename, cmdlineparam_array_bounds_check);
  wali::wpds::WPDS* pds = cr.createWPDS(cmdlineparam_use_fwpds);
  double wpds_construction_time = wpds_construct_timer.elapsed();
  std::cout << "\nWPDS construction time:" << wpds_construction_time;

  // Print PDS and its stats for debugging reasons
  std::cout << "WPDS Statistics:" << std::endl;
  pds->printStatistics(std::cout);
  std::cout << std::endl;

  if(debug_print_level >= DBG_PRINT_OVERVIEW) {
    std::cout << "{ # Program PDS:" << std::endl << std::flush;
    pds->print(std::cout);
    std::cout << "} # end Program PDS" << std::endl << std::flush;
  }

  wali::wfa::WFA* fa_prog = cr.BuildAutomaton(false/*is_backward*/, &std::cout);

  if(debug_print_level >= DBG_PRINT_OVERVIEW) {
    std::cout << "\nThe query Automaton is:" << std::endl;
    fa_prog->print(std::cout);
    std::cout << std::endl;
  }

  //Calculate the number of transitions in the WFA before poststar
  wali::wfa::TransCounter tf;
  fa_prog->for_each(tf);
  std::cout  << std::dec << "The number of transitions in the WFA before poststar is " << tf.getNumTrans() << std::endl;

  /********************Print inputs to putput file********************************/
  std::vector<std::pair<std::string, std::string> > input_stats_map;
  input_stats_map.push_back(std::make_pair("name", filename));

  // Collect stats on the module
  unsigned num_funcs = 0, num_bbs = 0, num_instrs = 0, num_call_sites = 0;
  for(Module::FunctionListType::iterator fit = cr.getModule()->getFunctionList().begin(); fit != cr.getModule()->getFunctionList().end(); fit++) {
    if(fit->getName() != "_VERIFIER_assert" && !cr.isModel(fit)) {
      num_funcs++;
      for(Function::BasicBlockListType::iterator bbit = fit->getBasicBlockList().begin(); bbit != fit->getBasicBlockList().end(); bbit++) {
        num_bbs++;
        for(BasicBlock::iterator iit = bbit->begin(); iit != bbit->end(); iit++) {
          num_instrs++;
        }
      }
    }
  }

  // Add stats information
  std::stringstream ss;
  ss.str(std::string()); ss << num_funcs;
  input_stats_map.push_back(std::make_pair("Num Funcs", ss.str()));

  ss.str(std::string()); ss << num_bbs;
  input_stats_map.push_back(std::make_pair("Num BBs", ss.str()));

  ss.str(std::string()); ss << num_instrs;
  input_stats_map.push_back(std::make_pair("Num Instrs", ss.str()));

  ss.str(std::string()); ss << num_call_sites;
  input_stats_map.push_back(std::make_pair("Num Calls", ss.str()));

  // Print user input information
  if(cmdlineparam_use_oct)
    input_stats_map.push_back(std::make_pair("Abstraction", "Oct"));
  else
    input_stats_map.push_back(std::make_pair("Abstraction", "Poly"));
  ss.str(std::string()); ss << cmdlineparam_max_disjunctions;
  input_stats_map.push_back(std::make_pair("Num Disjs", ss.str()));

  std::string cmdlineparam_disable_wrapping_str = "false";
  if(cmdlineparam_disable_wrapping)
    cmdlineparam_disable_wrapping_str = "true";
  input_stats_map.push_back(std::make_pair("Disable wrapping", cmdlineparam_disable_wrapping_str));

  std::string cmdlineparam_use_extrapolation_str = "false";
  if(cmdlineparam_use_extrapolation)
    cmdlineparam_use_extrapolation_str = "true";
  input_stats_map.push_back(std::make_pair("Use extrapolation", cmdlineparam_use_extrapolation_str));

  std::string cmdlineparam_perform_narrowing_str = "false";
  if(cmdlineparam_perform_narrowing)
    cmdlineparam_perform_narrowing_str = "true";
  input_stats_map.push_back(std::make_pair("Perform narrowing", cmdlineparam_perform_narrowing_str));

  std::string cmdlineparam_allow_phis_str = "false";
  if(cmdlineparam_allow_phis)
    cmdlineparam_allow_phis_str = "true";
  input_stats_map.push_back(std::make_pair("Allow phis", cmdlineparam_allow_phis_str));

  // Get vocabulary size information
  std::pair<size_t, size_t> min_max_voc_size = cr.GetMinMaxVocSize();
  ss.str(std::string()); ss << min_max_voc_size.first;
  input_stats_map.push_back(std::make_pair("Min voc size", ss.str()));
  ss.str(std::string()); ss << min_max_voc_size.second;
  input_stats_map.push_back(std::make_pair("Max voc size", ss.str()));

  // Assertion information
  std::set<wali::Key> unreachable_keys = cr.GetUnreachableKeys();
  ss.str(std::string()); ss << unreachable_keys.size();
  input_stats_map.push_back(std::make_pair("Num Assertions", ss.str()));

  std::set<wali::Key> unreachable_array_bounds_check_keys = cr.GetUnreachableArrayBoundsCheckKeys();
  if(cmdlineparam_array_bounds_check) {
    ss.str(std::string()); ss << unreachable_array_bounds_check_keys.size();
    input_stats_map.push_back(std::make_pair("Num Array Bounds Check Assertions", ss.str()));
  }

  result_file << "{";
  for(std::vector<std::pair<std::string, std::string> >::iterator it = input_stats_map.begin(); it != input_stats_map.end(); it++) {
    if(it != input_stats_map.begin())
      result_file << ", ";
    result_file << "\"" << it->first << "\":\"" << it->second << "\"";
  }
  result_file << "}\n" << std::flush;

 
  /********************************Poststar*******************************/
  utils::Timer poststar_timer("poststar", std::cout, false);
#ifdef USE_AKASH_FWPDS
  pds->poststar(*fa_prog, *fa_prog);
#else
  fa_prog = new wali::wfa::WFA(pds->poststar(*fa_prog));
#endif

  double poststar_time = poststar_timer.elapsed();

  std::cout << "poststar_time:" << poststar_time << std::endl;
  AvSemiring::av_semiring_stats_.print(std::cout << "\nSemiring stats after poststar:");

  //Calculate the number of transitions in the WFA after poststar
  wali::wfa::TransCounter tf_ps;
  fa_prog->for_each(tf_ps);
  std::cout  << std::dec << "\nThe number of transitions in the WFA after poststar is " << tf_ps.getNumTrans() << std::endl;

  if(debug_print_level >= DBG_PRINT_OVERVIEW) {
    std::cout << "\nThe poststar Automaton is:" << std::endl;
    fa_prog->print(std::cout);
  }

  /********************************Path summary*******************************/
  utils::Timer pathsummary_timer("pathsummary", std::cout, false);
  fa_prog->path_summary();
  double pathsummary_time = pathsummary_timer.elapsed();

  std::cout << "pathsummary_time:" << pathsummary_time << std::endl;
  AvSemiring::av_semiring_stats_.print(std::cout << "\nSemiring stats after pathsummary:");

  //Calculate the number of transitions in the WFA after path summary
  wali::wfa::TransCounter tf_after_path_sum;
  fa_prog->for_each(tf_after_path_sum);
  std::cout << std::dec << "The number of transitions in the WFA after path summary is " << tf_after_path_sum.getNumTrans() << std::endl;

  if(debug_print_level >= DBG_PRINT_OVERVIEW) {
    std::cout << "\nThe pathsummary Automaton is:" << std::endl;
    fa_prog->print(std::cout);
    std::cout << "\n\n";
  }

  std::cout  << std::dec << "\nThe number of transitions in the WFA after path summary is " << tf_after_path_sum.getNumTrans() << std::endl;


  /**************************Perform Downward Iteration Sequence********************/
  utils::Timer dis_timer("Downward Iteration Sequence", std::cout, false);
  if(cmdlineparam_perform_narrowing) {
    // TODO: Move the downward iteration sequence to a separate function
    // Copy wfa to a new wfa where the new wfa has witness weights
    wali::wfa::TransCopierWithWitness tcww(*fa_prog);
    fa_prog->for_each(tcww);
    wali::wfa::WFA* fa_prog_cp_witness = tcww.getWFA();

    wali::wpds::RuleCopierWithWitness rcww(pds);
    pds->for_each(rcww);
    wali::wpds::WPDS* pds_cp_witness = rcww.getWPDS();

    std::cout << "{ # Copy of program PDS with witness:" << std::endl << std::flush;
    pds_cp_witness->print(std::cout);
    std::cout << "} # end Program PDS with witness" << std::endl << std::flush;

    std::cout << "\nThe copy of pathsummary Automaton with no witnesses:" << std::endl;
    fa_prog_cp_witness->print(std::cout);
    std::cout << "\n\n";

    // Perform post star operation
    utils::Timer dis_poststar_timer("DIS Poststar", std::cout, false);
#ifdef USE_AKASH_FWPDS
    pds_cp_witness->poststar(*fa_prog_cp_witness, *fa_prog_cp_witness);
#else
    fa_prog_cp_witness = new wali::wfa::WFA(pds_cp_witness->poststar(*fa_prog_cp_witness));
#endif

    double poststar_witness_time = dis_poststar_timer.elapsed();

    std::cout << "poststar_witness_time:" << poststar_witness_time << std::endl;
    AvSemiring::av_semiring_stats_.print(std::cout << "\nSemiring stats after poststar_witness:");

    //Calculate the number of transitions in the WFA withe witnesses after poststar
    wali::wfa::TransCounter tf_ps_wit;
    fa_prog_cp_witness->for_each(tf_ps_wit);
    std::cout  << std::dec << "\nThe number of transitions in the WFA after poststar of WFA with witnesses is " << tf_ps_wit.getNumTrans() << std::endl;

    std::cout << "\nThe poststar Automaton with witnesses is:" << std::endl;
    fa_prog_cp_witness->print(std::cout);

    // Perform downward iteration sequence
    wali::wfa::DownwardIterationSequencer disq(fa_prog, fa_prog_cp_witness, pds);
    disq.performIteration();

    //Calculate the number of transitions in the WFA withe witnesses after poststar
    wali::wfa::TransCounter tf_disq_wit;
    fa_prog->for_each(tf_disq_wit);
    std::cout  << std::dec << "\nThe number of transitions in the WFA after downward iteration sequence is " << tf_disq_wit.getNumTrans() << std::endl;

    std::cout << "\nfa_prog after downward iteration sequence is:" << std::endl;
    fa_prog->print(std::cout);
  }
  double downwarditeration_time = dis_timer.elapsed();

  /***********************************Query***********************************/
  // Get the total number of proved assertions in the program in proved_unreachable_keys
  utils::Timer query_timer("Query", std::cout, false);
  std::set<wali::Key> proved_unreachable_keys;
  for(std::set<wali::Key>::const_iterator it = unreachable_keys.begin(); it != unreachable_keys.end(); it++) {
    wali::Key unreachable_key = *it;
    sem_elem_t path_sum = GetPathSummary(fa_prog, cr.GetProgramKey(), unreachable_key);
    assert(path_sum != NULL);
    if(debug_print_level >= DBG_PRINT_OVERVIEW) { 
      path_sum->print(std::cout << "\nWeight for the unreachable key " << wali::key2str(unreachable_key) << " is:\n");
    }
    if(path_sum->equal(path_sum->zero())) {
      std::cout << "\nThe unreachable key " << wali::key2str(unreachable_key) << " is proved to be unreachable by the wrapped domain analysis.";
      proved_unreachable_keys.insert(unreachable_key);
    }
  }

  std::set<wali::Key> proved_unreachable_array_bounds_check_keys;
  if(cmdlineparam_array_bounds_check) {
    for(std::set<wali::Key>::const_iterator it = unreachable_array_bounds_check_keys.begin(); it != unreachable_array_bounds_check_keys.end(); it++) {
      wali::Key unreachable_key = *it;
      sem_elem_t path_sum = GetPathSummary(fa_prog, cr.GetProgramKey(), unreachable_key);
      assert(path_sum != NULL);
      if(debug_print_level >= DBG_PRINT_OVERVIEW) { 
        path_sum->print(std::cout << "\nWeight for the unreachable key " << wali::key2str(unreachable_key) << " is:\n");
      }
      if(path_sum->equal(path_sum->zero())) {
        std::cout << "\nThe unreachable key " << wali::key2str(unreachable_key) << " for array bound checking is proved to be unreachable by the wrapped domain analysis.";
        proved_unreachable_array_bounds_check_keys.insert(unreachable_key);
      }
    }
  }

  double query_time = query_timer.elapsed();
  std::cout << "query_time:" << query_time << std::endl;
  std::cout << "\n\n\nTotal number of assertions:" << unreachable_keys.size();
  std::cout << "\nTotal number of proved assertions:" << proved_unreachable_keys.size() << "\n";

  if(cmdlineparam_array_bounds_check) {
    std::cout << "\n\n\nTotal number of array_bounds_check assertions:" << unreachable_array_bounds_check_keys.size();
    std::cout << "\nTotal number of proved array_bounds_check assertions:" << proved_unreachable_array_bounds_check_keys.size() << "\n";
  }

  wali::wpds::WpdsStackSymbols pds_stack;
  pds->for_each(pds_stack);

  num_call_sites = pds_stack.callPoints.size();

  std::vector<std::pair<std::string, std::string> > result_map;

  // Timing information
  ss.str(std::string()); ss << wpds_construction_time;
  result_map.push_back(std::make_pair("WPDS build time", ss.str()));

  ss.str(std::string()); ss << poststar_time;
  result_map.push_back(std::make_pair("post* time", ss.str()));

  ss.str(std::string()); ss << pathsummary_time;
  result_map.push_back(std::make_pair("pathsummary time", ss.str()));

  ss.str(std::string()); ss << downwarditeration_time;
  result_map.push_back(std::make_pair("downwarditeration time", ss.str()));

  ss.str(std::string()); ss << query_time;
  result_map.push_back(std::make_pair("query time", ss.str()));

  // Assertion information
  ss.str(std::string()); ss << proved_unreachable_keys.size();
  result_map.push_back(std::make_pair("Num Proved Assertions", ss.str()));

  if(cmdlineparam_array_bounds_check) {
    ss.str(std::string()); ss << proved_unreachable_array_bounds_check_keys.size();
    result_map.push_back(std::make_pair("Num Proved Array Bounds Check Assertions", ss.str()));
  }

  result_file << "{";
  for(std::vector<std::pair<std::string, std::string> >::iterator it = result_map.begin(); it != result_map.end(); it++) {
    if(it != result_map.begin())
      result_file << ", ";
    result_file << "\"" << it->first << "\":\"" << it->second << "\"";
  }
  result_file << "}";
  result_file.close();

  delete pds;
  delete fa_prog;
}

int main(int argc, char* argv[]) 
{
  int c;
  std::vector<std::string> inputs;
  std::string output = "";
  bool has_input = false;
  int option_index = 0;

  static struct option long_options[] =
    {
      /* These options set a flag. */
      {"debug_print_level", required_argument, NULL, 'd'},
      {"use_oct", no_argument, NULL, 'o'},
      {"use_red_prod", no_argument, NULL, 'r'},
      {"use_fwpds", no_argument, NULL, 'w'},
      {"filename", required_argument, NULL, 'f'},
      {"max_disjunctions", optional_argument, NULL, 'm'},
      {"disable_wrapping", no_argument, NULL, 'u'},
      {"use_extrapolation", no_argument, NULL, 'e'},
      {"perform_narrowing", no_argument, NULL, 'n'},
      {"array_bounds_check", no_argument, NULL, 'a'},
      {"allow_phis", no_argument, NULL, 'p'},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0}
    };
  while ((c = getopt_long (argc, argv, "d:or:t:s:p:m:hd:w:uc", long_options, &option_index)) != -1) {
    switch (c) {
    case 'd':
      debug_print_level = std::stoul(optarg);
      break;
    case 'o':
      cmdlineparam_use_oct = true;
      break;
    case 'r':
      cmdlineparam_use_red_prod = true;
      break;
    case 'w':
      cmdlineparam_use_fwpds = true;
      break;
    case 'f':
      cmdlineparam_filename = std::string(optarg).c_str();
      break;
    case 'm':
      cmdlineparam_max_disjunctions = std::stoul(optarg);
      break;
    case 'u':
      cmdlineparam_disable_wrapping = true;
      break;
    case 'e':
      cmdlineparam_use_extrapolation = true;
      break;
    case 'n':
      cmdlineparam_perform_narrowing = true;
      break;
    case 'a':
      cmdlineparam_array_bounds_check = true;
      break;
    case 'p':
      cmdlineparam_allow_phis = true;
      break;
    case 'h': 
      {
        std::cout << "Help on options:";
        option* crt_opt = &long_options[0];
        while(crt_opt->name) {
          std::cout << "\nName:" << crt_opt->name << ", has_arg: " << crt_opt->has_arg 
                    << ", short form: " << (char)crt_opt->val;
          crt_opt++;
        }
        std::cout << "\n";
        exit(0);
      }
      break;
    default:
      std::cout << "ERROR while parsing program argument " << (char)c << std::endl;
      return 1;
	}
  }
  if (!has_input) {
    std::cout << "ERROR while parsing program arguments" << std::endl;
  }

  std::cout << "\nParsing llvm bitcode as an llvm module.";

  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;

  // Load the input module...
  std::unique_ptr<Module> M = parseIRFile(cmdlineparam_filename, Err, Context);

  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  abstractInterp(M, cmdlineparam_max_disjunctions, cmdlineparam_filename);
  std::cout << std::flush;
  return 0;
}
