ACLOCAL_AMFLAGS             = -I m4

AM_LDFLAGS                  = -module -avoid-version -L../lib -llirc_driver
AM_CFLAGS                   = -pthread -Wall
AM_CPPFLAGS                 = -I../lib  -I$(top_srcdir)/lib -I$(top_srcdir)
AM_CPPFLAGS                 += -Wall -Wp,-D_FORTIFY_SOURCE=2

DISTCLEANFILES              = pluginlist.am

plugindir                   =  $(pkglibdir)/plugins
EXTRA_DIST                  = pluginlist.am make-pluginlist.sh
plugin_LTLIBRARIES          =

if BUILD_CARACA
plugin_LTLIBRARIES          += caraca.la
caraca_la_SOURCES           = caraca.c
caraca_la_LDFLAGS           = $(AM_LDFLAGS) -lcaraca_client
endif

if BUILD_USB
plugin_LTLIBRARIES          += atilibusb.la
atilibusb_la_SOURCES        = atilibusb.c
atilibusb_la_LDFLAGS        = $(AM_LDFLAGS) @usb_libs@
atilibusb_la_CFLAGS         = $(AM_CFLAGS) $(LIBUSB_CFLAGS)

plugin_LTLIBRARIES          += awlibusb.la
awlibusb_la_SOURCES         = awlibusb.c
awlibusb_la_LDFLAGS         = $(AM_LDFLAGS) @usb_libs@
awlibusb_la_CFLAGS          = $(AM_CFLAGS) $(LIBUSB_CFLAGS)

plugin_LTLIBRARIES          += dfclibusb.la
dfclibusb_la_SOURCES        = dfclibusb.c
dfclibusb_la_LDFLAGS        = $(AM_LDFLAGS) @usb_libs@
dfclibusb_la_CFLAGS         = $(AM_CFLAGS) $(LIBUSB_CFLAGS)

plugin_LTLIBRARIES          += commandir.la
commandir_la_SOURCES        = commandir.c
commandir_la_LDFLAGS        = $(AM_LDFLAGS) @usb_libs@
commandir_la_CFLAGS         = $(AM_CFLAGS) $(LIBUSB_CFLAGS)

plugin_LTLIBRARIES          += srm7500libusb.la
srm7500libusb_la_SOURCES    = srm7500libusb.c
srm7500libusb_la_LDFLAGS    = $(AM_LDFLAGS) @usb_libs@
srm7500libusb_la_CFLAGS     = $(AM_CFLAGS) $(LIBUSB_CFLAGS)
endif

if BUILD_FTDI
plugin_LTLIBRARIES          += ftdi.la
ftdi_la_SOURCES             = ftdi.c
ftdi_la_LDFLAGS             = $(AM_LDFLAGS) @FTDI_LIBS@
endif

if BUILD_IGUANAIR
plugin_LTLIBRARIES          += iguanaIR.la
iguanaIR_la_SOURCES         = iguanaIR.c
iguanaIR_la_LDFLAGS         = $(AM_LDFLAGS) -liguanaIR -lrt
endif

if BUILD_LIBIRMAN
plugin_LTLIBRARIES          += irman.la
irman_la_SOURCES            = irman_drv.c
irman_la_LDFLAGS            = $(AM_LDFLAGS) -lirman
endif

if BUILD_LIBIRMAN_SW
plugin_LTLIBRARIES          += irman_sw.la
irman_sw_la_SOURCES         = irman_drv.c
irman_sw_la_LDFLAGS         = $(AM_LDFLAGS) -lirman_sw
endif

if BUILD_LIBPORTAUDIO
plugin_LTLIBRARIES          += audio.la
audio_la_SOURCES            = audio.c
audio_la_LDFLAGS            = $(AM_LDFLAGS) -lportaudio \
                              $(portaudio_lib_other)  @forkpty@
endif

if BUILD_LIBALSA
plugin_LTLIBRARIES          += audio_alsa.la
audio_alsa_la_SOURCES       = audio_alsa.c
audio_alsa_la_LDFLAGS       = $(AM_LDFLAGS) -lasound
endif

if BUILD_ALSA_SB_RC
plugin_LTLIBRARIES          += alsa_usb.la
alsa_usb_la_SOURCES         = alsa_usb.c
alsa_usb_la_LDFLAGS         = $(AM_LDFLAGS) -lasound
endif

if BUILD_I2CUSER
plugin_LTLIBRARIES          += i2cuser.la
i2cuser_la_SOURCES          = i2cuser.c
endif

if BUILD_DSP
plugin_LTLIBRARIES          += dsp.la
dsp_la_SOURCES              = dsp.c
endif

plugin_LTLIBRARIES          += atwf83.la
atwf83_la_SOURCES           = atwf83.c
atwf83_la_LDFLAGS           = $(AM_LDFLAGS) -lpthread

plugin_LTLIBRARIES          += livedrive_midi.la
livedrive_midi_la_SOURCES   = livedrive_midi.c livedrive_common.h \
                              livedrive_common.c

plugin_LTLIBRARIES          += livedrive_seq.la
livedrive_seq_la_SOURCES    = livedrive_seq.c livedrive_common.h \
                              livedrive_common.c

plugin_LTLIBRARIES          += uirt2.la
uirt2_la_SOURCES            = uirt2.c uirt2_common.c uirt2_common.h

plugin_LTLIBRARIES          += uirt2_raw.la
uirt2_raw_la_SOURCES        = uirt2_raw.c uirt2_common.c uirt2_common.h

if DARWIN
plugin_LTLIBRARIES          += osx_usbraw.la
osx_usbraw_la_SOURCES       = osx_usbraw.c
osx_usbraw_la_LDFLAGS       = $(AM_LDFLAGS) -framework IOKit -framework Cocoa

else
plugin_LTLIBRARIES          += hiddev.la
hiddev_la_SOURCES           = hiddev.c

plugin_LTLIBRARIES          += creative_infracd.la
creative_infracd_la_SOURCES = creative_infracd.c

plugin_LTLIBRARIES          += devinput.la
devinput_la_SOURCES         = devinput.c

plugin_LTLIBRARIES          += zotac.la
zotac_la_SOURCES            = zotac.c
endif


dist-hook:
	$(srcdir)/make-pluginlist.sh > pluginlist.am


include pluginlist.am
