TOP_DIR=../../..
EXTERNAL_DIR=$(TOP_DIR)/external
BOOST_DIR=$(EXTERNAL_DIR)/external/boost_1_61_0

LLVM_CXX_CONFIG_NOEH=$(shell $(EXTERNAL_DIR)/bin/llvm-config --cxxflags)
LLVM_CONFIG_LD_LIBS=$(shell $(EXTERNAL_DIR)/bin/llvm-config --ldflags --libs all --system-libs)
LLVM_CXX_CONFIG=$(subst -fno-exceptions,,$(LLVM_CXX_CONFIG_NOEH))

INCLUDE_FLAGS=-I.. -I$(TOP_DIR) -I$(BOOST_DIR) -I$(EXTERNAL_DIR)/include -I$(EXTERNAL_DIR)/WALi-OpenNWA/Source $(LLVM_CXX_CONFIG) -g -D_DEBUG
CPP_FLAGS=-std=c++11
LINK_FLAGS=-lstdc++ $(LLVM_CXX_CONFIG) -L $(EXTERNAL_DIR)/lib -lz3 $(LLVM_CONFIG_LD_LIBS)

libPointsetPowersetAv.a: pointset_powerset_av.o
	gcc $(LINK_FLAGS) -shared -o $@ $^ 

pointset_powerset_av.o: pointset_powerset_av.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c pointset_powerset_av.cpp

clean: 
	-@rm *.o libPointsetPowersetAv.a 2>/dev/null || true
