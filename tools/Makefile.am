
## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS             = -I m4

AM_CPPFLAGS = -I @kerneldir@ -I$(top_srcdir)/lib -I$(top_srcdir) @X_CFLAGS@ -Wall

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
irpty_LDADD = @forkpty@  @hw_module_libs@
irpty_LDFLAGS = -L../lib -llirc -llirc_client -lpthread
irexec_SOURCES = irexec.c
irexec_LDADD = @daemon@  @hw_module_libs@
irexec_LDFLAGS = -L../lib -llirc -llirc_client
ircat_SOURCES = ircat.c
ircat_LDADD   = @hw_module_libs@
ircat_LDFLAGS = -L../lib -llirc -llirc_client
mode2_SOURCES = mode2.c
mode2_LDFLAGS = -L../lib -llirc -llirc_client
irsend_SOURCES = irsend.c
irsend_LDFLAGS = -L../lib -llirc -llirc_client

mode2_LDADD =  @hw_module_libs@

## vga programs
smode2_SOURCES = smode2.c
smode2_LDADD = -lvga -lvgagl @hw_module_libs@

## x programs
irxevent_SOURCES = irxevent.c
irxevent_LDADD = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@
irxevent_LDFLAGS = -L../lib -llirc -llirc_client  @hw_module_libs@
xmode2_SOURCES = xmode2.c
xmode2_LDADD = ../daemons/libhw_module.a @hw_module_libs@ \
        @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

## lircrcd

lircrcd_SOURCES = lircrcd.c
lircrcd_LDFLAGS = -L../lib -llirc -llirc_client  @hw_module_libs@


if HAVE_PYTHON
bin_SCRIPTS = pronto2lirc

pronto2lirc: $(srcdir)/pronto2lirc.py
	echo "#! ${PYTHON}" >$@
	cat $< >>$@
	chmod +x $@
endif

EXTRA_DIST = pronto2lirc.py

CLEANFILES = *~ pronto2lirc
