ACLOCAL_AMFLAGS             = -I m4

AM_LDFLAGS                  = -ldl

BUILT_SOURCES               = lirc/input_map.inc

AM_CPPFLAGS                 = -I@kerneldir@

lib_LTLIBRARIES             = liblirc.la

liblirc_la_SOURCES          = config_file.c \
                              ciniparser.c \
                              dictionary.c \
                              dump_config.c \
                              hw-types.c \
                              input_map.c \
                              ir_remote.c \
                              lirc_log.c \
                              lirc_options.c \
                              receive.c  \
                              release.c \
                              serial.c \
                              transmit.c

dist_include_HEADERS        = lirc_private.h

lircincludedir              = $(includedir)/lirc
dist_lircinclude_HEADERS    = config_file.h \
                              ciniparser.h \
                              dictionary.h \
                              dump_config.h \
                              hardware.h \
                              hw-types.h \
                              input_map.h \
                              ir_remote.h \
                              ir_remote_types.h \
                              lirc_client.h \
                              lirc_log.h \
                              lirc_options.h \
                              release.h \
                              receive.h \
                              serial.h \
                              transmit.h \
                              input_map.inc

dist-hook:
	mkdir $(distdir)/lirc
	$(srcdir)/input_map.sh > $(distdir)/lirc/input_map.inc
	cp $(srcdir)/*.h $(distdir)/lirc

input_map.lo: lirc/input_map.inc

input_map.inc: lirc/input_map.inc

lirc/input_map.inc:
	$(srcdir)/input_map.sh >./input_map.inc

DISTCLEANFILES              = input_map.inc lirc/input_map.inc
EXTRA_DIST                  = input_map.inc input_map.sh

