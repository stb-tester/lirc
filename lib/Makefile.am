ACLOCAL_AMFLAGS             = -I m4

DISTCLEANFILES              = input_map.inc lirc/input_map.inc
CLEANFILES                  = driver_api.dox lirc_client.dox
EXTRA_DIST                  = input_map.inc input_map.sh \
			      driver_api.dox  lirc_client.dox  mainpage.dox \
			      driver_api.doxhead  lirc_client.doxhead

BUILT_SOURCES               = lirc/input_map.inc

AM_CPPFLAGS                 = -I$(top_srcdir) -Wall -Wp,-D_FORTIFY_SOURCE=2

lib_LTLIBRARIES             = liblirc.la liblirc_client.la liblirc_driver.la

liblirc_la_LDFLAGS          = -ldl
liblirc_la_SOURCES          = config_file.c \
                              ciniparser.c \
                              dictionary.c \
			      driver.c \
                              drv_admin.c \
                              dump_config.c \
                              input_map.c \
                              ir_remote.c \
                              lirc_log.c \
                              lirc_options.c \
                              receive.c  \
                              release.c \
                              serial.c \
                              transmit.c

liblirc_client_la_LDFLAGS   = -version-info 3:0:3
liblirc_client_la_SOURCES   = lirc_client.c lirc_client.h lirc_log.c lirc_log.h

liblirc_driver_la_LDFLAGS   = -version-info 0:0:0
liblirc_driver_la_SOURCES   = driver.h \
                              ir_remote.c \
                              ir_remote.h \
                              ir_remote_types.h \
                              lirc_log.c \
                              lirc_log.h \
                              receive.c \
                              receive.h \
			      release.c \
                              serial.h \
                              serial.c \
                              transmit.c \
                              transmit.h

dist_include_HEADERS        = lirc_client.h \
                              lirc_driver.h \
                              lirc_private.h

lircincludedir              = $(includedir)/lirc
dist_lircinclude_HEADERS    = config_file.h \
                              config_flags.h \
                              ciniparser.h \
                              dictionary.h \
                              drv_admin.h \
                              dump_config.h \
                              driver.h \
                              input_map.h \
                              ir_remote.h \
                              ir_remote_types.h \
                              lirc_client.h \
                              lirc_config.h \
                              lirc_log.h \
                              lirc_options.h \
                              release.h \
                              receive.h \
                              serial.h \
                              transmit.h \
                              input_map.inc

driver_api.dox: $(srcdir)/driver_api.doxhead  \
		$(top_srcdir)/doc/html-source/driver-api.html
	cat $? > $@
	echo "@example: drivers/default/Makefile" >> $@
	echo "*/" >> $@

lirc_client.dox: $(srcdir)/lirc_client.doxhead \
		$(top_srcdir)/doc/html-source/lirc_client.html
	cat $?  > $@
	echo "*/" >> $@

dox: driver_api.dox lirc_client.dox

dist-hook:
	mkdir $(distdir)/lirc
	$(srcdir)/input_map.sh > $(distdir)/lirc/input_map.inc
	cp $(srcdir)/*.h $(distdir)/lirc

input_map.lo: lirc/input_map.inc

input_map.inc: lirc/input_map.inc

lirc/input_map.inc:
	$(srcdir)/input_map.sh >./input_map.inc


