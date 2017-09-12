## Process this file with automake to produce Makefile.in
## Makefile.am, (c)1999 Tom Wheeley <tw104@york.ac.uk>
ACLOCAL_AMFLAGS         = -I m4

AUTOMAKE_OPTIONS        = 1.5 check-news dist-bzip2 -Wno-portability \
                          subdir-objects

include                 pylint.mak

GIT_COMMIT      = $(shell git log -1 --pretty=format:%h || echo UNKNOWN)
GIT_DATE        = $(shell git log -1 --pretty=format:%cd || echo UNKNOWN)
GIT_REFS        = $(shell git log -1 --pretty=format:%d || echo UNKNOWN)
GIT_DATE_ISO    = $(shell git log -1 --pretty=format:%ci || date +"%F %T")

BASE_VERSION    = $(subst -rc,rc,$(subst -devel,,$(VERSION)))

VERBOSE         = $(subst 0,,$(V))

DPY_PKG          = notused
PY_PKG_CFLAGS   = -I$(abs_top_srcdir)/lib -I$(abs_builddir)/lib $(CFLAGS)
PY_PKG_LFLAGS   = -L$(abs_builddir)/lib/.libs $(LDFLAGS)

if HAVE_PYTHON35
PYTHON_TARBALL          = python-pkg/dist/lirc-$(BASE_VERSION).tar.gz
endif

EXTRA_DIST              = README autogen.sh CONTRIBUTE.md ChangeLog \
                          COPYING.curl COPYING.ciniparser

DISTCLEANFILES          = configure.sh  ChangeLog paths.h
CLEANFILES              = *~ remotes/*~ contrib/*~ devinput.lircd.conf
CHANGELOG_REVS          = lirc-0.10.0..HEAD
MANTAINERCLEANFILES     = config.sub install-sh depcomp py-compile \
                          missing libtool plugins/Makefile.am



AM_DISTCHECK_CONFIGURE_FLAGS = \
    --with-systemdsystemunitdir=$${dc_install_base}/lib/systemd/system

BUILT_SOURCES           = paths.h python-pkg/lirc/config.py python-pkg/VERSION

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

lircdconfigdir          = $(sysconfdir)/lirc/lircd.conf.d
dist_lircdconfig_DATA   = README.conf.d

if HAVE_DEVINPUT
nodist_lircdconfig_DATA = devinput.lircd.conf
endif

endif

dist_doc_DATA           = VERSION

nodist_pkgdata_DATA     = $(PYTHON_TARBALL)

pkgconfigdir            = $(libdir)/pkgconfig
dist_pkgconfig_DATA     = lirc.pc lirc-driver.pc

headerdir               = $(includedir)/lirc
header_HEADERS          = include/linux/input-event-codes.h \
                          drivers/irpipe/irpipe.h

mediadir                = $(includedir)/lirc/media
media_HEADERS           = include/media/lirc.h

py_pkgdir               = $(pkgdatadir)/python-pkg
dist_py_pkg_DATA        = python-pkg/setup.py \
                          python-pkg/README.rst

py_pkg_lircdir          = $(pkgdatadir)/python-pkg/lirc
py_PYTHON               = python-pkg/lirc/__init__.py \
                          python-pkg/lirc/database.py \
                          python-pkg/lirc/paths.py

nodist_py_pkg_PYTHON    = python-pkg/lirc/config.py

if HAVE_PYTHON35
py_PYTHON               += python-pkg/lirc/async_client.py \
                          python-pkg/lirc/client.py

py_pkg_testdir          = $(pkgdatadir)/python-pkg/tests
dist_py_pkg_test_DATA   = python-pkg/tests/test_client.py \
                          python-pkg/tests/dummy-server \
                          python-pkg/tests/lircrc.conf
endif

dist_py_pkg_lirc_DATA   = $(py_PYTHON) \
                          python-pkg/lirc/_client.c \
                          python-pkg/lirc/lirctool.py \
                          python-pkg/lirc/lirctool

py_pkg_docdir           = $(pkgdatadir)/python-pkg/lirc/doc
dist_py_pkg_doc_DATA    = python-pkg/doc/Doxyfile \
                          python-pkg/doc/mainpage.dox \
                          python-pkg/doc/diode-2.gif

libpython               = $(libdir)/python$(PYTHON_VERSION)
pydir                   = $(libpython)/site-packages/lirc
if HAVE_PYTHON35
py_LTLIBRARIES          = python-pkg/lib/_client.la
python_pkg_lib__client_la_SOURCES = \
                          python-pkg/lirc/_client.c
python_pkg_lib__client_la_CPPFLAGS = \
                          $(PYTHON_CFLAGS) $(AM_CPPFLAGS) \
			  -I$(abs_builddir)/lib -I$(abs_srcdir)/lib

python_pkg_lib__client_la_LDFLAGS  = \
                          -module -avoid-version -export-symbols-regex client \
                          $(PYTHON_LIBS)
python_pkg_lib__client_la_LIBADD = \
                          lib/liblirc_client.la
endif

pylint: .phony
	$(MAKE) -C tools pylint
	-PYTHONPATH=$(PYTHONPATH) $(PYLINT) --rcfile=pylint.conf \
	--msg-template='$(pylint_template)' $(py_PYTHON)

pep8: $(py_PYTHON)
	$(MAKE) -C tools pep8
	python3-pep8 --config=pep8.conf $?

if HAVE_PYTHON35
all-local:
	cd python-pkg; \
	    $(SETUPTOOLS_ENV) \
	    CFLAGS="$(PY_PKG_CFLAGS)" LDFLAGS="$(PY_PKG_LFLAGS)" \
	    $(PYTHON) setup.py $(if $(VERBOSE),,-q) build
endif

install-data-hook:
	$(SED) -i -e '/^plugindir/s|/usr/lib|$(libdir)|' \
	    $(DESTDIR)$(lirc_confdir)/lirc_options.conf
	cp -pr $(srcdir)/contrib $(DESTDIR)$(pkgdatadir)

uninstall-hook:
	-chmod -R u+w $(DESTDIR)/$(pkgdatadir)/contrib
	-rm -rf $(DESTDIR)/$(pkgdatadir) \
	        $(DESTDIR)/$(pkgpythondir) \
	        $(DESTDIR)/$(pkgdatadir)/contrib

dist-hook: ChangeLog fix-version
	touch -d "$(GIT_DATE_ISO)" $(distdir)/lib/lirc/* VERSION
	cp -pr $(srcdir)/contrib $(distdir)

clean-local:
	cd $(builddir)/python-pkg; rm -rf  lirc/__pycache__ lirc.egg-info \
	    lirc/config.py lirc/_client.so tests/__pycache__  VERSION \
	    dist build lib

distclean-local:
	test $(abs_builddir) = $(abs_srcdir) || \
	    rm -rf $(abs_builddir)/python-pkg


maintainer-clean-local:
	find . -name Makefile.in -delete

fix-version: .phony
	chmod u+w $(distdir)/VERSION
	$(SED) -i '/COMMIT/s/=.*/="$(GIT_COMMIT)"/' $(distdir)/VERSION
	$(SED) -i '/DATE/s/=.*/="$(GIT_DATE)"/' $(distdir)/VERSION
	$(SED) -i '/REFS/s|=.*|="$(GIT_REFS)"|' $(distdir)/VERSION

$(abs_builddir)/python-pkg/setup.py:
	cp -ar $(top_srcdir)/python-pkg  $(abs_builddir)
	chmod -R u+w  python-pkg

$(PYTHON_TARBALL): $(abs_builddir)/python-pkg/setup.py
	cp $(top_builddir)/VERSION $(abs_builddir)/python-pkg
	cd $(abs_builddir)/python-pkg; \
	    CFLAGS="$(PY_PKG_CFLAGS)" LDFLAGS="$(PY_PKG_LFLAGS)" \
	    $(SETUPTOOLS_ENV) \
	    $(PYTHON) setup.py -q sdist

$(top_builddir)/python-pkg/VERSION: VERSION
	cp $? $@

python-pkg/lirc/config.py: Makefile  $(abs_builddir)/python-pkg/setup.py
	@echo '# Generated by lib/Makefile.am.' >$@
	@echo 'DATADIR="$(datadir)"' >>$@
	@echo 'SYSCONFDIR="$(sysconfdir)"' >>$@
	@echo 'VARRUNDIR="$(localstatedir)/run"' >>$@
	@echo 'LOCALSTATEDIR="$(localstatedir)"' >>$@
	@echo 'BINDIR="$(bindir)"' >>$@
	@echo 'LIBDIR="$(libdir)"' >>$@
	@echo 'DOCDIR="$(docdir)"' >>$@
	@echo 'MODINFO="$(MODINFO)"' >>$@
	@echo 'VERSION="$(VERSION)"' >>$@

paths.h: Makefile
	@echo "#ifndef PATHS_H"          >paths.h
	@echo "#define PATHS_H"          >>paths.h
	@echo '#define DATADIR          "$(datadir)"' >>paths.h
	@echo '#define SYSCONFDIR       "$(sysconfdir)"' >>paths.h
	@echo '#define VARRUNDIR        "$(runstatedir)"' >>paths.h
	@echo '#define LOCALSTATEDIR    "$(localstatedir)"' >>paths.h
	@echo '#define LIBDIR           "$(libdir)"' >>paths.h
	@echo "#endif"                  >>paths.h

ChangeLog: .phony
	test -s ChangeLog || ( \
	    $(top_srcdir)/git-tools/gitlog-to-changelog $(CHANGELOG_REVS) || \
	        echo "See http://sf.net/p/lirc/git/ci//tree/"  \
	) > ChangeLog

devinput.lircd.conf:
	echo "# Rename to devinput.lircd.dist if not using devinput driver" > $@
	echo "# Re-generate for current kernel using lirc-make-devinput" >> $@
	echo '#' >> $@
	PYTHON=$(PYTHON) \
	    $(srcdir)/tools/lirc-make-devinput $(DEVINPUT_HEADER) >> $@

.phony:

## Apply .SILENT unless $(V) is set and not "0".
$(subst 0,,$(V)).SILENT:
