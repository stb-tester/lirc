## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

AM_CPPFLAGS             = -I$(top_builddir) -I$(top_builddir)/lib  \
			  -I$(top_srcdir)/lib
AM_CPPFLAGS             += -std=c++11 -Wp,-D_FORTIFY_SOURCE=2 -Wall
AM_CPPFLAGS             += @kernel_header_flags@
AM_LDFLAGS              = -lpthread $(SYSTEMD_LIBS)

CLEANFILES              = *~

varrundir               = $(localstatedir)/run

bin_PROGRAMS            = lircrcd

lircrcd_SOURCES         = lircrcd.cpp
lircrcd_LDADD           = ../lib/liblirc_client.la

sbin_PROGRAMS           = lircd lircmd

if ENABLE_UINPUT
sbin_PROGRAMS           += lircd-uinput
endif

lircd_SOURCES           = lircd.cpp
lircd_LDADD             = ../lib/liblirc.la

lircd_uinput_SOURCES    = lircd-uinput.cpp
lircd_uinput_LDADD     = ../lib/liblirc.la

lircmd_SOURCES          = lircmd.cpp
lircmd_LDADD            = ../lib/liblirc.la
lircmd_CPPFLAGS         = $(AM_CPPFLAGS) 
if ENABLE_UINPUT
lircmd_CPPFLAGS         += -DUSE_UINPUT
endif


install-exec-local:
	test -d  $(DESTDIR)$(varrundir)/$(PACKAGE) \
		 || mkdir -p $(DESTDIR)$(varrundir)/$(PACKAGE)
uninstall-local:
	-$(RM) $(DESTDIR)$(varrundir)/$(PACKAGE)/lircd

checkfiles:
	../git-tools/checkfiles $(SOURCES)
