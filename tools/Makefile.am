
## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS             = -I m4

AM_CPPFLAGS = -I @kerneldir@ -I$(top_srcdir)/lib -I$(top_srcdir) @X_CFLAGS@ -Wall

EXTRA_PROGRAMS = smode2 xmode2 irxevent
bin_PROGRAMS = irw irpty irexec ircat mode2 irsend lircrcd \
        @vga_progs@ @x_progs@

## simple programs
irw_SOURCES = irw.c
irpty_SOURCES = irpty.c
irpty_LDADD = @forkpty@ ../daemons/libhw_module.a @hw_module_libs@
irpty_LDFLAGS = -L../lib -llirc -llirc_client -lpthread
irexec_SOURCES = irexec.c
irexec_LDADD = @daemon@ ../daemons/libhw_module.a @hw_module_libs@
irexec_LDFLAGS = -L../lib -llirc -llirc_client
ircat_SOURCES = ircat.c
ircat_LDADD   = @hw_module_libs@ ../daemons/libhw_module.a
ircat_LDFLAGS = -L../lib -llirc -llirc_client
mode2_SOURCES = mode2.c
irsend_SOURCES = irsend.c

mode2_LDADD = ../daemons/libhw_module.a @hw_module_libs@

## vga programs
smode2_SOURCES = smode2.c
smode2_LDADD = -lvga -lvgagl ./daemons/libhw_module.a @hw_module_libs@

## x programs
irxevent_SOURCES = irxevent.c
irxevent_LDADD = @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@
irxevent_LDFLAGS = -L../lib -llirc -llirc_client ../daemons/libhw_module.a @hw_module_libs@
xmode2_SOURCES = xmode2.c
xmode2_LDADD = ../daemons/libhw_module.a @hw_module_libs@ \
        @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

## lircrcd

lircrcd_SOURCES = lircrcd.c
lircrcd_LDFLAGS = -L../lib -llirc -llirc_client ../daemons/libhw_module.a @hw_module_libs@


if HAVE_PYTHON
bin_SCRIPTS = pronto2lirc

pronto2lirc: $(srcdir)/pronto2lirc.py
	echo "#! ${PYTHON}" >$@
	cat $< >>$@
	chmod +x $@
endif

EXTRA_DIST = pronto2lirc.py

CLEANFILES = *~ pronto2lirc
