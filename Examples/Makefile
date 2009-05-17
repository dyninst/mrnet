MRNET_ROOT=/home/jolly/mrn
MRNET_PLATFORM = i686-pc-linux-gnu

CXX        = g++
CXXFLAGS   = -g $(INCDIR)
LDFLAGS    = -rdynamic
SOFLAGS    = -fPIC -shared -rdynamic

### USERS SHOULD NOT NEED TO EDIT BELOW THIS POINT ###

INCDIR     = -I$(MRNET_ROOT)/include -I$(MRNET_ROOT)/xplat/include
LIBDIR     = $(MRNET_ROOT)/lib/${MRNET_PLATFORM}

LIBMRNET   = $(LIBDIR)/libmrnet.a
LIBXPLAT   = $(LIBDIR)/libxplat.a
LIBS       = $(LIBMRNET) $(LIBXPLAT) -lfl -lm -lpthread -ldl

BA_TARGETS = BackendAttach_FE BackendAttach_BE
IA_TARGETS = IntegerAddition_FE IntegerAddition_BE IntegerAdditionFilter.so
HFT_TARGETS = HeteroTest_FE HeteroTest_BE HeteroTest.so
HF_TARGETS = HeteroFilters_FE HeteroFilters_BE HeteroFilters.so
PD_TARGETS = PerfData_FE PerfData_BE PerfDataFilter.so
TARGETS = $(BA_TARGETS) $(IA_TARGETS) $(PD_TARGETS) $(HFT_TARGETS) $(HF_TARGETS)

PREREQS   = .set_mrnet_dir .set_platform

default: all

.PHONY: all backend-attach integer-add hetero-test hetero-filter perf-data clean distclean

.READY:
    ifeq ($(MRNET_ROOT), SET_TO_YOUR_MRNET_DIR)
	@echo ' * You must set MRNET_ROOT in this Makefile to your MRNET distribution directory.'
    else
	@touch -a .set_mrnet_dir
    endif
    ifeq ($(MRNET_PLATFORM), SET_TO_YOUR_MRNET_PLATFORM)
	@echo ' * You must set MRNET_PLATFORM in this Makefile; see MRNET README for details.'
	@echo ' -- NOTE: This should be the same as MRNET_PLATFORM in your MRNET Makefile.'
    else
	@touch -a .set_platform
    endif

all: .READY $(TARGETS)

# Back-end Attach Example
backend-attach: $(PREREQS) $(BA_TARGETS) 

BackendAttach_FE: NoBackEndInstantiation/FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ NoBackEndInstantiation/FE.C $(LIBS)

BackendAttach_BE: NoBackEndInstantiation/BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ NoBackEndInstantiation/BE.C $(LIBS)

# Integer Addition Example
integer-add: $(PREREQS) $(IA_TARGETS)

IntegerAddition_FE: IntegerAddition/IntegerAddition_FE.C
	$(CXX) $(CXXFLAGS) -IIntegerAddition $(LDFLAGS) -o $@ IntegerAddition/IntegerAddition_FE.C $(LIBS)

IntegerAddition_FE_async: IntegerAddition/IntegerAddition_FE_async.C
	$(CXX) $(CXXFLAGS) -IIntegerAddition $(LDFLAGS) -o $@ IntegerAddition/IntegerAddition_FE_async.C $(LIBS)

IntegerAddition_BE: IntegerAddition/IntegerAddition_BE.C
	$(CXX) $(CXXFLAGS) -IIntegerAddition $(LDFLAGS) -o $@ IntegerAddition/IntegerAddition_BE.C $(LIBS)

IntegerAdditionFilter.so: IntegerAddition/IntegerAdditionFilter.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ IntegerAddition/IntegerAdditionFilter.C

# Heterogeneous Filters Test
hetero-test: $(HFT_TARGETS) 

HeteroTest_FE: HeterogeneousTest/HeteroTest_FE.C
	$(CXX) $(CXXFLAGS) -IHeteroTest $(LDFLAGS) -o $@ HeterogeneousTest/HeteroTest_FE.C $(LIBS)

HeteroTest_BE: HeterogeneousTest/HeteroTest_BE.C
	$(CXX) $(CXXFLAGS) -IHeteroTest $(LDFLAGS) -o $@ HeterogeneousTest/HeteroTest_BE.C $(LIBS)

HeteroTest.so: HeterogeneousTest/HeteroTest.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ HeterogeneousTest/HeteroTest.C

# Heterogeneous Filters Example
hetero-filter: $(HF_TARGETS) 

HeteroFilters_FE: HeterogeneousFilters/HeteroFilters_FE.C
	$(CXX) $(CXXFLAGS) -IHeterogeneousFilters $(LDFLAGS) -o $@ HeterogeneousFilters/HeteroFilters_FE.C $(LIBS)

HeteroFilters_BE: HeterogeneousFilters/HeteroFilters_BE.C
	$(CXX) $(CXXFLAGS) -IHeterogeneousFilters $(LDFLAGS) -o $@ HeterogeneousFilters/HeteroFilters_BE.C $(LIBS)

HeteroFilters.so: HeterogeneousFilters/HeteroFilters.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ HeterogeneousFilters/HeteroFilters.C

# Performance Data Collection Example
perf-data: $(PREREQS) $(PD_TARGETS)

PerfData_FE: PerformanceData/FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ PerformanceData/FE.C $(LIBS)

PerfData_BE: PerformanceData/BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ PerformanceData/BE.C $(LIBS)

PerfDataFilter.so: PerformanceData/PerfDataFilter.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ PerformanceData/PerfDataFilter.C

clean:
	/bin/rm -f *.o *~ $(TARGETS)

distclean: clean
	/bin/rm -f $(PREREQS)
