## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

menuconfigs             = $(srcdir)/*.conf

configsdir              = $(pkgdatadir)/configs
dist_configs_DATA       = $(menuconfigs) \
                          kernel-drivers.dist\
                          remotes.list

nodist_configs_DATA     = drivers.yaml \
                          confs_by_driver.yaml \
                          kernel-drivers.yaml


CLEANFILES              = drivers.yaml kernel-drivers.yaml confs_by_driver.yaml
DISTCLEANFILES          = remotes.list

drivers.yaml: ../tools/lirc-lsplugins $(menuconfigs)
	../tools/lirc-lsplugins  -U ../plugins/.libs --yaml > drivers.yaml

kernel-drivers.yaml: $(srcdir)/kernel-drivers.dist
	$(top_srcdir)/tools/lirc-kerneldrivers $(srcdir)/kernel-drivers.dist \
		 > kernel-drivers.yaml

confs_by_driver.yaml: $(srcdir)/remotes.list $(top_srcdir)/tools/irdb-get
	-XDG_CACHE_HOME=$(srcdir) \
	    $(top_srcdir)/tools/irdb-get yaml-config > confs_by_driver.yaml

remotes.list:
	test -w $(srcdir) && \
	    XDG_CACHE_HOME=$(srcdir) ../tools/irdb-get update

../tools/lirc-lsplugins:
	cd ../lib; $(MAKE)
	cd ../tools; $(MAKE) lirc-lsplugins
