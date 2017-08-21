## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4
AUTOMAKE_OPTIONS        = subdir-objects

include                 ../pylint.mak

AM_CPPFLAGS             = -I$(top_builddir)/lib -I$(top_builddir) -I$(top_srcdir)/lib \
                           @X_CFLAGS@  @kernel_header_flags@
AM_CPPFLAGS             += -g -Wall -Wp,-D_FORTIFY_SOURCE=2
AM_LDFLAGS              =  -L../lib -llirc -llirc_client

CLEANFILES              =  .pylint-stamp

LIRC_LIBS               = ../lib/liblirc.la ../lib/liblirc_client.la

sbin_PROGRAMS           = lirc-lsplugins
bin_PROGRAMS            = ircat \
                          irexec \
                          irrecord \
                          irsend \
                          irsimreceive \
                          irsimsend \
                          irtestcase \
                          irw \
                          lirc-lsremotes \
                          mode2
if BUILD_XTOOLS
bin_PROGRAMS            += xmode2 irxevent
endif

if LINUX_KERNEL
bin_PROGRAMS            += irpipe
endif

bin_PROGRAMS            += irpty

## Simple programs
irw_SOURCES             = irw.cpp
irpty_SOURCES           = irpty.cpp
irpty_LDADD             = @forkpty@ -lpthread $(LIRC_LIBS)
irexec_SOURCES          = irexec.cpp
irexec_LDADD            = @daemon@ $(LIRC_LIBS)
ircat_SOURCES           = ircat.cpp
ircat_LDADD             = $(LIRC_LIBS)
irpipe_SOURCES          = irpipe.cpp
irpipe_CPPFLAGS         = -I$(top_srcdir)/include \
                          -I$(top_srcdir)/drivers/irpipe $(AM_CPPFLAGS)
irrecord_SOURCES        = irrecord-ui.cpp
irrecord_LDADD          = $(LIRC_LIBS)  ../lib/libirrecord.la
irsimsend_SOURCES       = irsimsend.cpp
irsimsend_LDADD         = $(LIRC_LIBS)
irsimreceive_SOURCES    = irsimreceive.cpp
irsimreceive_LDADD      = $(LIRC_LIBS)
mode2_SOURCES           = mode2.cpp
mode2_LDADD             = $(LIRC_LIBS)
irtestcase_SOURCES      = irtestcase.cpp
irtestcase_LDADD        = $(LIRC_LIBS)
irsend_SOURCES          = irsend.cpp
irsend_LDADD            = $(LIRC_LIBS)
lirc_lsplugins_SOURCES  = lirc-lsplugins.cpp
lirc_lsplugins_LDADD    = $(LIRC_LIBS)
lirc_lsremotes_SOURCES  = lirc-lsremotes.cpp
lirc_lsremotes_LDADD    = $(LIRC_LIBS)


## X programs
irxevent_SOURCES        = irxevent.cpp
irxevent_LDADD          = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ \
                          $(LIRC_LIBS)
xmode2_SOURCES          = xmode2.cpp
xmode2_LDADD            = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ \
                          $(LIRC_LIBS)

dist_bin_SCRIPTS        = lirc-config-tool lirc-init-db

if HAVE_DEVINPUT
dist_bin_SCRIPTS        += lirc-make-devinput
endif

dist_bin_SCRIPTS        += pronto2lirc irdb-get irtext2udp lirc-postinstall
dist_sbin_SCRIPTS       = lircd-setup
dist_noinst_SCRIPTS     = make_rel_symlink.py check_configs.py

libpython               = $(libdir)/python$(PYTHON_VERSION)

setupdir                = $(libpython)/site-packages/lirc-setup
setup_PYTHON            = lirc-setup/mvc_control.py \
                          lirc-setup/mvc_view.py \
                          lirc-setup/mvc_model.py \
                          lirc-setup/choosers.py \
                          lirc-setup/baseview.py \
                          lirc-setup/util.py
dist_setup_SCRIPTS      = lirc-setup/lirc-setup
dist_setup_DATA         = lirc-setup/lirc-setup.ui

py_sources              = irdb-get irtext2udp \
                          $(setup_PYTHON) $(dist_setup_SCRIPTS)

install-exec-hook:
	$(SED) -i -e 's/@version@/$(VERSION)/' \
	    $(DESTDIR)$(bindir)/lirc-config-tool \
	    $(DESTDIR)$(bindir)/irdb-get
	cd $(DESTDIR)$(bindir);  \
	    $(SED) -i '\|^#!/usr/bin/env|s|python3|$(PYTHON)|' \
	    irdb-get \
	    irtext2udp \
	    lirc-init-db \
	    lirc-postinstall \
	    pronto2lirc
	cd $(DESTDIR)$(sbindir);  \
	    $(SED) -i '\|^#!/usr/bin/env|s|python3|$(PYTHON)|' lircd-setup


install-data-hook:
	cd $(DESTDIR)/$(setupdir);  \
	    $(SED) -i '1s|python3[0-9\.]*|$(PYTHON)|' \
	        $(notdir $(dist_setup_SCRIPTS))
	$(PYTHON) $(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)/$(setupdir)/lirc-setup \
	    $(DESTDIR)/$(bindir)/lirc-setup
	rm -rf $(DESTDIR)/$(setupdir)/configs
	$(PYTHON) $(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)/$(pkgdatadir)/configs \
	        $(DESTDIR)/$(setupdir)/configs

uninstall-hook:
	rm -f $(DESTDIR)/$(bindir)/lirc-setup \
	      $(DESTDIR)/$(setupdir)/configs

clean-local:
	-find . -name __pycache__ -exec rm -r {} \;

checkfiles:
	../git-tools/checkfiles $(SOURCES)

pep8:   $(py_sources)
	python3-pep8 --config=../pep8.conf $?

force-pylint: .phony
	rm .pylint-stamp
	$(MAKE) pylint

pylint: .pylint-stamp
.pylint-stamp: $(py_sources)
	-PYTHONPATH=$(PYTHONPATH) $(PYLINT) --rcfile=../pylint.conf \
	--msg-template='$(pylint_template)' $? && touch $@

.phony:
