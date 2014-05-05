#!/bin/sh
#
# This script is used for lirc protocol encode/decode regression testing
# between two versions of lirc. You'll need two source trees, both fully
# built, using --enable-debug and --enable-maintainer-mode.
#
# Nb: this will generate a LOT of output, both on-screen and in files, so
# consider running it with 2>/dev/null and in a fresh/empty directory.
#

# old known-good LIRC version
OLD=lirc-0.9.0/daemons/
# new LIRC version
NEW=lirc-0.9.1/daemons/
# where the config files are located -- these can be downloaded from
# http://lirc.org/remotes.tar.bz2

REMOTES=remotes

find -L $REMOTES -type f -print0|xargs -0 -n 1 echo|while read;
do
        # Skip invalid configs with 'one   0  0', see
        # http://sourceforge.net/p/lirc/mailman/message/32297923
	grep  '  one *0 *0' $REPLY && {
 		echo "Skipping invalid config (one '0'): $REPLY"
                continue
	}
        if echo $REPLY|grep "remove.sh\|lircmd\|\.png$\|\.jpg$\|\.irman$\|\.tira\|\.gif$\|\.lircrc$\|\.html$">/dev/null; then
                continue
        fi
        name="output/`basename $REPLY`"

#       echo "REPLY: $REPLY"

#
# send
#
#echo send1
	echo "Using remote: $REPLY" >> /tmp/lircd.log
        $NEW/lircd.simsend -n --pidfile=/tmp/lircd.sim.pid --output=/tmp/lircd.sim --logfile=/tmp/lircd.log $REPLY >${name}.new 2>/dev/null || continue
        while test -e /tmp/lircd.sim.pid; do sleep .1; done
#echo send2
        $OLD/lircd.simsend -n --pidfile=/tmp/lircd.sim.pid --output=/tmp/lircd.sim --logfile=/tmp/lircd.log $REPLY >${name}.old 2>/dev/null || continue
        while test -e /tmp/lircd.sim.pid; do sleep .1; done
#
# receive
#
#echo rec1
        $NEW/lircd.simrec --pidfile=/tmp/lircd.sim.pid --output=/tmp/lircd.sim --logfile=/tmp/lircd.log $REPLY <${name}.new >fail 2>&1&
        while ! tail -1 /tmp/lircd.log|grep ready >/dev/null; do sleep .12; done
        if ! irw /tmp/lircd.sim 2>/dev/null >${name}.rec_new; then
                echo "new irw failed!!!"
        fi
        while test -e /tmp/lircd.sim.pid; do sleep .1; done
#echo rec2
        $OLD/lircd.simrec --pidfile=/tmp/lircd.sim.pid --output=/tmp/lircd.sim --logfile=/tmp/lircd.log $REPLY <${name}.new >fail 2>&1&
        while ! tail -1 /tmp/lircd.log|grep ready >/dev/null; do sleep .14; done
        if ! irw /tmp/lircd.sim 2>/dev/null >${name}.rec_old; then
                echo "old irw failed!!!"
        fi
        while test -e /tmp/lircd.sim.pid; do sleep .1; done
        if ! test -s ${name}.new; then
                echo "simsend without output: $REPLY"
        else
                if diff ${name}.new ${name}.old >/dev/null; then
                        if ! test -s ${name}.rec_new; then
                                echo "simrec without output: $REPLY"
                        else
                                if diff ${name}.rec_new ${name}.rec_old >/dev/null; then

                                        rm ${name}.old ${name}.new
                                        rm ${name}.rec_old ${name}.rec_new
                                else
                                        echo "simrec output differs: $REPLY"
                                        diff -u ${name}.rec_old ${name}.rec_new
                                fi
                        fi
                else
                        echo "simsend output differs: $REPLY"
                fi
        fi
done
