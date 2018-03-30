#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "src/AbstractDomain/common/dimension.hpp"
#include "LlvmVocabularyUtils.hpp"

namespace {
  class LiveVariableAnalysis: public llvm::FunctionPass {
  public:
    typedef std::map<llvm::BasicBlock *, Vocabulary> BBtoVocMap;
    typedef std::map<llvm::Instruction *, Vocabulary> InstrToVocMap;
    typedef llvm_abstract_transformer::value_to_dim_t value_to_dim_t;
    typedef std::map<llvm::Value*, std::pair<DimensionKey, unsigned> > AllocaMap;

  private:
    Vocabulary fvoc, farg, falloca, fret, fins, fglobal;
    value_to_dim_t value_to_dim;
    AllocaMap alloca_map;
    BBtoVocMap liveBeforeMap;
    BBtoVocMap liveAfterMap;
    BBtoVocMap liveVarsGenMap;
    BBtoVocMap liveVarsKillMap;
    InstrToVocMap insLiveBeforeMap;
    InstrToVocMap insLiveAfterMap;

  public:
    std::map<llvm::Function*, BBtoVocMap> liveBeforeMapMap;
    std::map<llvm::Function*, BBtoVocMap> liveAfterMapMap;
    std::map<llvm::Function*, InstrToVocMap> insLiveBeforeMapMap;
    std::map<llvm::Function*, InstrToVocMap> insLiveAfterMapMap;
    std::map<llvm::Function*, AllocaMap> allocaMapMap;
    std::map<llvm::Function*, value_to_dim_t> valueToDimMapMap;

    const llvm::DataLayout* TD_;

    LiveVariableAnalysis() : FunctionPass(ID) {
    }

    void setDataLayout(const llvm::DataLayout* TD) {
      TD_ = TD;
    }

    static char ID;

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }

    virtual bool runOnFunction(llvm::Function& F) {
      value_to_dim.clear();
      alloca_map.clear();
      farg = llvm_abstract_transformer::CreateFunctionArgumentVocabulary(&F, CBTI_INT64(0), value_to_dim);
      falloca = llvm_abstract_transformer::CreateAllocaVocabulary(&F, CBTI_INT64(0), *TD_, alloca_map);
      fret = llvm_abstract_transformer::CreateReturnVocabulary(&F, CBTI_INT64(0));
      fins = llvm_abstract_transformer::CreateFunctionInstructionVocabulary(&F, *TD_, value_to_dim);
      fglobal = llvm_abstract_transformer::CreateGlobalVocabulary(F.begin()->getModule(), CBTI_INT64(0), value_to_dim);


      fvoc.clear();
      Vocabulary arg_alloca, arg_alloca_ret, arg_alloca_ret_ins;
      UnionVocabularies(farg, falloca, arg_alloca);
      UnionVocabularies(arg_alloca, fret, arg_alloca_ret);
      UnionVocabularies(arg_alloca_ret, fins, arg_alloca_ret_ins);
      UnionVocabularies(arg_alloca_ret_ins, fglobal, fvoc);

      // initialize live maps to empty
      liveBeforeMap.clear();
      liveAfterMap.clear();
      liveVarsGenMap.clear();
      liveVarsKillMap.clear();
      insLiveBeforeMap.clear();
      insLiveAfterMap.clear();

      analyzeBasicBlocksLiveVars(F);
      DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                     std::cout << "\n\nPrinting live information after analyze BB for function " << 
                     F.getName().str() << ":";
                     printLiveBeforeAndAfterInformation(std::cout););

      analyzeInstructionsLiveVars(F);

      DEBUG_PRINTING(DBG_PRINT_OVERVIEW, 
                     std::cout << "\n\nPrinting live information for function " << F.getName().str() << ":";
                     printLiveBeforeAndAfterInformation(std::cout););

      liveBeforeMapMap.insert(std::make_pair(&F, liveBeforeMap));
      liveAfterMapMap.insert(std::make_pair(&F, liveAfterMap));
      insLiveBeforeMapMap.insert(std::make_pair(&F, insLiveBeforeMap));
      insLiveAfterMapMap.insert(std::make_pair(&F, insLiveAfterMap));
      allocaMapMap.insert(std::make_pair(&F, alloca_map));
      valueToDimMapMap.insert(std::make_pair(&F, value_to_dim));
      return false;
    }

    virtual void printLiveBeforeAndAfterInformation(std::ostream& o) {
      o << "\nliveBeforeMap:";
      for(BBtoVocMap::iterator it = liveBeforeMap.begin(); it != liveBeforeMap.end(); it++) {
        llvm::BasicBlock* bb = it->first;
        o << "\n" << llvm_abstract_transformer::getName(bb) << "(" << it->second.size() << "): ";
        abstract_domain::print(o, it->second);
      }

      o << "\n\nliveAfterMap:";
      for(BBtoVocMap::iterator it = liveAfterMap.begin(); it != liveAfterMap.end(); it++) {
        llvm::BasicBlock* bb = it->first;
        o << "\n" << llvm_abstract_transformer::getName(bb) << "(" << it->second.size() << "): ";
        abstract_domain::print(o, it->second);
      }

      o << "\n\ninsLiveBeforeMap:";
      for(InstrToVocMap::iterator it = insLiveBeforeMap.begin(); it != insLiveBeforeMap.end(); it++) {
        llvm::Instruction* I = it->first;
        o << "\n" << llvm_abstract_transformer::getName(I) << "(" << it->second.size() << "): ";
        abstract_domain::print(o, it->second);
      }

      o << "\n\ninsLiveAfterMap:";
      for(InstrToVocMap::iterator it = insLiveAfterMap.begin(); it != insLiveAfterMap.end(); it++) {
        llvm::Instruction* I = it->first;
        o << "\n" << llvm_abstract_transformer::getName(I) << "(" << it->second.size() << "): ";
        abstract_domain::print(o, it->second);
      }

      llvm_abstract_transformer::printValueToDim(std::cout << "\nvalue_to_dim:", value_to_dim);
      llvm_abstract_transformer::printAllocaMap(std::cout << "\nalloca_map:", alloca_map);
    }

    //**********************************************************************
    // analyzeBasicBlocksLiveVars
    //
    // iterate over all basic blocks bb
    //    bb.gen = all upwards-exposed uses in bb
    //    bb.kill = all defs in bb
    //    put bb on the worklist
    //**********************************************************************
    void analyzeBasicBlocksLiveVars(llvm::Function &Fn) {

      // initialize all before/after/gen/kill sets and
      // put all basic blocks on the worklist
      std::set<llvm::BasicBlock *> worklist;
      for (llvm::Function::iterator MFIt = Fn.begin(), MFendIt = Fn.end(); MFIt
             != MFendIt; MFIt++) {
        liveBeforeMap[MFIt].clear();
        liveAfterMap[MFIt].clear();
        liveVarsGenMap[MFIt] = getUpwardsExposedUses(MFIt);
        liveVarsKillMap[MFIt] = getAllDefs(MFIt);
        worklist.insert(MFIt);
      }

      // while the worklist is not empty {
      //   remove one basic block bb
      //   compute new bb.liveAfter = union of liveBefore's of all successors
      //   replace old liveAfter with new one
      //   compute new bb.liveBefore = (bb.liveAfter - bb.kill) union bb.gen
      //   if bb.liveBefore changed {
      //      replace old liveBefore with new one
      //      add all of bb's predecessors to the worklist
      //   }
      // }
      while (!worklist.empty()) {
        // remove one basic block and compute its new liveAfter set
        std::set<llvm::BasicBlock *>::iterator oneBB = worklist.begin();
        llvm::BasicBlock *bb = *oneBB;
        worklist.erase(bb);

        Vocabulary newLiveAfter = computeLiveAfter(bb);

        // update the liveAfter map
        liveAfterMap.erase(bb);
        liveAfterMap[bb] = newLiveAfter;
        // compute its new liveBefore, see if it has changed (it can only
        // get bigger)
        Vocabulary newLiveBefore = computeLiveBefore(bb);
        Vocabulary oldLiveBefore = liveBeforeMap[bb];
        if (newLiveBefore.size() > oldLiveBefore.size()) {
          // update the liveBefore map and put all preds of bb on worklist
          liveBeforeMap.erase(bb);
          liveBeforeMap[bb] = newLiveBefore;
          for (llvm::pred_iterator PI = llvm::pred_begin(bb), E =
                 llvm::pred_end(bb); PI != E; PI++) {
            worklist.insert(*PI);
          }
        }
      }
    }

	// **********************************************************************
	// computeLiveBefore
	//
	// given: bb          ptr to a MachineBasicBlock
	//
	// do:    compute and return bb's current LiveBefore set:
	//          (bb.liveAfter - bb.kill) union bb.gen
	// **********************************************************************
    Vocabulary computeLiveBefore(llvm::BasicBlock *bb) {
		return vocUnion(
				vocSubtract(liveAfterMap[bb], liveVarsKillMap[bb]),
				liveVarsGenMap[bb]);
	}

	// **********************************************************************
	// computeLiveAfter
	//
	// given: bb  ptr to a MachineBasicBlock
	//
	// do:    compute and return bb's current LiveAfter set: the union
	//        of the LiveBefore sets of all of bb's CFG successors
	// **********************************************************************
    Vocabulary computeLiveAfter(llvm::BasicBlock *bb) {
      Vocabulary result;
      for (llvm::succ_iterator SI = succ_begin(bb); SI
             != succ_end(bb); SI++) {
        llvm::BasicBlock *oneSucc = *SI;
        result = vocUnion(result, liveBeforeMap[oneSucc]);
      }
      return result;
	}

    //**********************************************************************
    // analyzeInstructionsLiveVars
    //
    // do live-var analysis at the instruction level:
    //   iterate over all basic blocks
    //   for each, iterate backwards over instructions, propagating
    //             live-var info:
    //     for each instruction inst
    //             live-before = (live-after - kill) union gen
    //     where kill is the defined reg of inst (if any) and
    //           gen is all reg-use operands of inst
    //**********************************************************************
    void analyzeInstructionsLiveVars(llvm::Function &Fn) {
      for (llvm::Function::iterator bb = Fn.begin(), bbe = Fn.end(); bb
             != bbe; bb++) {
        // no reverse iterator and recursion doesn't work,
        // so create vector of instructions for backward traversal
        std::vector<llvm::Instruction *> instVector;
        for (llvm::BasicBlock::iterator inIt = bb->begin(); inIt
               != bb->end(); inIt++) {
          instVector.push_back(inIt);
        }

        liveForInstr(instVector, liveAfterMap[bb]);
      }
    }


	// **********************************************************************
	// getUpwardsExposedUses
	//
	// given: bb      ptr to a basic block
	// do:    return the vocabulary that are used before
	//        being defined in bb; include aliases!
	// **********************************************************************
    Vocabulary getUpwardsExposedUses(llvm::BasicBlock *bb) {
      Vocabulary result;
      Vocabulary defs;
      for (llvm::BasicBlock::iterator instruct = bb->begin(), instructEnd =
             bb->end(); instruct != instructEnd; instruct++) {
        Vocabulary uses = getOneInstrRegUses(instruct);
        Vocabulary upUses = vocSubtract(uses, defs);
        result = vocUnion(result, upUses);
        Vocabulary defSet = getOneInstrRegDefs(instruct);
        for (Vocabulary::iterator IT = defSet.begin(); IT
               != defSet.end(); IT++) {
          DimensionKey oneDef = *IT;
          defs.insert(oneDef);
        }
      } // end iterate over all instrutions in this basic block

      return result;
	}

	// **********************************************************************
	// getAllDefs
	//
	// given: bb      ptr to a basic block
	// do:    return the set of regs that are defined in bb
	// **********************************************************************
    Vocabulary getAllDefs(llvm::BasicBlock *bb) {
      Vocabulary result;

      // iterate over all instructions in bb
      //   for each operand that is a non-zero reg:
      //     if it is a def then add it to the result set
      // return result
      //
      for (llvm::BasicBlock::iterator instruct = bb->begin(), instructEnd =
				bb->end(); instruct != instructEnd; instruct++) {
        Vocabulary oneinstrdef = getOneInstrRegDefs(instruct);
        result = vocUnion(result, oneinstrdef);
      } // end iterate over all instrutions in this basic block
      return result;
	}

	//**********************************************************************
	// getOneInstrRegUses
	//
	// return the set of registers (virtual or physical) used by the
	// given instruction, including aliases of any physical registers
	//**********************************************************************
    Vocabulary getOneInstrRegUses(llvm::Instruction *instruct) {
      Vocabulary result;
      unsigned numOperands = instruct->getNumOperands();

      // Add global vocabulary for use in the first instruction
      llvm::Function* fn = instruct->getParent()->getParent();
      llvm::Instruction* first_inst = fn->begin()->begin();
      if(instruct == first_inst)
        result = fglobal;

      // Handle load instruction separately
      llvm::LoadInst* LI = dynamic_cast<llvm::LoadInst*>(instruct);
      if(LI) {
        DimensionKey k;
        bool isalloca = llvm_abstract_transformer::isLoadAddrInAllocaMap(alloca_map, LI, *TD_, k);
        if(isalloca)
          result.insert(k);
      } else {
        for (unsigned n = 0; n < numOperands; n++) {
          llvm::Value* MOp = instruct->getOperand(n);

          // Check if the Value MOp is in value_to_dim
          value_to_dim_t::const_iterator vec_it = llvm_abstract_transformer::findByValue(value_to_dim, MOp);
          if (vec_it != value_to_dim.end()) {
            DimensionKey k = vec_it->second;
            result.insert(k);
          }
        }
      }

      return result;
    }

	//**********************************************************************
	// getOneInstrRegDefs
	//**********************************************************************
    Vocabulary getOneInstrRegDefs(llvm::Instruction *instruct) {
      Vocabulary result;

      // Handle store instruction separately
      llvm::StoreInst* SI = dynamic_cast<llvm::StoreInst*>(instruct);
      if(SI) {
        DimensionKey k;
        bool isalloca = llvm_abstract_transformer::isStoreAddrInAllocaMap(alloca_map, SI, *TD_, k);
        if(isalloca)
          result.insert(k);
      } else {
        // Handle ret instruction
        llvm::ReturnInst* RI = dynamic_cast<llvm::ReturnInst*>(instruct);
        if(RI) {
          // For return instruction return the return vocabulary fret as the vocabulary defined by this instr
          if (instruct->getNumOperands()) {
            return fret;
          }
        } else {
          // Check if the Value instruct is in value_to_dim
          value_to_dim_t::const_iterator vec_it = llvm_abstract_transformer::findByValue(value_to_dim, instruct);
          if (vec_it != value_to_dim.end()) {
            DimensionKey k = vec_it->second;
            result.insert(k);
          }
        }
      }


      return result;
	}


	// **********************************************************************
	// liveForInstr
	//
	// given: instVector vector of ptrs to Instructions for one basic block
	//        liveAfter  live after set for the *last* instruction in the block
	//
	// do:    compute and set liveAfter and liveBefore for each instruction
	//        liveAfter = liveBefore of next instruction
	//        liveBefore = (liveAfter - kill) union gen
	// **********************************************************************
	void liveForInstr(std::vector<llvm::Instruction *> instVector,
                      Vocabulary liveAfter) {
      while (instVector.size() > 0) {
        llvm::Instruction *oneInstr = instVector.back();
        instVector.pop_back();
        insLiveAfterMap[oneInstr] = liveAfter;

        // create liveBefore for this instruction
        // (which is also liveAfter for the previous one in the block)
        //   remove the reg defined here (if any) from the set
        //   then add all used reg operands

        Vocabulary liveBefore;
        Vocabulary gen = getOneInstrRegUses(oneInstr);
        Vocabulary kill = getOneInstrRegDefs(oneInstr);
        if (kill.size() != 0) {
          liveBefore = vocUnion(vocSubtract(liveAfter, kill), gen);
        } else {
          liveBefore = vocUnion(liveAfter, gen);
        }

        // add this instruction's liveBefore set to the map
        // and prepare for the next iteration of the loop
        insLiveBeforeMap[oneInstr] = liveBefore;
        liveAfter = liveBefore;
      } // end while
	}

	// **********************************************************************
	// vocUnion
	// **********************************************************************
    Vocabulary vocUnion(const Vocabulary& S1, const Vocabulary& S2) {
      Vocabulary result;
      UnionVocabularies(S1, S2, result);
      return result;
	}

	// **********************************************************************
	// vocSubtract
	// **********************************************************************
	Vocabulary vocSubtract(const Vocabulary& S1, const Vocabulary& S2) {
      Vocabulary result;
      SubtractVocabularies(S1, S2, result);
      return result;
	}
  };
}
