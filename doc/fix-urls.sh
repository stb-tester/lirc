#!/usr/bin/bash

echo -n "Fixing urls..."
find . -name \*.html | xargs sed -i \
    -e 's|href=".*sudo.html"|href="https://manned.org/sudo"|g' \
    -e 's|href=".*xdotool.html"|href="http://https://manned.org/xdotool"|g' \
    -e 's|href=".*socat.html"|href="https://manned.org/socat.1"|g' \
    -e 's|href=".*XStringToKeysym.3x.html"|href="https://www.x.org/releases/X11R7.5/doc/man/man3/XStringToKeysym.3.html"|g' \
    -e 's|href=".*XKeysymToKeycode.3x.html"|href=https://www.x.org/releases/X11R7.5/doc/man/man3/XKeysymToKeycode.3.html"|g' \
    -e 's|href=".*XClassHint.3x.html"|href="https://www.x.org/releases/X11R7.5/doc/man/man3/XAllocClassHint.3.html"|g' \
    -e 's|href=".*XGetInputFocus.3x.html"|href="https://www.x.org/releases/X11R7.5/doc/man/man3/XSetInputFocus.3.html"|g' \
    -e 's|href=".*RootWindow.3x.html"|href="https://www.x.org/releases/X11R7.5/doc/man/man3/RootWindow.3.html"|g' \
    -e 's|href=".*ncat.html"|href="http://man7.org/linux/man-pages/man1/ncat.1.html"|g' \
    -e 's|href=".*fnmatch.html"|href="http://man7.org/linux/man-pages/man3/fnmatch.3.html"|g' \
    -e 's|href=".*dmesg.html"|href="http://man7.org/linux/man-pages/man1/dmesg.1.html"|g' \
    -e 's|href=".*syslog.html"|href="http://man7.org/linux/man-pages/man3/syslog.3.html"|g' \
    -e 's|href=".*chmod.html"|href="http://man7.org/linux/man-pages/man1/chmod.1.html"|g' \
    -e 's|href=".*setfacl.html"|href="http://man7.org/linux/man-pages/man1/setfacl.1.html"|g' \
    -e 's|href=".*glob.html"|href="http://man7.org/linux/man-pages/man3/glob.3.html"|g'
