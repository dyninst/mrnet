
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
TARGETS   = $(EXEC_NAME)_frontend $(EXEC_NAME)_backend $(EXEC_NAME)_filter.so
PREREQS   = .set_mrnet_dir .set_platform

.PHONY: all clean

all: .READY $(TARGETS)

.READY:
    ifeq ($(MRNET_ROOT), SET_TO_YOUR_MRNET_DIR)
	@rm -f .set_mrnet_dir
	@echo ' * You must set MRNET_ROOT in this Makefile to your MRNET distribution directory.'
    else
	@touch .set_mrnet_dir
    endif
    ifeq ($(MRNET_PLATFORM), SET_TO_YOUR_MRNET_PLATFORM)
	@rm -f .set_mrnet_dir
	@echo ' * You must set MRNET_PLATFORM in this Makefile; see MRNET README for details.'
	@echo ' -- NOTE: This should be the same as MRNET_PLATFORM in your MRNET Makefile.'
    else
	@touch .set_platform
    endif

$(EXEC_NAME)_frontend: $(EXEC_NAME)_frontend.C boar.h $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(EXEC_NAME)_frontend.C $(LIBS)

$(EXEC_NAME)_filter.so: $(EXEC_NAME)_filter.C $(PREREQS)
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ $(EXEC_NAME)_filter.C
  
$(EXEC_NAME)_backend: $(EXEC_NAME)_backend.C boar.h message_processing.h input_reader.o $(PREREQS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) input_reader.o -o $@ $(EXEC_NAME)_backend.C $(LIBS)

input_reader.o: input_reader.C input_reader.h $(PREREQS)
	$(CXX) $(CXXFLAGS) -c input_reader.C

clean:
	/bin/rm -f *.o *~ $(TARGETS)

