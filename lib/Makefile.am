ACLOCAL_AMFLAGS             = -I m4

DISTCLEANFILES              = input_map.inc lirc/input_map.inc
CLEANFILES                  = driver_api.dox lirc_client.dox config.py
EXTRA_DIST                  = driver_api.dox  lirc_client.dox  mainpage.dox \
                              driver_api.doxhead  lirc_client.doxhead

BUILT_SOURCES               =

if HAVE_DEVINPUT
BUILT_SOURCES               += lirc/input_map.inc
endif

AM_CPPFLAGS                 = -I$(top_srcdir) -Wall -Wp,-D_FORTIFY_SOURCE=2 \
                              @osflags@

lib_LTLIBRARIES             = liblirc.la liblirc_client.la liblirc_driver.la \
                              libirrecord.la

liblirc_la_SOURCES          = config_file.c \
                              ciniparser.c \
                              dictionary.c \
                              driver.c \
                              drv_admin.c \
                              dump_config.c \
                              input_map.c \
                              ir_remote.c \
                              line_buffer.cpp \
                              lirc_log.c \
                              lirc_options.c \
                              lirc-utils.c \
                              curl_poll.c  \
                              receive.c  \
                              release.c \
                              serial.c \
                              transmit.c

libirrecord_la_LIBADD       = liblirc.la
libirrecord_la_SOURCES      = irrecord.c

liblirc_client_la_LDFLAGS   = -version-info 4:0:4
liblirc_client_la_SOURCES   = lirc_client.c\
			      lirc_client.h \
			      curl_poll.c \
			      curl_poll.h \
			      lirc_log.c \
			      lirc_log.h

liblirc_driver_la_LDFLAGS   = -version-info 0:0:0
liblirc_driver_la_SOURCES   = driver.h \
                              drv_enum.c \
                              drv_enum.h \
                              ir_remote.c \
                              ir_remote.h \
                              ir_remote_types.h \
                              lirc_log.c \
                              lirc_log.h \
                              curl_poll.c \
                              curl_poll.h \
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
                              curl_poll.h \
                              dictionary.h \
                              drv_admin.h \
                              dump_config.h \
                              driver.h \
                              input_map.h \
                              ir_remote.h \
                              ir_remote_types.h \
                              irrecord.h \
                              line_buffer.h \
                              lirc_client.h \
                              lirc_config.h \
                              lirc_log.h \
                              lirc_options.h \
                              lirc-utils.h \
                              release.h \
                              receive.h \
                              serial.h \
                              transmit.h

if HAVE_DEVINPUT
dist_lircinclude_HEADERS    += input_map.inc
endif

pep8:   $(py_PYTHON)
	python3-pep8 --config=../pep8.conf $?

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
	cp $(srcdir)/*.h $(distdir)/lirc

input_map.lo: lirc/input_map.inc

input_map.inc: lirc/input_map.inc

if HAVE_DEVINPUT
lirc/input_map.inc:
	ln -s . lirc || :
	$(top_srcdir)/tools/lirc-make-devinput -i > input_map.inc
	$(top_srcdir)/tools/lirc-make-devinput -i > $@
else
lirc/input_map.inc:
	touch $@
	touch input_map.inc
endif

checkfiles:
	../git-tools/checkfiles $(SOURCES) $(HEADERS)

.phony:

