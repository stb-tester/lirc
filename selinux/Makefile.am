## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS       = -I m4
AUTOMAKE_OPTIONS      = 1.5

.NOTPARALLEL:

SEMODULES             = lircd_daemon.pp.mls lircd_daemon.pp.targeted

dist_pkgdata_DATA     = lircd_daemon.te

SELINUX_MAKEFILE      = /usr/share/selinux/devel/Makefile


all: $(SEMODULES)

$(SEMODULES): $(abs_srcdir)/lircd_daemon.te
	test -e lircd_daemon.te || cp  $(srcdir)/lircd_daemon.te .
	name=$$(a=$@; echo $${a/*.pp./}); \
	make NAME="$$name" -f $(SELINUX_MAKEFILE); \
	mv lircd_daemon.pp $@; \
	make NAME="$$name" -f $(SELINUX_MAKEFILE) clean

clean-local:
	rm -f *.pp.* lircd_daemon.if lircd_daemon.fc
	test "$(srcdir)" = "." || rm -f lircd_daemon.te
