MPICXX = mpiCC

### USERS SHOULD NOT NEED TO EDIT BELOW THIS POINT  ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/BackendAttach_FE \
          $(BINDIR)/BackendAttach_mpi_BE \
          $(BINDIR)/BackendAttach_BE

default: all

.PHONY: all clean install

all: $(TARGETS)

$(BINDIR)/BackendAttach_FE: FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ FE.C $(LIBS)

$(BINDIR)/BackendAttach_mpi_BE: BE_mpi.C
	$(MPICXX) $(CXXFLAGS) -c -o $(OBJDIR)/BE_mpi.o BE_mpi.C
	$(MPICXX) $(LDFLAGS) -o $@ $(OBJDIR)/BE_mpi.o $(LIBS)

$(BINDIR)/BackendAttach_BE: BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ BE.C $(LIBS)

clean:
	$(RM) -f $(TARGETS)

install: $(TARGETS)
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -p -m 755 $$exe $(IBINDIR)/ ; \
	done
