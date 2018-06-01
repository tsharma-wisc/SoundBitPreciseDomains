TOP_DIR=$(CURDIR)/../..
EXTERNAL_DIR=$(TOP_DIR)/external
BOOST_DIR=$(EXTERNAL_DIR)/boost_1_61_0

LLVM_CXX_CONFIG_NOEH=$(shell $(EXTERNAL_DIR)/bin/llvm-config --cxxflags)
LLVM_CONFIG_LD_LIBS=$(shell $(EXTERNAL_DIR)/bin/llvm-config --ldflags --libs all --system-libs)
LLVM_CXX_CONFIG=$(subst -fno-exceptions,,$(LLVM_CXX_CONFIG_NOEH))

INCLUDE_FLAGS=-I.. -I$(TOP_DIR) -I$(BOOST_DIR) -I$(EXTERNAL_DIR)/include -I$(EXTERNAL_DIR)/WALi-OpenNWA/Source $(LLVM_CXX_CONFIG) -g -D_DEBUG
CPP_FLAGS=-std=c++11
LINK_FLAGS=-lstdc++ $(LLVM_CXX_CONFIG) -L $(EXTERNAL_DIR)/lib -lz3 -L $(TOP_DIR)/src/AbstractDomain/common -lAbstractDomain -L $(TOP_DIR)/src/AbstractDomain/PointsetPowerset -lPointsetPowersetAv -L $(TOP_DIR)/src/reinterp/wrapped_domain -lWrappedDomainReinterp -L $(TOP_DIR)/utils -lUtils -L $(EXTERNAL_DIR)/WALi-OpenNWA/lib64 -lwali -lgmpxx -lgmp -lppl $(LLVM_CONFIG_LD_LIBS)


bvsfdAnalysis: analysis.o analysis__cmdline_options.o utils common PointsetPowerset reinterp
	gcc -o $@ analysis.o analysis__cmdline_options.o $(LINK_FLAGS)

analysis.o: analysis.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c analysis.cpp

analysis__cmdline_options.o: analysis__cmdline_options.cpp
	g++ $(CPP_FLAGS) $(INCLUDE_FLAGS) -c analysis__cmdline_options.cpp

utils: 
	$(MAKE) -C $(TOP_DIR)/utils

common: 
	$(MAKE) -C $(TOP_DIR)/src/AbstractDomain/common

PointsetPowerset: 
	$(MAKE) -C $(TOP_DIR)/src/AbstractDomain/PointsetPowerset

reinterp: 
	$(MAKE) -C $(TOP_DIR)/src/reinterp/wrapped_domain

objects = *.o
.PHONY: clean
clean:
	-$(MAKE) -C $(TOP_DIR)/src/AbstractDomain/common clean && $(MAKE) -C $(TOP_DIR)/src/AbstractDomain/PointsetPowerset clean && $(MAKE) -C $(TOP_DIR)/utils clean && $(MAKE) -C $(TOP_DIR)/src/reinterp/wrapped_domain clean && rm bvsfdAnalysis $(objects)
