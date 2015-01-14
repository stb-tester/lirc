## Process this file with automake to produce Makefile.in
## Makefile.am, (c)1999 Tom Wheeley <tw104@york.ac.uk>
ACLOCAL_AMFLAGS         = -I m4

AUTOMAKE_OPTIONS        = 1.5 check-news dist-bzip2 -Wno-portability

EXTRA_DIST              = README autogen.sh CONTRIBUTE.md ChangeLog

DISTCLEANFILES          = configure.sh  ChangeLog paths.h
CLEANFILES              = *~ remotes/*~ contrib/*~
CHANGELOG_REVS          = lirc-0_9_1..HEAD

AM_DISTCHECK_CONFIGURE_FLAGS = \
    --with-systemdsystemunitdir=$${dc_install_base}/lib/systemd/system

BUILT_SOURCES           = paths.h

if WITH_SYSTEMDSYSTEMUNITDIR
    SYSTEMD_DIR         = systemd
endif

SUBDIRS                 = lib daemons tools doc $(SYSTEMD_DIR) plugins

if INSTALL_ETC

lirc_confdir            = $(sysconfdir)/lirc
dist_lirc_conf_DATA     = lircd.conf lirc_options.conf lircmd.conf

lircdconfigdir		= $(sysconfdir)/lirc/lircd.conf.d
dist_lircdconfig_DATA	= README.conf.d

endif

pkgconfigdir            = $(libdir)/pkgconfig
dist_pkgconfig_DATA     = lirc.pc lirc-driver.pc

nobase_dist_pkgdata_DATA = $(srcdir)/configs/* $(srcdir)/contrib/*

headerdir               = $(includedir)/lirc
nobase_header_HEADERS   = include/media/lirc.h
header_HEADERS          = paths.h

install-data-hook:
	$(SED) -i -e '/^plugindir/s|/usr/lib|$(libdir)|' \
	    $(DESTDIR)$(lirc_confdir)/lirc_options.conf

dist-hook: ChangeLog
	chmod u+w $(distdir)
	find $(distdir) -name .gitkeep -o -name .gitignore -delete

paths.h: Makefile
	@echo "#ifndef PATHS_H"          >$@
	@echo "#define PATHS_H"          >>$@
	@echo '#define DATADIR          "$(datadir)"' >>$@
	@echo '#define SYSCONFDIR       "$(sysconfdir)"' >>$@
	@echo '#define VARRUNDIR        "$(localstatedir)/run"' >>$@
	@echo '#define LOCALSTATEDIR    "$(localstatedir)"' >>$@
	@echo '#define LIBDIR           "$(libdir)"' >>$@
	@echo "#endif"                  >>$@

ChangeLog: .phony
	test -s ChangeLog || ( \
	    git-tools/gitlog-to-changelog $(CHANGELOG_REVS) || \
	        echo "See http://sf.net/p/lirc/git/ci//tree/"  \
	) > ChangeLog

.phony:
