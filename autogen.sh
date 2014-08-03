#!/bin/bash
rm -rf aclocal.m4 config.guess config.h.in* config.sub configure depcomp install-sh install.sh
rm -rf ltmain.sh missing autom4te.cache
find . -name Makefile -delete
find . -name Makefile.in -delete
find . -name \*.la -delete
autoreconf -i -f
(cd contrib/hal/ && ./gen-hal-fdi.pl)
cd plugins; ./make-pluginlist.sh > pluginlist.am
