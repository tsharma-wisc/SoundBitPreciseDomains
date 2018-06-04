#include "LlvmVocabularyUtils.hpp"

#include "llvm/ADT/APInt.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"

#include "utils/timer/timer.hpp"
#include "utils/debug/DebugOptions.hpp"
#include "llvm/IR/CFG.h"
#include <sstream>

extern bool cmdlineparam_allow_phis;
using namespace utils;
using namespace abstract_domain;

namespace llvm_abstract_transformer {

  std::string getName(const llvm::Function* f) {
    std::string name = f->getName().str();
    if(name == std::string("")) {
      llvm::raw_string_ostream rso(name);
      f->print(rso);
    }
    return name;
  }

  std::string getName(const llvm::BasicBlock* bb) {
    std::string name = bb->getName().str();
    if(name == std::string("")) {
      llvm::raw_string_ostream rso(name);
      bb->print(rso);
    }
    return name;
  }

  std::string getName(const llvm::Instruction* i) {
    std::string name = i->getName().str();
    if(name == std::string("")) {
      llvm::raw_string_ostream rso(name);
      i->print(rso);
    }
    return name;
  }

  std::string getValueName(const llvm::Value* v) {
    std::string name = v->getName().str();
    if(name == std::string("")) {
      llvm::raw_string_ostream rso(name);
      v->print(rso);
    }
    return name;
  }

  // Returns 32-bitsize if t is not a 1, 8, 16, 32 or 64 bitsize variable
  utils::Bitsize GetBitsizeFromType(const llvm::Type* t) {
    switch(t->getTypeID()) {
    case llvm::Type::VectorTyID:
      // Cannot handle vector argument
      return utils::thirty_two;
    case llvm::Type::VoidTyID:
    case llvm::Type::StructTyID:
    case llvm::Type::ArrayTyID:
    case llvm::Type::X86_FP80TyID:
    case llvm::Type::FP128TyID:
    case llvm::Type::PPC_FP128TyID:
      // Cannot handle any of these types
      return utils::thirty_two;
      // These are the things we can handle
    case llvm::Type::HalfTyID:
    case llvm::Type::IntegerTyID: {
      unsigned bitsize = t->getPrimitiveSizeInBits();
      switch(bitsize) {
      case 64: {
        return utils::sixty_four;
      }
        break;
      case 32: {
        return utils::thirty_two;
      }
        break;
      case 16: {
        return utils::sixteen;
      }
        break;
      case 8: {
        return utils::eight;
      }
        break;
      case 1:
        return utils::one;
        break;
      default:
        // Float type not handled
        return utils::thirty_two;
      }
    }
      break;
    case llvm::Type::FloatTyID:
    case llvm::Type::DoubleTyID:
    case llvm::Type::PointerTyID:
      return utils::thirty_two;
    default:
      assert(false && "Do not recognize type.");
      return utils::thirty_two;
    }
    return utils::thirty_two;
  }

  // Returns true, if it can convert v to a DimensionKey 
  // (for example, it will return true for 32-bit or 64-bit ints, but not for a 
  // float.) Else returns false prefix: Contains a string used to give prefix 
  // to the name of the symbolic value in k
  bool CreateDimensionFromType(std::string name, llvm::Type* t, DimensionKey& k, Version ver, bool create_bool_val) {
    //llvm::Type* sc_t;
    switch(t->getTypeID()) {
    case llvm::Type::VectorTyID:
      // Cannot handle vector argument
      return false;
      /*sc_t = v->getScalarType();
        for (unsigned i = 0; i < argtype->getVectorNumElements(); ++i) {
        }*/
      break;
    case llvm::Type::VoidTyID:
    case llvm::Type::StructTyID:
    case llvm::Type::ArrayTyID:
    case llvm::Type::X86_FP80TyID:
    case llvm::Type::FP128TyID:
    case llvm::Type::PPC_FP128TyID:
      // Cannot handle any of these types
      return false;
      break;
      // These are the things we can handle
    case llvm::Type::HalfTyID:
    case llvm::Type::IntegerTyID: 
      {
        unsigned numbits = t->getPrimitiveSizeInBits();
        utils::Bitsize bitsize = utils::GetBitsize(numbits);
        k = abstract_domain::DimensionKey(name, ver, bitsize);
        return true;
      }
      break;
    case llvm::Type::FloatTyID:
      // Float type not handled
      return false;
      break;
    case llvm::Type::DoubleTyID:
      // Double type not handled
      return false;
      break;
    case llvm::Type::PointerTyID:
      // Pointer types not handled
      return false;
      break;
    default:
      assert(false && "Do not recognize type.");
      return false;
    }
    return false;
  }

  bool CreateDimensionFromValue(std::string prefix, 
                                llvm::Value* v, 
                                DimensionKey& k, 
                                Version ver, 
                                bool create_bool_val) {
    std::string name = prefix + std::string("_") + getValueName(v);
    llvm::Type* t = v->getType();
    bool b = CreateDimensionFromType(name, t, k, ver, create_bool_val);
    return b;
  }

  Vocabulary CreateGlobalVocabulary(llvm::Module* m,
                                    Version ver, 
                                    value_to_dim_t& instr_value_to_dim) {
    Vocabulary v;
    for(llvm::Module::global_iterator it = m->global_begin(); it != m->global_end(); it++) {
      DimensionKey k;
      bool b = CreateDimensionFromValue(std::string("global"), it, k, ver);
      if(b) {
        v.insert(k);
        llvm::Value* val = it;
        instr_value_to_dim.insert(std::make_pair(val, k));
      }
    }
    return v;
  }

  Vocabulary CreateFunctionArgumentVocabulary(llvm::Function* f, 
                                              Version ver, 
                                              value_to_dim_t& instr_value_to_dim) {
    Vocabulary v;
    for(llvm::Function::arg_iterator argit = f->arg_begin(); argit != f->arg_end(); argit++) {
      std::stringstream ss;
      ss << argit->getArgNo();
      std::string argprefix = getName(f) + std::string("_") + std::string("arg") + ss.str();

      DimensionKey k;
      bool b = CreateDimensionFromValue(argprefix, argit, k, ver);
      if(b) {
        v.insert(k);
        llvm::Value* val = argit;
        instr_value_to_dim.insert(std::make_pair(val, k));
      }
    }
    return v;
  }

  // Local alloca variables (variables which are created on stack via alloca)
  Vocabulary CreateAllocaVocabulary(llvm::Function* f, Version ver, const llvm::DataLayout& TD, std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map) {
    Vocabulary v;
    alloca_map.clear();
    for(llvm::Function::BasicBlockListType::iterator bbit = f->getBasicBlockList().begin(); bbit != f->getBasicBlockList().end(); bbit++) {
      llvm::BasicBlock* bb = bbit;
      for(llvm::BasicBlock::iterator inst_it = bb->begin(); inst_it != bb->end(); inst_it++) {
        llvm::Instruction* I = inst_it;
        llvm::AllocaInst* ai = dynamic_cast<llvm::AllocaInst*>(I);
        if(ai) {
          // Type to be allocated          
          llvm::Type *Ty = ai->getType()->getElementType();  
          unsigned TypeSize = (size_t)TD.getTypeAllocSize(Ty);

          // Get the number of elements being allocated by the alloca instruction
          // The bool method specifies if the size is constant, if not then we 
          // cannot handle it
          bool is_const = false;
          llvm::APInt NumElements;
          if(llvm::ConstantInt* C = dynamic_cast<llvm::ConstantInt*>(ai->getOperand(0))) {
            if(C) {
              NumElements = C->getValue();
              is_const = true;
            }
          }

          // If the number of elements is constant and equal to 1, and 
          // TypeSize is 32, then add k to alloca_map,
          // else ignore it, domain cannot handle it
          if(is_const && NumElements == 1) {
            std::string name = getName(f) + std::string("a_") + getName(I);
            DimensionKey k, k_addr;
            bool is_dim = CreateDimensionFromType(name, Ty, k, ver);
            if(is_dim) {
              k_addr = k; //k is symbolic, hence set k_addr to k
            }
            alloca_map.insert(std::pair<llvm::Value*, std::pair<DimensionKey, unsigned> >(I, std::make_pair(k_addr, TypeSize))); 
            v.insert(k);
          }
        }
      }
    }
    return v;
  }

  Vocabulary CreateReturnVocabulary(llvm::Function *f, Version ver) {
    Vocabulary v;
    llvm::Type* t = f->getReturnType();
    DimensionKey k;
    bool b = CreateDimensionFromType(std::string("return_") + getName(f), t, k, ver);
    if(b)
      v.insert(k);
    return v;
  }

  value_to_dim_t::const_iterator findByValue(const value_to_dim_t& vec, llvm::Value* val) {
    value_to_dim_t::const_iterator it = vec.find(val);
    return it;
  }

  value_to_dim_t::iterator findByValue(value_to_dim_t& vec, llvm::Value* val) {
    value_to_dim_t::iterator it = vec.find(val);
    return it;
  }

  std::ostream& printValueToDim(std::ostream& out, const value_to_dim_t& vec) {
    for(value_to_dim_t::const_iterator it = vec.begin(); it != vec.end(); it++) {
      out << "(" << getValueName(it->first) << ", ";
      abstract_domain::print(out, it->second) << ") ; ";
    }
    return out;
  }

  std::ostream& printAllocaMap(std::ostream& out, const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& m) {
    for(std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >::const_iterator it = m.begin(); it != m.end(); it++) {
      out << "(" << getValueName(it->first) << ", (";
      abstract_domain::print(out, it->second.first) << "), " << it->second.second << ") ; ";
    }
    return out;
  }

  // Check if LI uses an addr defined in alloca_map, if yes, then return true and
  // set k to the corresponding dimension
  bool isLoadAddrInAllocaMap
  (const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map,
   llvm::LoadInst* LI,
   const llvm::DataLayout& TD,
   DimensionKey& k) {
    llvm::Value* v = LI->getPointerOperand();
    std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >::const_iterator it = 
      alloca_map.find(v);
    if(it != alloca_map.end()) {
      llvm::Type* Ty = LI->getType();
      const unsigned LoadBytes = TD.getTypeStoreSize(Ty);
      if(LoadBytes ==  it->second.second) {
        k = it->second.first;
        return true;
      }
    } 
    return false;
  }

  // Check if SI uses an addr defined in alloca_map, if yes, then return true and
  // set k to the corresponding dimension
  bool isStoreAddrInAllocaMap
  (const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map,
   llvm::StoreInst* SI,
   const llvm::DataLayout& TD,
   DimensionKey& k) {
    llvm::Value* PtrDest = SI->getPointerOperand();
    std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >::const_iterator it = alloca_map.find(PtrDest);
    if(it != alloca_map.end()) {
      llvm::Type* Ty = SI->getOperand(0)->getType();
      const unsigned StoreBytes = TD.getTypeStoreSize(Ty);

      if(StoreBytes ==  it->second.second) {
        k = it->second.first;
        return true;
      }
    } 
    return false;
  }

  // Instruction variables (ie. local variables associated with an llvm instruction) or 
  // alloca and argument used/defined in the basic block
  Vocabulary GetBasicBlockVocabulary
  (llvm::BasicBlock* bb,
   llvm::BasicBlock::iterator start,
   llvm::BasicBlock::iterator end,
   const llvm::DataLayout& TD,
   const value_to_dim_t& value_to_dim, 
   const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map) {
    DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                   std::cout << "In CreateBasicBlockVocabulary:" << std::endl;
                   printValueToDim(std::cout << "\nvalue_to_dim:", value_to_dim););
    Vocabulary v;

    for(llvm::BasicBlock::iterator inst_it = start; inst_it != end; inst_it++) {
      llvm::Instruction *I = inst_it;
      Vocabulary I_voc = GetInstructionVocabulary(I, *(bb->getParent()), TD, value_to_dim, alloca_map);
      v.insert(I_voc.begin(), I_voc.end());
    }
    return v;
  }

  // Instruction variables (ie. local variables associated with an llvm instruction) or 
  // alloca and argument used/defined in the basic blocko
  Vocabulary GetInstructionVocabulary
  (llvm::Instruction* I,
   llvm::Function& F,
   const llvm::DataLayout& TD,
   const value_to_dim_t& value_to_dim, 
   const std::map<llvm::Value*, std::pair<DimensionKey, unsigned> >& alloca_map) {
    DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                   std::cout << "In CreateInstructionVocabulary:" << std::endl;
                   printValueToDim(std::cout << "\nvalue_to_dim:", value_to_dim););
    Vocabulary v;

    // Ignore alloca instructions
    if(I->getOpcode() == llvm::Instruction::Alloca)
      return v;

    // Handle other kind of instructions
    llvm::Value* val_I = I;
    value_to_dim_t::const_iterator vec_it = findByValue(value_to_dim, val_I);
    if (vec_it != value_to_dim.end()) {
      // Bool vals are precisely handled by bool stores, no need to add them in BB vocabulary
      DimensionKey k = vec_it->second;
      if(k.GetBitsize() != utils::one) {
        v.insert(abstract_domain::replaceVersion(k, 0, 1));
      }
    }

    // Handle load/store by looking them up in alloca_map and if present add them to v
    // Handle load instruction separately
    llvm::LoadInst* LI = dynamic_cast<llvm::LoadInst*>(I);
    if(LI) {
      DimensionKey k;
      bool isalloca = isLoadAddrInAllocaMap(alloca_map, LI, TD, k);
      if(isalloca) {
        v.insert(abstract_domain::replaceVersion(k, 0, 1));
      }
    } else {
      llvm::StoreInst* SI = dynamic_cast<llvm::StoreInst*>(I);
      if(SI) {
        DimensionKey k;
        bool isalloca = isStoreAddrInAllocaMap(alloca_map, SI, TD, k);
        if(isalloca)
          v.insert(abstract_domain::replaceVersion(k, 0, 1));
      }
    }

    // Handle the operands to see if any of them are in value_to_dim map,
    // if yes then add them to v
    for (unsigned n = 0; n < I->getNumOperands(); n++) {
      llvm::Value* MOp = I->getOperand(n);

      // Check if the Value MOp is in value_to_dim
      value_to_dim_t::const_iterator vec_it = findByValue(value_to_dim, MOp);
      if (vec_it != value_to_dim.end()) {
        DimensionKey k = vec_it->second;
        v.insert(k);
        v.insert(abstract_domain::replaceVersion(k, 0, 1));
        DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                       abstract_domain::print(std::cout << "\nAdding k and k' to vocabulary as it is used in " << getName(I) << ":", k););
      }
    }
    return v;
  }

  std::map<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>, int> succ_star_cache;
  bool isSuccessorStar(llvm::BasicBlock* b, llvm::BasicBlock* possible_succ_star) {
    std::pair<llvm::BasicBlock*, llvm::BasicBlock*> bb_pair(b, possible_succ_star);
    std::map<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>, int>::iterator ssm_it = 
      succ_star_cache.find(bb_pair);
    if(ssm_it != succ_star_cache.end())
      return ssm_it->second;

    // -1 means this pair is being calculated, this is crucial to avoid loops
    int result = -1;
    succ_star_cache[bb_pair] = result;

    if(b == possible_succ_star) {
      result = 1;
    } else {
      for(llvm::succ_iterator it = llvm::succ_begin(b); it != llvm::succ_end(b); it++) {
        llvm::BasicBlock* succ = *it;
        if(succ == b)
          continue;

        int succ_result = isSuccessorStar(succ, possible_succ_star);
        if(succ_result == -1) {
          // we have come a circle with encountering possible_succ_star, so ignore
          continue;
        }

        if(succ_result == 1) {
          result = 1;
          break;
        }
      }
    }

    if(result == -1)
      result = 0;

    succ_star_cache[bb_pair] = result;
    return result;
  }

  void SetValueKeyVersionToZero(llvm::Value* val, value_to_dim_t& value_to_dim, Vocabulary& v) {
    value_to_dim_t::const_iterator vec_it = findByValue(value_to_dim, val);
    if (vec_it != value_to_dim.end()) {
      DimensionKey k = vec_it->second;
      if(isVersionInDimension(k, UNVERSIONED_VERSION)) {
        std::cout << "\n\nNote: This value " << getValueName(val) << " is a variable that can be ";
        std::cout << "defined after use in a BB as it is used in a PHI node with self-loop. ";
        std::cout << "For soundness, making this vocabulary a non-versioned vocabulary.";
        DimensionKey k_0 = replaceVersion(k, UNVERSIONED_VERSION, 0);
        v.erase(k);
        v.insert(k_0);
        value_to_dim[val] = k_0;
      }
    }
  }

  // Instruction variables (ie. local variables associated with an llvm instruction) 
  Vocabulary CreateFunctionInstructionVocabulary
  (llvm::Function *f,
   const llvm::DataLayout& TD,
   value_to_dim_t& value_to_dim) {
    DEBUG_PRINTING(DBG_PRINT_DETAILS, 
                   std::cout << "In CreateInstructionVocabulary:" << std::endl;
                   printValueToDim(std::cout << "\nvalue_to_dim:", value_to_dim););
    Vocabulary v;

    // In absense of phis, the instruction voc can be unversioned as inst vars 
    // are not live across BBs. This facilitates in more improvement
    Version ver = UNVERSIONED_VERSION;

    for(llvm::Function::iterator bb_it = f->begin(); bb_it != f->end(); bb_it++) {
      llvm::BasicBlock* bb = bb_it;
      for(llvm::BasicBlock::iterator inst_it = bb->begin(); inst_it != bb->end(); inst_it++) {
        llvm::Instruction *I = inst_it;
        // Ignore alloca instructions
        if(I->getOpcode() == llvm::Instruction::Alloca)
          continue;
        
        //Handle other kind of instructions
        llvm::Value* val_I = I;
        DimensionKey k;

        // Do not create BB values for bool values in a basic block
        bool b = CreateDimensionFromValue(getName(bb->getParent()) + std::string("_inst"), val_I, k, ver);
        if(b) {
          v.insert(k);
          llvm::Value* val = val_I;
          value_to_dim.insert(std::make_pair(val, k));
        }
      }
    }

    // Specially handle those values that might be used before define via a 
    // phi instruction
    for(llvm::Function::iterator bb_it = f->begin(); bb_it != f->end(); bb_it++) {
      llvm::BasicBlock* bb = bb_it;
      for(llvm::BasicBlock::iterator inst_it = bb->begin(); inst_it != bb->end(); inst_it++) {
        llvm::Instruction *I = inst_it;
        llvm::PHINode* PN = dynamic_cast<llvm::PHINode*>(I);
        if(PN) {
          bool possible_cyclic_value_dependency = false;
          for(unsigned i = 0; i < PN->getNumIncomingValues(); i++) {
            // Find the values in phi which comes from self loop
            llvm::BasicBlock* incoming_bb = PN->getIncomingBlock(i);
          
            // Check if the incoming value might flow froma define in bb by seeing if bb --succ*--> incoming_bb
            // relationship is true. This will be true in case bb and incoming_bb are in some loop
            if(isSuccessorStar(bb, incoming_bb)) {
              // Value PN->getIncomingValue(i) comes back from the same BB
              // for soundness change the value_to_dim entry for this value with a Version 0
              // as this value is no longer unversioned for BB abstract transformation purpose
              possible_cyclic_value_dependency = true;
              llvm::Value* val_i = PN->getIncomingValue(i);
              SetValueKeyVersionToZero(val_i, value_to_dim, v);
            }
          }

          if(possible_cyclic_value_dependency) {
            SetValueKeyVersionToZero(I, value_to_dim, v);
          }
        }
      }
    }
    
    return v;
  }
}
