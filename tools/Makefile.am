## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

PYLINT                  = python3-pylint

AM_CPPFLAGS             = -I$(top_srcdir)/lib -I$(top_srcdir) @X_CFLAGS@
AM_CPPFLAGS             += -g -Wall -Wp,-D_FORTIFY_SOURCE=2
AM_LDFLAGS              =  -L../lib -llirc -llirc_client -ldl

if NEED_PYTHON3
BUILT_SOURCES           = python3
endif

LIRC_LIBS               = ../lib/liblirc.la ../lib/liblirc_client.la

EXTRA_PROGRAMS          =

if BUILD_XTOOLS
EXTRA_PROGRAMS          += xmode2 irxevent
endif

sbin_PROGRAMS           = lirc-lsplugins
bin_PROGRAMS            = irw irpty irexec ircat mode2 irsend lirc-lsremotes \
                          irtestcase irsimsend irsimreceive irrecord @x_progs@

## Simple programs
irw_SOURCES             = irw.c
irpty_SOURCES           = irpty.c
irpty_LDADD             = @forkpty@ -lpthread $(LIRC_LIBS)
irexec_SOURCES          = irexec.c
irexec_LDADD            = @daemon@ $(LIRC_LIBS)
ircat_SOURCES           = ircat.c
ircat_LDADD             = $(LIRC_LIBS)
irrecord_SOURCES        = irrecord.c
irrecord_LDADD          = $(LIRC_LIBS)
irsimsend_SOURCES       = irsimsend.c
irsimsend_LDADD         = $(LIRC_LIBS)
irsimreceive_SOURCES    = irsimreceive.c
irsimreceive_LDADD      = $(LIRC_LIBS)
mode2_SOURCES           = mode2.c
mode2_LDADD             = $(LIRC_LIBS)
irtestcase_SOURCES      = irtestcase.c
irtestcase_LDADD        = $(LIRC_LIBS)
irsend_SOURCES          = irsend.c
irsend_LDADD            = $(LIRC_LIBS)
lirc_lsplugins_SOURCES  = lirc-lsplugins.c
lirc_lsplugins_LDADD    = $(LIRC_LIBS)
lirc_lsremotes_SOURCES  = lirc-lsremotes.c
lirc_lsremotes_LDADD    = $(LIRC_LIBS)


## X programs
irxevent_SOURCES        = irxevent.c
irxevent_LDADD          = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ \
                          $(LIRC_LIBS)
xmode2_SOURCES          = xmode2.c
xmode2_LDADD            = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ \
                          $(LIRC_LIBS)

dist_bin_SCRIPTS        = lirc-config-tool


if NEED_PYTHON3
dist_bin_SCRIPTS        += python3
endif

if HAVE_PYTHON
dist_bin_SCRIPTS        += pronto2lirc irdb-get
dist_noinst_SCRIPTS     = make_rel_symlink.py check_configs.py
pkgpython_PYTHON        = lirc-setup/mvc_control.py \
                          lirc-setup/mvc_view.py \
                          lirc-setup/mvc_model.py \
                          lirc-setup/selectors.py \
                          lirc-setup/baseview.py
dist_pkgpython_SCRIPTS  = lirc-setup/lirc-setup
dist_pkgpython_DATA     = lirc-setup/lirc-setup.ui
endif

pylint: irdb-get $(pkgpython_PYTHON)
	PYTHONPATH=./lirc-setup $(PYLINT) --rcfile=pylint.conf \
	--msg-template='{path}:{line}: [{msg_id}({symbol}), {obj}] {msg}' \
	$?

pep8:   irdb-get $(pkgpython_PYTHON)
	python3-pep8 --config=pep8.conf $?

python3:
	echo "#!/bin/sh" > $@
	echo "exec $(PYTHON) \$$@" >> $@
	chmod 755 $@

install-exec-hook:
	$(SED) -i 's/@version@/$(VERSION)/' $(DESTDIR)$(bindir)/lirc-config-tool
	rm -f $(DESTDIR)/$(bindir)/lirc-setup
	$(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)/$(pkgpythondir)/lirc-setup \
	    $(DESTDIR)/$(bindir)/lirc-setup

install-data-hook:
	rm -f $(DESTDIR)/$(pkgpythondir)/configs
	$(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)$(pkgdatadir)/configs \
	    $(DESTDIR)$(pkgpythondir)/configs
