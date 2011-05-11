CURRENT_PLATFORM = `conf/config.guess`

.PHONY: all examples tests install install-tests install-examples clean distclean

all:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile all

examples:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile examples

tests:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile tests

install:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile install

install-tests:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile install-tests

install-examples:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile install-examples

clean:
	$(MAKE) -f build/$(CURRENT_PLATFORM)/Makefile clean

distclean:
	@echo Cleaning all platforms ...
	for dir in build/* ; do \
	    if [ -d $$dir ] ; then $(RM) -r $$dir ; fi \
	done
	if [ -d conf/autom4te.cache ] ; then $(RM) -r conf/autom4te.cache ; fi

