### USERS SHOULD NOT NEED TO EDIT THIS FILE ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/IntegerAddition_FE \
          $(BINDIR)/IntegerAddition_async_FE \
          $(BINDIR)/IntegerAddition_asyncStreams_FE \
          $(BINDIR)/IntegerAddition_timeout_FE \
          $(BINDIR)/IntegerAddition_concurrent_FE \
          $(BINDIR)/IntegerAddition_BE \
          $(BINDIR)/IntegerAddition_BE_lightweight		

TARGETS_SO = $(LIBDIR)/IntegerAdditionFilter.so

default: all

.PHONY: all clean install

all: $(TARGETS) $(TARGETS_SO)

$(BINDIR)/IntegerAddition_FE: IntegerAddition_FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ IntegerAddition_FE.C $(LIBS)

$(BINDIR)/IntegerAddition_async_FE: IntegerAddition_FE_async.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ IntegerAddition_FE_async.C $(LIBS)

$(BINDIR)/IntegerAddition_asyncStreams_FE: IntegerAddition_FE_asyncStreams.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ IntegerAddition_FE_asyncStreams.C $(LIBS)

$(BINDIR)/IntegerAddition_timeout_FE: IntegerAddition_FE_timeout.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ IntegerAddition_FE_timeout.C $(LIBS)

$(BINDIR)/IntegerAddition_concurrent_FE: IntegerAddition_FE_concurrent.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ IntegerAddition_FE_concurrent.C $(LIBS)

$(BINDIR)/IntegerAddition_BE: IntegerAddition_BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ IntegerAddition_BE.C $(LIBS)

$(BINDIR)/IntegerAddition_BE_lightweight: IntegerAddition_BE_lightweight.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ IntegerAddition_BE_lightweight.c $(LIBS_LIGHTWEIGHT)

$(LIBDIR)/IntegerAdditionFilter.so: IntegerAdditionFilter.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ IntegerAdditionFilter.C

clean:
	$(RM) -f $(TARGETS) $(TARGETS_SO)

install: $(TARGETS) $(TARGETS_SO)
	for libso in $(TARGETS_SO) ; do \
	    $(INSTALL) -m 755 $$libso $(ILIBDIR)/ ; \
	done
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	done
