#
# Template for building a lirc userspace driver out of tree.
# Requires that lirc is installed in system locations, in
# particular that the /usr/lib[64]/pkgconfig/lirc-driver.pc
# is in place (/usr/local/lib/pkgconfig/... is also OK).
#

all:  default.so

CFLAGS          += $(shell pkg-config --cflags lirc-driver) -fpic
LDFLAGS         += $(shell pkg-config --libs lirc-driver)
PLUGINDIR       =  $(shell pkg-config --variable=plugindir lirc-driver)

default.o: default.c

default.so: default.o
	gcc --shared -fpic $(LDFLAGS) -o $@ $<

install: default.so
	install  $<  $(PLUGINDIR)

clean:
	rm -f *.o *.so
