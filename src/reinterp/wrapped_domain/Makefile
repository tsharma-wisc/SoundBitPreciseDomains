TOP_DIR=$(CURDIR)/../../..
EXTERNAL_DIR=$(TOP_DIR)/external
BOOST_DIR=$(EXTERNAL_DIR)/boost_1_61_0

LLVM_CXX_CONFIG_NOEH=$(shell $(EXTERNAL_DIR)/bin/llvm-config --cxxflags)
LLVM_CONFIG_LD_LIBS=$(shell $(EXTERNAL_DIR)/bin/llvm-config --ldflags --libs all --system-libs)
LLVM_CXX_CONFIG=$(subst -fno-exceptions,,$(LLVM_CXX_CONFIG_NOEH))

INCLUDE_FLAGS=-I.. -I$(TOP_DIR) -I$(BOOST_DIR) -I$(EXTERNAL_DIR)/include -I$(EXTERNAL_DIR)/WALi-OpenNWA/Source $(LLVM_CXX_CONFIG) -g -D_DEBUG
CPP_FLAGS=-std=c++11
LINK_FLAGS=-lstdc++ $(LLVM_CXX_CONFIG) $(LLVM_CONFIG_LD_LIBS)


libWrappedDomainReinterp.a: LlvmVocabularyUtils.o MergeFunction.o WrappedDomainBBAbsTransCreator.o WrappedDomain_Int.o WrappedDomainWPDSCreator.o
	gcc $(LINK_FLAGS) -shared -o $@ $^ 

LlvmVocabularyUtils.o: LlvmVocabularyUtils.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c LlvmVocabularyUtils.cpp

MergeFunction.o: MergeFunction.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c MergeFunction.cpp

WrappedDomainBBAbsTransCreator.o: WrappedDomainBBAbsTransCreator.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c WrappedDomainBBAbsTransCreator.cpp

WrappedDomain_Int.o: WrappedDomain_Int.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c WrappedDomain_Int.cpp

WrappedDomainWPDSCreator.o: WrappedDomainWPDSCreator.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c WrappedDomainWPDSCreator.cpp

clean: 
	-@rm *.o libWrappedDomainReinterp.a 2>/dev/null || true
