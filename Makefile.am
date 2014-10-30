## Process this file with automake to produce Makefile.in
## Makefile.am, (c)1999 Tom Wheeley <tw104@york.ac.uk>
ACLOCAL_AMFLAGS         = -I m4

AUTOMAKE_OPTIONS        = 1.5 check-news dist-bzip2

EXTRA_DIST              = README TODO autogen.sh CONTRIBUTE.md ChangeLog

DISTCLEANFILES          = configure.sh  ChangeLog paths.h
CLEANFILES              = *~ remotes/*~ contrib/*~

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


headerdir               = $(includedir)/lirc
nobase_header_HEADERS   = include/media/lirc.h

install-data-hook:
	$(SED) -i -e '/^plugindir/s|/usr/lib|$(libdir)|' \
	    $(DESTDIR)$(lirc_confdir)/lirc_options.conf
	for f in contrib lirc.org configs; do \
	    cp -pr $(srcdir)/$$f $(DESTDIR)$(pkgdatadir); \
	done

dist-hook:
	for f in contrib lirc.org configs; do \
	    cp -pr $(srcdir)/$$f $(distdir)/$$f; done
	git log >$(distdir)/ChangeLog || :

uninstall-local:
	chmod -R a+w $(DESTDIR)$(pkgdatadir)
	rm -rf $(DESTDIR)$(pkgdatadir)

paths.h: Makefile
	@echo '#define DATADIR          "$(datadir)"' >$@
	@echo '#define SYSCONFDIR       "$(sysconfdir)"' >>$@
	@echo '#define VARRUNDIR        "$(localstatedir)/run"' >>$@
	@echo '#define LOCALSTATEDIR    "$(localstatedir)"' >>$@
	@echo '#define LIBDIR           "$(libdir)"' >>$@

ChangeLog: .phony
	test -s ChangeLog || git log > ChangeLog || \
	    echo "See http://http://sourceforge.net/p/lirc/git/ci//tree/" > \
	    ChangeLog

lirc.org: .phony
	tar czf lirc.org.tar.gz lirc.org --dereference

.phony:
