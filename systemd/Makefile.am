## Process this file with automake to produce Makefile.in

## Basically, although basically a silly thing to do unitfiles are
## installed in ${prefix}/lib/systemd/system which might end up in
## /usr/local or worse. However, if we have write access to /usr
## (i. e., $(DESTDIR)/usr) we move the stuff in there afterwards.

ACLOCAL_AMFLAGS             = -I m4

dist_systemdsystemunit_DATA = lircd.socket lircd.service lircmd.service
