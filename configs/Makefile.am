## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

menuconfigs             = *.conf

configsdir              = $(pkgdatadir)/configs
dist_configs_DATA       = $(menuconfigs) \
                          drivers.yaml \
                          kernel-drivers.yaml \
                          confs_by_driver.yaml \
                          remotes.list


CLEANFILES = drivers.yaml kernel-drivers.yaml confs_by_driver.yaml

all-local: drivers.yaml kernel-drivers.yaml confs_by_driver.yaml

drivers.yaml: ../tools/lirc-lsplugins $(menuconfigs)
	../tools/lirc-lsplugins  -U ../plugins/.libs --yaml > drivers.yaml

kernel-drivers.yaml: kernel-drivers.dist ../tools/lirc-kerneldrivers
	../tools/lirc-kerneldrivers kernel-drivers.dist > kernel-drivers.yaml

confs_by_driver.yaml: remotes.list ../tools/irdb-get
	-XDG_CACHE_HOME=$$(pwd) \
	    ../tools/irdb-get yaml-config > confs_by_driver.yaml

remotes.list:
	XDG_CACHE_HOME=$$(pwd) ../tools/irdb-get update
