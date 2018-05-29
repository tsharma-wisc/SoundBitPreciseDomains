TOP_DIR=../../..
EXTERNAL_DIR=../../../external
BOOST_DIR=../../../external/boost_1_61_0

LLVM_CXX_CONFIG_NOEH=$(shell $(EXTERNAL_DIR)/bin/llvm-config --cxxflags)
LLVM_CONFIG_LD_LIBS=$(shell $(EXTERNAL_DIR)/bin/llvm-config --ldflags --libs all --system-libs)
LLVM_CXX_CONFIG=$(subst -fno-exceptions,,$(LLVM_CXX_CONFIG_NOEH))

INCLUDE_FLAGS=-I.. -I$(TOP_DIR) -I$(BOOST_DIR) -I$(EXTERNAL_DIR)/include -I$(EXTERNAL_DIR)/WALi-OpenNWA/Source $(LLVM_CXX_CONFIG) -g -D_DEBUG
CPP_FLAGS=-std=c++11
LINK_FLAGS=-lstdc++ $(LLVM_CXX_CONFIG) -L $(EXTERNAL_DIR)/lib -lz3 $(LLVM_CONFIG_LD_LIBS)


libAbstractDomain.a: dimension.o AbstractValue.o BitpreciseWrappedAbstractValue.o AvSemiring.o
	gcc $(LINK_FLAGS) -shared -o $@ $^ 

dimension.o: dimension.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c dimension.cpp

AbstractValue.o: AbstractValue.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c AbstractValue.cpp

BitpreciseWrappedAbstractValue.o: BitpreciseWrappedAbstractValue.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c BitpreciseWrappedAbstractValue.cpp

AvSemiring.o: AvSemiring.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c AvSemiring.cpp

clean: 
	-@rm *.o libAbstractDomain.a 2>/dev/null || true
