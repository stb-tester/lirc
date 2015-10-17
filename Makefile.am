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
dist_lirc_conf_DATA     = lircd.conf lirc_options.conf lircmd.conf

lircdconfigdir		= $(sysconfdir)/lirc/lircd.conf.d
dist_lircdconfig_DATA	= README.conf.d devinput.lircd.dist

endif

dist_doc_DATA           = VERSION

pkgconfigdir            = $(libdir)/pkgconfig
dist_pkgconfig_DATA     = lirc.pc lirc-driver.pc

plugindocsdir           = $(pkgdatadir)/plugindocs
dist_plugindocs_DATA    = plugindocs/README

nobase_dist_pkgdata_DATA = $(srcdir)/contrib/* \
                           $(srcdir)/configs/kernel-drivers.dist

headerdir               = $(includedir)/lirc
nobase_header_HEADERS   = include/media/lirc.h
header_HEADERS          = paths.h drivers/irpipe/irpipe.h

install-data-hook:
	$(SED) -i -e '/^plugindir/s|/usr/lib|$(libdir)|' \
	    $(DESTDIR)$(lirc_confdir)/lirc_options.conf

uninstall-hook:
	rm -rf $(pkgdatadir) $(pkgpythondir)

dist-hook: ChangeLog $(distdir)/debian/distfiles fix-version
	tar -T $(distdir)/debian/distfiles -C $(srcdir) -cf - | \
	    tar -C $(distdir) -xf -

$(distdir)/debian/distfiles:
	cp -ar $(srcdir)/debian $(distdir)
	chmod u+w $@
	git ls-files $(srcdir)/debian > $@ || \
	    find $(srcdir)/debian -type f > $@

fix-version: .phony
	chmod u+w $(distdir)/VERSION
	sed -i '/COMMIT/s/=.*/="$(GIT_COMMIT)"/' $(distdir)/VERSION
	sed -i '/DATE/s/=.*/="$(GIT_DATE)"/' $(distdir)/VERSION
	sed -i '/REFS/s|=.*|="$(GIT_REFS)"|' $(distdir)/VERSION

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

devinput.lircd.dist:
	echo "# Rename to devinput.lircd.conf when using devinput driver" > $@
	echo "# Re-generate for current kernel using lirc-make-devinput" >> $@
	echo '#' >> $@
	$(srcdir)/tools/lirc-make-devinput >> $@

debian-pkg:
	test -f lirc-$(VERSION).tar.gz || $(MAKE) dist
	test -d debian-pkg && rm -rf debian-pkg; mkdir debian-pkg
	cp lirc-$(VERSION).tar.gz \
	     debian-pkg/lirc_$(VERSION).orig.tar.gz
	cd debian-pkg && tar xf lirc*tar.gz
	cd debian-pkg/lirc-* && debuild -S -us -uc
	rm -rf debian-pkg/lirc-*


.phony:
