## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

AM_CPPFLAGS             = -I$(top_srcdir)/lib -I$(top_srcdir) @X_CFLAGS@ -Wall
AM_LDFLAGS              =  -L../lib -llirc -llirc_client -ldl

LIRC_LIBS               = ../lib/liblirc.la ../lib/liblirc_client.la

EXTRA_PROGRAMS          =

if BUILD_XTOOLS
EXTRA_PROGRAMS          += xmode2 irxevent
endif

sbin_PROGRAMS		= lirc-lsplugins
bin_PROGRAMS            = irw irpty irexec ircat mode2 irsend lircrcd \
                          irsimsend irrecord @x_progs@

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
mode2_SOURCES           = mode2.c
mode2_LDADD             = $(LIRC_LIBS)
irsend_SOURCES          = irsend.c
irsend_LDADD            = $(LIRC_LIBS)
lircrcd_SOURCES         = lircrcd.c
lircrcd_LDADD           = $(LIRC_LIBS)
lirc_lsplugins_SOURCES  = lirc-lsplugins.c


## X programs
irxevent_SOURCES        = irxevent.c
irxevent_LDADD          = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ \
                          $(LIRC_LIBS)
xmode2_SOURCES          = xmode2.c
xmode2_LDADD            = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ \
                          $(LIRC_LIBS)

if HAVE_PYTHON
dist_bin_SCRIPTS        = pronto2lirc lirc-setup
dist_noinst_SCRIPTS     = make_rel_symlink.py
dist_pkgdata_DATA       = lirc-setup.ui
endif


# Ugly fix until we deploy the script as a regular python thing.
install-data-hook:
	rm -f  $(DESTDIR)/$(bindir)/lirc-setup.ui
	$(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)/$(pkgdatadir)/lirc-setup.ui \
	    $(DESTDIR)/$(bindir)/lirc-setup.ui
	rm -f $(DESTDIR)/$(bindir)/configs
	$(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)/$(pkgdatadir)/configs \
	    $(DESTDIR)/$(bindir)/configs
