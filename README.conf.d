/etc/lirc/lircd.conf.d README
=============================

Files in this directory are as distributed included by the main
/etc/lirc/lircd.conf. Only files matching '*.conf' are included.

As distributed, here is a single file devinput.lircd.conf. This should
be used with the devinput driver which is enabled by default. When using
another driver, disable the devinput file e. g., by renaming it to
devinput.lircd.dist.

lircd normally (tries to) sort the remotes so the ones which decodes fastest
are used first. If you want to sort your configs manually, see the
Configuration Guide on using multiple remotes.
