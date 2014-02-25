
## Process this file with automake to produce Makefile.in

## here the correct driver is substituted in by the configure script
## (or empty for none)

EXTRA_DIST = lirc.h kcompat.h Makefile.kernel Makefile.common

DIST_SUBDIRS = \
	lirc_dev \
	lirc_gpio \
	lirc_i2c \
	lirc_igorplugusb \
	lirc_sasem \
	lirc_wpc8769l

SUBDIRS = @lirc_driver@

CLEANFILES = *~

