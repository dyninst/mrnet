### USERS SHOULD NOT NEED TO EDIT THIS FILE ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/FaultRecovery_FE \
          $(BINDIR)/FaultRecovery_BE \
          $(BINDIR)/FaultRecovery_BE_lightweight

TARGETS_SO = $(LIBDIR)/FaultRecoveryFilter.so

default: all

.PHONY: all clean install

all: $(TARGETS) $(TARGETS_SO)

$(BINDIR)/FaultRecovery_FE: FaultRecovery_FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ FaultRecovery_FE.C $(LIBS)

$(BINDIR)/FaultRecovery_BE: FaultRecovery_BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ FaultRecovery_BE.C $(LIBS)

$(BINDIR)/FaultRecovery_BE_lightweight: FaultRecovery_BE_lightweight.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ FaultRecovery_BE_lightweight.c $(LIBS_LIGHTWEIGHT)

$(LIBDIR)/FaultRecoveryFilter.so: FaultRecoveryFilter.C
	$(CXX) $(CXXFLAGS) $(SOFLAGS) -o $@ FaultRecoveryFilter.C

clean:
	$(RM) -f $(TARGETS) $(TARGETS_SO)

install: $(TARGETS) $(TARGETS_SO)
	for libso in $(TARGETS_SO) ; do \
	    $(INSTALL) -m 755 $$libso $(ILIBDIR)/ ; \
	done
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	done
