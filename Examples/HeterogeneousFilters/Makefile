### USERS SHOULD NOT NEED TO EDIT THIS FILE ###

# See Makefile in Examples directory for variable definitions


TARGETS = $(BINDIR)/HeteroFilters_FE \
          $(BINDIR)/HeteroFilters_BE

TARGETS_SO = $(LIBDIR)/HeteroFilters.so

default: all

.PHONY: all clean install

all: $(TARGETS) $(TARGETS_SO)

$(BINDIR)/HeteroFilters_FE: HeteroFilters_FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ HeteroFilters_FE.C $(LIBS)

$(BINDIR)/HeteroFilters_BE: HeteroFilters_BE.C file_util.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ HeteroFilters_BE.C $(LIBS)

$(LIBDIR)/HeteroFilters.so: HeteroFilters.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ HeteroFilters.C

clean:
	$(RM) -f $(TARGETS) $(TARGETS_SO)

install: $(TARGETS) $(TARGETS_SO)
	for libso in $(TARGETS_SO) ; do \
	    $(INSTALL) -m 755 $$libso $(ILIBDIR)/ ; \
	done
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	done
