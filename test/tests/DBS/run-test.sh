#!/bin/bash

##env > testenv


export LD_LIBRARY_PATH=../../../lib/.libs
here=$( dirname $( readlink -fn $0))
cd $here
exec &> ../../var/dbs-run-test.log
set -x

[ -s 'var/lircd.pid' ] && kill $( cat 'var/lircd.pid') &> /dev/null \
    && sleep 0.5 || :


rm -f DBS.received var/*.log

../../../daemons/lircd -O $PWD/lirc_options.conf \
                       -Dtrace lircd-DBS.conf &> var/lircd.log &
sleep 1.0
../../echoserver &> var/echoserver.log

sed -i '/^#/d' DBS.received
diff d9-mute.ref DBS.received


