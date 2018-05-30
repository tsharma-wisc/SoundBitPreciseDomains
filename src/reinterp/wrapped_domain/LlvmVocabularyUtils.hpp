#ifndef src_reinterps_wrapped_domain_LlvmVocabularyUtils_hpp
#define src_reinterps_wrapped_domain_LlvmVocabularyUtils_hpp

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "src/AbstractDomain/common/dimension.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

namespace llvm_abstract_transformer {
  typedef std::map<llvm::Value*, abstract_domain::DimensionKey> value_to_dim_t;

  std::string getName(const llvm::Function* f); 
  std::string getName(const llvm::BasicBlock* bb);
  std::string getName(const llvm::Instruction* i);
  std::string getValueName(const llvm::Value* v);
  utils::Bitsize GetBitsizeFromType(const llvm::Type* t);

  bool CreateDimensionFromType(std::string name, llvm::Type* t, abstract_domain::DimensionKey& k, abstract_domain::Version ver, bool create_bool_val = true);
  bool CreateDimensionFromValue(std::string prefix, llvm::Value* v, abstract_domain::DimensionKey& k, abstract_domain::Version ver, bool create_bool_val = true);

  abstract_domain::Vocabulary CreateGlobalVocabulary(llvm::Module* m, abstract_domain::Version ver, value_to_dim_t& instr_value_to_dim);
  abstract_domain::Vocabulary CreateFunctionArgumentVocabulary(llvm::Function* f, abstract_domain::Version ver, value_to_dim_t& instr_value_to_dim);

  // Local alloca variables (variables which are created on stack via alloca)
  abstract_domain::Vocabulary CreateAllocaVocabulary(llvm::Function* f, abstract_domain::Version ver, const llvm::DataLayout& TD, std::map<llvm::Value*, std::pair<abstract_domain::DimensionKey, unsigned> >& alloca_map);  
  abstract_domain::Vocabulary CreateReturnVocabulary(llvm::Function *f, abstract_domain::Version ver);

  value_to_dim_t::const_iterator findByValue(const value_to_dim_t& vec, llvm::Value* val);
  value_to_dim_t::iterator findByValue(value_to_dim_t& vec, llvm::Value* val);
  std::ostream& printValueToDim(std::ostream& out, const value_to_dim_t& vec);

  std::ostream& printAllocaMap(std::ostream& out, const std::map<llvm::Value*, std::pair<abstract_domain::DimensionKey, unsigned> >& vec);

  bool isLoadAddrInAllocaMap
  (const std::map<llvm::Value*, std::pair<abstract_domain::DimensionKey, unsigned> >& alloca_map,
   llvm::LoadInst* LI,
   const llvm::DataLayout& TD,
   abstract_domain::DimensionKey& k);

  bool isStoreAddrInAllocaMap
  (const std::map<llvm::Value*, std::pair<abstract_domain::DimensionKey, unsigned> >& alloca_map,
   llvm::StoreInst* LI,
   const llvm::DataLayout& TD,
   abstract_domain::DimensionKey& k);

  // Capture the vocabulary used/modified in BB. This will include all kind of variables
  // including alloca, instruction, function args and global vars.
  abstract_domain::Vocabulary GetBasicBlockVocabulary
  (llvm::BasicBlock* bb, 
   llvm::BasicBlock::iterator start,
   llvm::BasicBlock::iterator end,
   const llvm::DataLayout& TD,
   const value_to_dim_t& value_to_dim, 
   const std::map<llvm::Value*, std::pair<abstract_domain::DimensionKey, unsigned> >& alloca_map);

  abstract_domain::Vocabulary GetInstructionVocabulary
  (llvm::Instruction* I,
   llvm::Function& F,
   const llvm::DataLayout& TD,
   const value_to_dim_t& value_to_dim, 
   const std::map<llvm::Value*, std::pair<abstract_domain::DimensionKey, unsigned> >& alloca_map);

  // Instruction variables (ie. local variables associated with an llvm instruction) 
  abstract_domain::Vocabulary CreateFunctionInstructionVocabulary
  (llvm::Function *f,
   const llvm::DataLayout& TD,
   value_to_dim_t& value_to_dim);
}

#endif // src_reinterps_wrapped_domain_LlvmVocabularyUtils_hpp
