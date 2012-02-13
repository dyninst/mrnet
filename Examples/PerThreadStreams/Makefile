### USERS SHOULD NOT NEED TO EDIT THIS FILE ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/PerThreadStreams_FE \
		  $(BINDIR)/PerThreadStreams_BE \

ifeq ($(WANT_THREADSAFE_LTWT), yes)
    TARGETS += $(BINDIR)/PerThreadStreams_BE_lightweight
endif

TARGETS_SO = $(LIBDIR)/PerThreadStreamsFilter.so

default: all

.PHONY: all clean install

all: $(TARGETS) $(TARGETS_SO)

$(BINDIR)/PerThreadStreams_FE: PerThreadStreams_FE.C
	$(CXX) -g $(CXXFLAGS) $(LDFLAGS) -o $@ PerThreadStreams_FE.C $(LIBS)

$(BINDIR)/PerThreadStreams_BE: PerThreadStreams_BE.C
	$(CXX) -g $(CXXFLAGS) $(LDFLAGS) -o $@ PerThreadStreams_BE.C $(LIBS)

$(BINDIR)/PerThreadStreams_BE_lightweight: PerThreadStreams_BE_lightweight.c
	$(CC) -g $(CCFLAGS) $(LDFLAGS) -o $@ PerThreadStreams_BE_lightweight.c $(LIBS_LOWFAT)

$(LIBDIR)/PerThreadStreamsFilter.so: PerThreadStreamsFilter.C
	$(CXX) -g $(CXXFLAGS) $(SOFLAGS) -o $@ PerThreadStreamsFilter.C

clean:
	$(RM) -f $(TARGETS) $(TARGETS_SO)

install: $(TARGETS) $(TARGETS_SO)
	for libso in $(TARGETS_SO) ; do \
	    $(INSTALL) -m 755 $$libso $(ILIBDIR)/ ; \
	done
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	done
