
MRNET_ROOT = /afs/cs.wisc.edu/u/j/o/jolly/mrn
#MRNET_ROOT = /home/jolly/mrn
MRNET_PLATFORM = i686-pc-linux-gnu

CXX        = g++
CXXFLAGS   = -g $(INCDIR)
INCDIR     = -I$(MRNET_ROOT)/include -I$(MRNET_ROOT)/xplat/include 
LIBDIR     = $(MRNET_ROOT)/lib/${MRNET_PLATFORM}
LIBMRNET   = $(LIBDIR)/libmrnet.a
LIBXPLAT   = $(MRNET_ROOT)/xplat/libxplat.a
LIBS       = $(LIBMRNET) $(LIBXPLAT) -lfl -lm -lpthread -ldl
LDFLAGS    = -rdynamic
SOFLAGS    = -fPIC -shared -rdynamic

BA_TARGETS = BackendAttach_FE BackendAttach_BE
IA_TARGETS = IntegerAddition_FE IntegerAddition_BE IntegerAdditionFilter.so
MFT_TARGETS = MultFilTypes_FE MultFilTypes_BE MultFilTypes.so
PD_TARGETS = PerfData_FE PerfData_BE PerfDataFilter.so
TARGETS = $(BA_TARGETS) $(IA_TARGETS) $(PD_TARGETS) $(MFT_TARGETS)
PREREQS   = .set_mrnet_dir .set_platform

.PHONY: all backend-attach integer-add perf-data clean

all: .READY $(TARGETS)

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

backend-attach: $(BA_TARGETS) 

integer-add: $(IA_TARGETS)

perf-data: $(PD_TARGETS)


BackendAttach_FE: NoBackEndInstantiation/FE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ NoBackEndInstantiation/FE.C $(LIBS)

BackendAttach_BE: NoBackEndInstantiation/BE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ NoBackEndInstantiation/BE.C $(LIBS)


MultFilTypes_FE: MultFilTypes/MultFilTypes_FE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) -IMultFilTypes $(LDFLAGS) -o $@ MultFilTypes/MultFilTypes_FE.C $(LIBS)

MultFilTypes_BE: MultFilTypes/MultFilTypes_BE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) -IMultFilTypes $(LDFLAGS) -o $@ MultFilTypes/MultFilTypes_BE.C $(LIBS)

MultFilTypes.so: MultFilTypes/MultFilTypes.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ MultFilTypes/MultFilTypes.C


IntegerAddition_FE: IntegerAddition/IntegerAddition_FE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) -IIntegerAddition $(LDFLAGS) -o $@ IntegerAddition/IntegerAddition_FE.C $(LIBS)

IntegerAddition_FE_async: IntegerAddition/IntegerAddition_FE_async.C $(PREREQS)
	$(CXX) $(CXXFLAGS) -IIntegerAddition $(LDFLAGS) -o $@ IntegerAddition/IntegerAddition_FE_async.C $(LIBS)

IntegerAddition_BE: IntegerAddition/IntegerAddition_BE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) -IIntegerAddition $(LDFLAGS) -o $@ IntegerAddition/IntegerAddition_BE.C $(LIBS)

IntegerAdditionFilter.so: IntegerAddition/IntegerAdditionFilter.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ IntegerAddition/IntegerAdditionFilter.C

PerfData_FE: PerformanceData/FE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ PerformanceData/FE.C $(LIBS)

PerfData_BE: PerformanceData/BE.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ PerformanceData/BE.C $(LIBS)

PerfDataFilter.so: PerformanceData/PerfDataFilter.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ PerformanceData/PerfDataFilter.C

clean:
	/bin/rm -f *.o *~ $(TARGETS) $(PREREQS)

