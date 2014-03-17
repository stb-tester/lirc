KERNELDIR            = /usr/src/kernels/$(shell uname -r)
LIBS		     = $(shell pkg-config --libs lirc-driver)
PLUGINDIR            = $(shell pkg-config --variable=plugindir lirc-driver)
CFLAGS               += -g

all: default.so

default.so: default.o Makefile
	gcc -shared -fPIC -DPIC  $(LDFLAGS) $(LIBS) -o default.so default.o

default.o: default.c Makefile
	gcc -c default.c -I$(KERNELDIR) -fPIC $(CFLAGS)

install:  default.so
	test -d $(DESTDIR)$(PLUGINDIR) || mkdir -p $(DESTDIR)$(PLUGINDIR)
	cp -ar default.so $(DESTDIR)$(PLUGINDIR)

clean:
	rm -f *.o *.so
