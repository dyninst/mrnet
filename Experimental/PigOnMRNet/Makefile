
#MRNET_ROOT = /home/jolly/Documents/Research/mrnet
#MRNET_PLATFORM = i686-pc-linux-gnu
MRNET_ROOT = SET_TO_YOUR_MRNET_DIR
MRNET_PLATFORM = SET_TO_YOUR_MRNET_PLATFORM

CXX        = g++
CXXFLAGS   = -g -pedantic -Wall $(INCDIR)
INCDIR     = -I$(MRNET_ROOT)/include -I$(MRNET_ROOT)/xplat/include -I$(MRNET_ROOT)/src
LIBDIR     = $(MRNET_ROOT)/lib/${MRNET_PLATFORM}
LIBMRNET   = $(LIBDIR)/libmrnet.a
LIBXPLAT   = $(MRNET_ROOT)/xplat/libxplat.a
LIBS       = $(LIBMRNET) $(LIBXPLAT) -lfl -lm -lpthread -ldl
LDFLAGS    = -rdynamic
SOFLAGS    = -fPIC -shared -rdynamic

EXEC_NAME = boar
TARGETS   = $(EXEC_NAME)_frontend $(EXEC_NAME)_backend $(EXEC_NAME)_filter.so test_types_visitor
PREREQS   = .set_mrnet_dir .set_platform

.PHONY: all clean

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

$(EXEC_NAME)_frontend: $(EXEC_NAME)_frontend.C boar.h logger.h physical_plan.o types_visitor.o $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) physical_plan.o types_visitor.o -o $@ $(EXEC_NAME)_frontend.C $(LIBS)

$(EXEC_NAME)_filter.so: $(EXEC_NAME)_filter.C logger.h physical_plan.o types_visitor.o $(PREREQS)
	$(CXX) $(CXXFLAGS) $(SOFLAGS) physical_plan.o types_visitor.o -o $@ $(EXEC_NAME)_filter.C
  
$(EXEC_NAME)_backend: $(EXEC_NAME)_backend.C boar.h message_processing.h logger.h input_reader.o types_visitor.o $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) input_reader.o -o $@ $(EXEC_NAME)_backend.C $(LIBS)

test_types_visitor: test_types_visitor.C boar.h types_visitor.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) types_visitor.o -o $@ test_types_visitor.C $(LIBS)

input_reader.o: input_reader.C input_reader.h $(PREREQS)
	$(CXX) $(CXXFLAGS) -c input_reader.C

physical_plan.o: physical_plan.C physical_plan.h physical_operator.h $(PREREQS)
	$(CXX) $(CXXFLAGS) -c physical_plan.C

types_visitor.o: types_visitor.C types_visitor.h pig_types.h $(PREREQS)
	$(CXX) $(CXXFLAGS) -c types_visitor.C

clean:
	/bin/rm -f *.txt *.o *~ $(TARGETS) $(PREREQS)

