### USERS SHOULD NOT NEED TO EDIT THIS FILE ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/PerfData_FE \
          $(BINDIR)/PerfData_BE

TARGETS_SO = $(LIBDIR)/PerfDataFilter.so

default: all

.PHONY: all clean install

all: $(TARGETS) $(TARGETS_SO)

$(BINDIR)/PerfData_FE: FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ FE.C $(LIBS)

$(BINDIR)/PerfData_BE: BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ BE.C $(LIBS)

$(LIBDIR)/PerfDataFilter.so: PerfDataFilter.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ PerfDataFilter.C

clean:
	$(RM) -f $(TARGETS) $(TARGETS_SO)

install: $(TARGETS) $(TARGETS_SO)
	for libso in $(TARGETS_SO) ; do \
	    $(INSTALL) -p -m 755 $$libso $(ILIBDIR)/ ; \
	done
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -p -m 755 $$exe $(IBINDIR)/ ; \
	done