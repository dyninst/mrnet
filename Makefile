#/****************************************************************************
# * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
# *                  Detailed MRNet usage rights in "LICENSE" file.          *
# ****************************************************************************/

## Creation of the top level makefile

MRNET_ROOT     = /tmp/kglaser/stat_0725_sles15_x86_64_PE-24861/stat/mrnet
MRNET_PLATFORM = x86_64-pc-linux-gnu
MRNET_STARTUP_METHOD = cray-cti

VERSION        = 5.0.1
VERSION_MAJOR  = 5
VERSION_MINOR  = 0
VERSION_REV    = 1



#############################
#### INSTALL DIRECTORIES ####
#############################

PLATDIR = /tmp/kglaser/stat_0725_sles15_x86_64_PE-24861/stat/mrnet/build/x86_64-pc-linux-gnu

all: 
	$(MAKE) -f $(PLATDIR)/Makefile all

examples:
	$(MAKE) -f $(PLATDIR)/Makefile examples

tests:
	$(MAKE) -f $(PLATDIR)/Makefile tests

install:
	$(MAKE) -f $(PLATDIR)/Makefile install

install-tests:
	$(MAKE) -f $(PLATDIR)/Makefile install-tests

install-examples:
	$(MAKE) -f $(PLATDIR)/Makefile install-examples

clean:
	$(MAKE) -f $(PLATDIR)/Makefile clean

distclean:
	@echo Cleaning all platforms ...
	for dir in conf/autom4te.cache build/* ; do \
	    if [ -d $$dir ] ; then $(RM) -r $$dir ; fi \
	done
	for file in config.log config.status  ; do \
	    if [ -e $$file ] ; then $(RM) $$file ; fi \
	done

