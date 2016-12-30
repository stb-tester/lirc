## Process this file with automake to produce Makefile.in
## Makefile.am, (c)1999 Tom Wheeley <tw104@york.ac.uk>
ACLOCAL_AMFLAGS         = -I m4

AUTOMAKE_OPTIONS        = 1.5 check-news dist-bzip2 -Wno-portability

GIT_COMMIT      = $(shell git log -1 --pretty=format:%h || echo UNKNOWN)
GIT_DATE        = $(shell git log -1 --pretty=format:%cd || echo UNKNOWN)
GIT_REFS        = $(shell git log -1 --pretty=format:%d || echo UNKNOWN)

EXTRA_DIST              = README autogen.sh CONTRIBUTE.md ChangeLog

DISTCLEANFILES          = configure.sh  ChangeLog paths.h
CLEANFILES              = *~ remotes/*~ contrib/*~
MANTAINERCLEANFILES     = config.sub install-sh depcomp py-compile \
                          missing libtool plugins/Makefile.am
CHANGELOG_REVS          = lirc-0_9_3..HEAD

AM_DISTCHECK_CONFIGURE_FLAGS = \
    --with-systemdsystemunitdir=$${dc_install_base}/lib/systemd/system

BUILT_SOURCES           = paths.h

if WITH_SYSTEMDSYSTEMUNITDIR
    SYSTEMD_DIR         = systemd
endif

SUBDIRS                 = lib daemons tools plugins configs doc $(SYSTEMD_DIR)

if INSTALL_ETC

lirc_confdir            = $(sysconfdir)/lirc
dist_lirc_conf_DATA     = lircd.conf \
                          lircmd.conf \
                          lirc_options.conf \
                          irexec.lircrc

lircdconfigdir		= $(sysconfdir)/lirc/lircd.conf.d
dist_lircdconfig_DATA	= README.conf.d

if LINUX_KERNEL
dist_lircdconfig_DATA	+= devinput.lircd.conf
endif

endif

dist_doc_DATA           = VERSION

pkgconfigdir            = $(libdir)/pkgconfig
dist_pkgconfig_DATA     = lirc.pc lirc-driver.pc

headerdir               = $(includedir)/lirc
nobase_header_HEADERS   = include/media/lirc.h
header_HEADERS          = paths.h config.h drivers/irpipe/irpipe.h

install-data-hook:
	$(SED) -i -e '/^plugindir/s|/usr/lib|$(libdir)|' \
	    $(DESTDIR)$(lirc_confdir)/lirc_options.conf
	cp -pr $(srcdir)/contrib $(DESTDIR)$(pkgdatadir)

uninstall-hook:
	-chmod -R u+w $(DESTDIR)/$(pkgdatadir)/contrib
	-rm -r $(DESTDIR)/$(pkgdatadir) \
	       $(DESTDIR)/$(pkgpythondir) \
	       $(DESTDIR)/$(pkgdatadir)/contrib

dist-hook: ChangeLog fix-version
	cp -pr $(srcdir)/contrib $(distdir)

maintainer-clean-local:
	find . -name Makefile.in -delete

fix-version: .phony
	chmod u+w $(distdir)/VERSION
	$(SED) -i '/COMMIT/s/=.*/="$(GIT_COMMIT)"/' $(distdir)/VERSION
	$(SED) -i '/DATE/s/=.*/="$(GIT_DATE)"/' $(distdir)/VERSION
	$(SED) -i '/REFS/s|=.*|="$(GIT_REFS)"|' $(distdir)/VERSION

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

devinput.lircd.conf:
	echo "# Rename to devinput.lircd.dist if not using devinput driver" > $@
	echo "# Re-generate for current kernel using lirc-make-devinput" >> $@
	echo '#' >> $@
	$(srcdir)/tools/lirc-make-devinput >> $@

.phony:
