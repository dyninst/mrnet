### USERS SHOULD NOT NEED TO EDIT THIS FILE ###

# See Makefile in Examples directory for variable definitions

TARGETS = $(BINDIR)/MultThdStreams_FE \
          $(BINDIR)/MultThdStreams_BE_lightweight

TARGETS_SO = $(LIBDIR)/MultThdStreamsFilter.so

default: all

.PHONY: all clean install

all: $(TARGETS) $(TARGETS_SO)

$(BINDIR)/MultThdStreams_FE: MultThdStreams_FE.C
	$(CXX) -g $(CXXFLAGS) $(LDFLAGS) -o $@ MultThdStreams_FE.C $(LIBS)

$(BINDIR)/MultThdStreams_BE_lightweight: MultThdStreams_BE_lightweight.c
	$(CC) -g $(CCFLAGS) $(LDFLAGS) -o $@ MultThdStreams_BE_lightweight.c $(LIBS_LIGHTWEIGHT) -lpthread

$(LIBDIR)/MultThdStreamsFilter.so: MultThdStreamsFilter.C
	$(CXX) -g $(CXXFLAGS) $(SOFLAGS) -o $@ MultThdStreamsFilter.C

clean:
	$(RM) -f $(TARGETS) $(TARGETS_SO)

install: $(TARGETS) $(TARGETS_SO)
	for libso in $(TARGETS_SO) ; do \
	    $(INSTALL) -m 755 $$libso $(ILIBDIR)/ ; \
	done
	for exe in $(TARGETS) ; do \
	    $(INSTALL) -m 755 $$exe $(IBINDIR)/ ; \
	done
