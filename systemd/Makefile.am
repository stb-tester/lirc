## Process this file with automake to produce Makefile.in

## Basically, although basically a silly thing to do unitfiles are
## installed in ${prefix}/lib/systemd/system which might end up in
## /usr/local or worse. However, if we have write access to /usr
## (i. e., $(DESTDIR)/usr) we move the stuff in there afterwards.

ACLOCAL_AMFLAGS             = -I m4

UNITFILES                   = lircd.socket lircd.service lircmd.service
live_systemddir             = $(DESTDIR)/usr/lib/systemd/system

dist_sysconf_DATA           = lirc.sysconfig

systemddir                  = $(prefix)/lib/systemd/system
dist_systemd_DATA           = $(UNITFILES)

install-data-hook:
	if test -w $(DESTDIR)/usr -a "$(prefix)" != "/usr"; then \
	    mkdir -p $(live_systemddir) || :; \
	    for u in $(UNITFILES); do \
	        mv $(DESTDIR)$(systemddir)/$$u $(live_systemddir); \
	    done; \
	fi

uninstall-local:
	for u in $(UNITFILES); do\
	    rm -f $(live_systemddir)/$$u 2>/dev/null|| :; \
	done
