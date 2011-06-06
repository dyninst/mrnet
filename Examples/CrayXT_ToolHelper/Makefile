# -- UPDATE THESE SETTINGS AS APPROPRIATE FOR YOUR ENVIRONMENT --

INSTALL_BIN  = ${HOME}/bin
INSTALL_LIB  = ${HOME}/lib

MRNET_INSTALL_PREFIX = ${HOME}
MRNET_LIBS    = -lmrnet -lxplat -lpthread -ldl 

ALPSTOOLHELPER_INC  = -I${HOME}/software/alpstoolhelp/include
ALPSTOOLHELPER_LIB  = -L/${HOME}/software/alpstoolhelp/lib/all/a
ALPSTOOLHELPER_LIBS = -lalps -lxmlrpc

# -- SHOULD NOT NEED TO EDIT BELOW --

export

CXX        = g++
CXXFLAGS   = -g -O0 \
             -I$(MRNET_INSTALL_PREFIX)/include \
             $(ALPSTOOLHELPER_INC) 

FE_LDFLAGS = -g -O0 -Wl,-E \
             -L$(MRNET_INSTALL_PREFIX)/lib \
             $(ALPSTOOLHELPER_LIB)
FE_LIBS    = $(MRNET_LIBS) $(ALPSTOOLHELPER_LIBS)

BE_LDFLAGS = -g -O0 -Wl,-E \
             -L$(MRNET_INSTALL_PREFIX)/lib \
             $(ALPSTOOLHELPER_LIB)
BE_LIBS    = $(MRNET_LIBS) $(ALPSTOOLHELPER_LIBS)

FILTER_LDFLAGS = -shared -fPIC -rdynamic
         
TOOL_FE      = xt_toolFE
TOOL_FE_SRCS = xt_toolFE.cpp xt_util.cpp
TOOL_FE_OBJS = $(TOOL_FE_SRCS:.cpp=.o)

TOOL_BE      = xt_toolBE
TOOL_BE_SRCS = xt_toolBE.cpp xt_util.cpp
TOOL_BE_OBJS = $(TOOL_BE_SRCS:.cpp=.o)

TOOL_FILTERS = xt_toolFilters.so

all: $(TOOL_FE) $(TOOL_BE) $(TOOL_FILTERS)
	$(MAKE) -C TestApp $@

$(TOOL_FE): $(TOOL_FE_OBJS)
	$(CXX) -o $@ $(FE_LDFLAGS) $^ $(FE_LIBS)

$(TOOL_BE): $(TOOL_BE_OBJS)
	$(CXX) -o $@ $(BE_LDFLAGS) $^ $(BE_LIBS)

$(TOOL_FILTERS): xt_toolFilters.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(FILTER_LDFLAGS) $^ 

install: all
	cp $(TOOL_FE) $(TOOL_BE) $(INSTALL_BIN)/
	cp $(TOOL_FILTERS) $(INSTALL_LIB)/
	$(MAKE) -C TestApp $@

clean:
	/bin/rm -f $(TOOL_FE) $(TOOL_FE_OBJS) $(TOOL_BE) $(TOOL_BE_OBJS) $(TOOL_FILTERS)
	$(MAKE) -C TestApp $@

