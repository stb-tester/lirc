#!/usr/bin/env python3

'''
Reference python implementation of ircat(1)
Run using python3 ircat.py [socket_path [lircrc path]]
'''

import sys

import lirc

if len(sys.argv) >= 5 or len(sys.argv) < 2:
    sys.stderr.write("Usage: ircat.py program [lircrc path [socket path]]\n")
    sys.exit(1)
socket_path = sys.argv[3] if len(sys.argv) == 4 else None
lircrc_path = sys.argv[2] if len(sys.argv) >= 3 else None
program = sys.argv[1]

with lirc.LircdConnection(program, lircrc_path, socket_path) as conn:
    while True:
        print(conn.readline())
