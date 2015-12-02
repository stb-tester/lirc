obj-m += irpipe.o
ccflags-y +=-O1 -g

all: test
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

test:	test.c
	cc -o test $(CFLAGS) -I ../../include  test.c
