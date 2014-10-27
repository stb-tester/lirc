#!/bin/bash
#
# This script is used for lirc protocol encode/decode regression testing
# between two versions of lirc. You'll need two source trees, both fully
# built, using --enable-debug and --enable-maintainer-mode for pre-0.9.2;
# the options are not required for 0.9.2+.
#
# Nb: this will generate a LOT of output, both on-screen and in files, so
# consider running it with 2>/dev/null and in a fresh/empty directory.
#

# old known-good LIRC version
OLD=lirc-0.9.0/
# new LIRC version
NEW=lirc-0.9.2-devel
# where the config files are located -- these can be downloaded from
# http://lirc.org/remotes.tar.bz2
REMOTES=remotes

SKIP_PATTERN='remove.sh|lircmd|png$|jpg$|irman$|.tira|gif$|html$|list$'
SKIP_PATTERN="$SKIP_PATTERN"'|JPG$|htaccess|lircmd|lircrc'
logfile="/tmp/lircd.log"
test -d output || mkdir output

function cleanup()
{
    rm -f /tmp/lircd.log
    rm -f /tmp/lircd.sim*
    rm -rf output; mkdir output
    pkill lircd
    exit 0
}


function logecho()
{
    echo $* >> $logfile
    echo $*
}


function get_new_args()
# return plugindir/driver/debug options as required for current NEW version
{
    local send_or_receive=$1
    local version=$( $NEW/daemons/lircd -v )
    local args=''

    case $version in
        *0.9.2* ) args="--plugindir=$NEW/plugins/.libs --debug=6"
            if [[ $send_or_receive == *send* ]]; then
                args="$args --driver=simsend"
            else
                args="$args --driver=simreceive"
            fi
            ;;
        * ) args="--driver=default"
            ;;
    esac
    echo $args
}


if [ "$1" = '-c' ]; then
    cleanup
fi

find -L $REMOTES -type f -print0|xargs -0 -n 1 echo|while read;
do
        # Skip invalid configs with 'one   0  0', see
        # http://sourceforge.net/p/lirc/mailman/message/32297923
        grep  '  one *0 *0' $REPLY && {
                logecho "SKIP:  invalid config (one '0'): $REPLY"
                continue
        }
        if echo $REPLY | grep -Eq $SKIP_PATTERN; then
                logecho "SKIP: (bad name) : $REPLY"
                continue
        fi
        name="output/`basename $REPLY`"

#
# send
#
        logecho send1
        logecho "Using remote: $REPLY"
        $NEW/daemons/lircd.simsend --pidfile=/tmp/lircd.sim.pid \
                                   --output=/tmp/lircd.sim \
                                   --logfile=$logfile \
                                   --nodaemon \
                                   $(get_new_args 'send') \
            $REPLY >${name}.new 2>send1.log || :
        while test -e /tmp/lircd.sim.pid; do sleep .1; done

        logecho send2
        $OLD/daemons/lircd.simsend --pidfile=/tmp/lircd.sim.pid \
                                   --output=/tmp/lircd.sim \
                                   --logfile=$logfile \
                                   --nodaemon \
            $REPLY >${name}.old 2>send2.log || :
        while test -e /tmp/lircd.sim.pid; do sleep .1; done
#
# receive
#
        logecho "rec1 ($REPLY)"
        $NEW/daemons/lircd.simrec --pidfile=/tmp/lircd.sim.pid \
                                  --output=/tmp/lircd.sim \
                                  --logfile=$logfile \
                                  --nodaemon \
                                  $(get_new_args 'receive') \
            $REPLY <${name}.new >rec1.log 2>&1 &
        while ! tail -5 $logfile | grep ready >/dev/null; do sleep .12; done
        if ! $NEW/tools/irw /tmp/lircd.sim 2>>$logfile >${name}.rec_new; then
                logecho "ERROR: new irw failed!!!"
        fi
        while test -e /tmp/lircd.sim.pid; do sleep .1; done

        logecho "rec2 ($REPLY)"
        $OLD/daemons/lircd.simrec --pidfile=/tmp/lircd.sim.pid \
                                  --output=/tmp/lircd.sim \
                                  --logfile=$logfile \
                                  --nodaemon \
            $REPLY <${name}.new >rec2.log 2>&1 &
        while ! tail -1 $logfile | grep ready >/dev/null; do sleep .14; done
        if ! $OLD/tools/irw /tmp/lircd.sim 2>/dev/null >${name}.rec_old; then
                logecho "ERROR: old irw failed!!!"
        fi
        while test -e /tmp/lircd.sim.pid; do sleep .1; done

#
# Check results
#

        if ! test -s ${name}.new; then
                logecho "ERROR: simsend without output: $REPLY"
        else
                if diff ${name}.new ${name}.old >/dev/null; then
                        if ! test -s ${name}.rec_new; then
                                logecho "FAIL: simrec without output: $REPLY"
                        else
                                if diff ${name}.rec_new ${name}.rec_old >/dev/null; then
                                        logecho "OK: $REPLY"
                                        rm ${name}.old ${name}.new
                                        rm ${name}.rec_old ${name}.rec_new
                                else
                                        logecho "FAIL:simrec output differs: $REPLY"
                                        diff -u ${name}.rec_old ${name}.rec_new
                                fi
                        fi
                else
                        logecho "FAIL: simsend output differs: $REPLY"
                fi
        fi
done
