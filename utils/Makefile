TOP_DIR=..
EXTERNAL_DIR=$(TOP_DIR)/external
BOOST_DIR=$(EXTERNAL_DIR)/boost_1_61_0

LLVM_CXX_CONFIG_NOEH=$(shell $(EXTERNAL_DIR)/bin/llvm-config --cxxflags)
LLVM_CONFIG_LD_LIBS=$(shell $(EXTERNAL_DIR)/bin/llvm-config --ldflags --libs all --system-libs)
LLVM_CXX_CONFIG=$(subst -fno-exceptions,,$(LLVM_CXX_CONFIG_NOEH))

INCLUDE_FLAGS=-I.. -I$(TOP_DIR) -I$(BOOST_DIR) -I$(EXTERNAL_DIR)/include -I$(EXTERNAL_DIR)/WALi-OpenNWA/Source $(LLVM_CXX_CONFIG) -g -D_DEBUG
CPP_FLAGS=-std=c++11
LINK_FLAGS=-lstdc++ $(LLVM_CXX_CONFIG) $(LLVM_CONFIG_LD_LIBS)


libUtils.a: DebugOptions.o gmpxx_utils.o value.o timer.o
	gcc $(LINK_FLAGS) -shared -o $@ $^ 

DebugOptions.o: debug/DebugOptions.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c debug/DebugOptions.cpp

gmpxx_utils.o: gmp/gmpxx_utils.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c gmp/gmpxx_utils.cpp

value.o: gmp/value.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c gmp/value.cpp

timer.o: timer/timer.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c timer/timer.cpp

objects = *.o
.PHONY: clean
clean: 
	-@rm libUtils.a $(objects) 2>/dev/null || true
