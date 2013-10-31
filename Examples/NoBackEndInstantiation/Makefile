MPICC = cc -dynamic
MPICXX = CC -dynamic
#MPICXX = mpiCC

### USERS SHOULD NOT NEED TO EDIT BELOW THIS POINT  ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/BackendAttach_FE \
          $(BINDIR)/BackendAttach_BE \
          $(BINDIR)/BackendAttach_BE_lightweight

MPI_TARGETS = $(BINDIR)/BackendAttach_mpi_BE \
              $(BINDIR)/BackendAttach_mpi_BE_lightweight

default: all

.PHONY: all mpi clean install

all: $(TARGETS)

mpi: $(MPI_TARGETS)

$(BINDIR)/BackendAttach_FE: FE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ FE.C $(LIBS)

$(BINDIR)/BackendAttach_mpi_BE: BE_mpi.C
	$(MPICXX) $(CXXFLAGS) -c -o $(OBJDIR)/BE_mpi.o BE_mpi.C
	$(MPICXX) $(LDFLAGS) -o $@ $(OBJDIR)/BE_mpi.o $(LIBS)

$(BINDIR)/BackendAttach_mpi_BE_lightweight: BE_mpi_lightweight.c
	$(MPICC) $(CCFLAGS) -c -o $(OBJDIR)/BE_mpi_lightweight.o BE_mpi_lightweight.c
	$(MPICC) $(LDFLAGS) -o $@ $(OBJDIR)/BE_mpi_lightweight.o $(LIBS_LOWFAT)

$(BINDIR)/BackendAttach_BE: BE.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ BE.C $(LIBS)

$(BINDIR)/BackendAttach_BE_lightweight: BE_lightweight.c
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ BE_lightweight.c $(LIBS_LIGHTWEIGHT)

clean:
	$(RM) -f $(TARGETS) $(MPI_TARGETS)

install: $(TARGETS)
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	done ; \
	for exe in $(MPI_TARGETS) ; do \
	    if [ -f $$exe ]; then \
	       $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	    fi ; \
	done
