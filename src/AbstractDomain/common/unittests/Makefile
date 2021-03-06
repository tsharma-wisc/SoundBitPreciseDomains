TOP_DIR=$(CURDIR)/../../../..
SRC_DIR=..
EXTERNAL_DIR=$(TOP_DIR)/external
GTEST_DIR=$(EXTERNAL_DIR)/googletest-master/googletest
BOOST_DIR=$(EXTERNAL_DIR)/boost_1_61_0

LLVM_CXX_CONFIG_NOEH=$(shell $(EXTERNAL_DIR)/bin/llvm-config --cxxflags)
LLVM_CONFIG_LD_LIBS=$(shell $(EXTERNAL_DIR)/bin/llvm-config --ldflags --libs all --system-libs)
LLVM_CXX_CONFIG=$(subst -fno-exceptions,,$(LLVM_CXX_CONFIG_NOEH))

INCLUDE_FLAGS=-g -I.. -I$(TOP_DIR) -I$(BOOST_DIR) -I$(EXTERNAL_DIR)/include $(LLVM_CXX_CONFIG) -I$(GTEST_DIR)/include
LINK_FLAGS=-lstdc++ $(LLVM_CXX_CONFIG) -L $(EXTERNAL_DIR)/lib -L $(TOP_DIR)/src/AbstractDomain/common -lAbstractDomain -L $(TOP_DIR)/utils -lUtils -L $(EXTERNAL_DIR)/WALi-OpenNWA/lib64 -lwali -lgmpxx -lppl $(LLVM_CONFIG_LD_LIBS) $(GTEST_DIR)/make/gtest-all.o

CommonUnitTests: dimension__unit_tests.o common utils
	gcc $(LINK_FLAGS) -o CommonUnitTests dimension__unit_tests.o

dimension__unit_tests.o: dimension__unit_tests.cpp
	g++ $(INCLUDE_FLAGS) -c dimension__unit_tests.cpp

utils: 
	cd $(TOP_DIR)/utils && $(MAKE)

common: 
	cd $(TOP_DIR)/src/AbstractDomain/common && $(MAKE)

objects = *.o
.PHONY: clean
clean:
	-$(MAKE) -C $(TOP_DIR)/src/AbstractDomain/common clean && $(MAKE) -C $(TOP_DIR)/utils clean && rm CommonUnitTests $(objects)
