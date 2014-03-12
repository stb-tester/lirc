
## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS             = -I m4

AM_CPPFLAGS = -I @kerneldir@ -I$(top_srcdir)/lib -I$(top_srcdir) @X_CFLAGS@ -Wall
AM_LDFLAGS  =  -L../lib -llirc -llirc_client
LIRC_LIBS = ../lib/liblirc.la ../lib/liblirc_client.la

EXTRA_PROGRAMS =

if BUILD_XTOOLS
EXTRA_PROGRAMS += xmode2 irxevent
endif

if BUILD_SMODE2
EXTRA_PROGRAMS += smode2
endif

bin_PROGRAMS = irw irpty irexec ircat mode2 irsend lircrcd \
        @vga_progs@ @x_progs@

## simple programs
irw_SOURCES = irw.c
irpty_SOURCES = irpty.c
irpty_LDADD = @forkpty@ -lpthread $(LIRC_LIBS)
irexec_SOURCES = irexec.c
irexec_LDADD = @daemon@ $(LIRC_LIBS)
ircat_SOURCES = ircat.c
ircat_LDADD = $(LIRC_LIBS)
mode2_SOURCES = mode2.c
mode2_LDADD = $(LIRC_LIBS)
irsend_SOURCES = irsend.c
irsend_LDADD = $(LIRC_LIBS)


## vga programs
smode2_SOURCES = smode2.c
smode2_LDADD = -lvga -lvgagl $(LIRC_LIBS)

## x programs
irxevent_SOURCES = irxevent.c
irxevent_LDADD = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ $(LIRC_LIBS)
xmode2_SOURCES = xmode2.c
xmode2_LDADD =  @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ $(LIRC_LIBS)

## lircrcd

lircrcd_SOURCES = lircrcd.c
lircrcd_LDADD = $(LIRC_LIBS)


if HAVE_PYTHON
bin_SCRIPTS = pronto2lirc

pronto2lirc: $(srcdir)/pronto2lirc.py
	echo "#! ${PYTHON}" >$@
	cat $< >>$@
	chmod +x $@
endif

EXTRA_DIST = pronto2lirc.py

CLEANFILES = *~ pronto2lirc
