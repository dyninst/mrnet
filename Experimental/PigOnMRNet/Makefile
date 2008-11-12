
MRNET_ROOT     = /home/jolly/mrnet/mrnet
MRNET_PLATFORM = i686-pc-linux-gnu
INCDIR         = -I$(MRNET_ROOT)/include -I$(MRNET_ROOT)/xplat/include -I$(MRNET_ROOT)/src

CXX      = g++
CXXFLAGS = -g -pedantic -Wall $(INCDIR)
LIBDIR   = $(MRNET_ROOT)/lib/${MRNET_PLATFORM}
LIBMRNET = $(LIBDIR)/libmrnet.a
LIBXPLAT = $(MRNET_ROOT)/xplat/libxplat.a
LIBS     = $(LIBMRNET) $(LIBXPLAT) -lfl -lm -lpthread -ldl
LDFLAGS  = -rdynamic
SOFLAGS  = -fPIC -shared -rdynamic

EXEC_NAME = boar
TARGETS   = $(EXEC_NAME)_frontend $(EXEC_NAME)_backend $(EXEC_NAME)_filter.so

.PHONY: all clean

all: $(TARGETS)

$(EXEC_NAME)_frontend: $(EXEC_NAME)_frontend.C boar.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(EXEC_NAME)_frontend.C $(LIBS)

$(EXEC_NAME)_filter.so: $(EXEC_NAME)_filter.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ $(EXEC_NAME)_filter.C
  
$(EXEC_NAME)_backend: $(EXEC_NAME)_backend.C boar.h message_processing.h input_reader.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) input_reader.o -o $@ $(EXEC_NAME)_backend.C $(LIBS)

input_reader.o: input_reader.C input_reader.h
	$(CXX) $(CXXFLAGS) -c input_reader.C

clean:
	/bin/rm -f *.o *~ $(TARGETS)

