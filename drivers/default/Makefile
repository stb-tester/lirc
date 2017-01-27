#
# Template for building a lirc userspace driver out of tree.
# Requires that lirc is installed in system locations, in
# particular that the /usr/lib[64]/pkgconfig/lirc-driver.pc
# is in place (/usr/local/lib/pkgconfig/... is also OK).
#
# The document installation needs to be finished, typically
# as root using something like:
#
#     # cd $(pkg-config --variable=plugindocs lirc-driver); make
#
# This updates some docs in /var/lib/lirc which eventually are
# included in the main docs.


driver          = default

CFLAGS          += $(shell pkg-config --cflags lirc-driver)
LDFLAGS         += $(shell pkg-config --libs lirc-driver)
PLUGINDIR       ?= $(shell pkg-config --variable=plugindir lirc-driver)
CONFIGDIR       ?= $(shell pkg-config --variable=configdir lirc-driver)
PLUGINDOCS      ?= $(shell pkg-config --variable=plugindocs lirc-driver)


all:  $(driver).so

$(driver).o: $(driver).c

$(driver).so: $(driver).o
	gcc --shared -fpic $(LDFLAGS) -o $@ $<

install: $(driver).so
	install -D $< $(DESTDIR)$(PLUGINDIR)/$<
	install -Dm 644 $(driver).conf $(DESTDIR)$(CONFIGDIR)/$(driver).conf
	install -Dm 644 $(driver).html $(DESTDIR)$(PLUGINDOCS)/$(driver).html

clean:
	rm -f *.o *.so
