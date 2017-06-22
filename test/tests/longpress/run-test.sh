#!/bin/bash

##set -x

export LD_LIBRARY_PATH=../../../lib/.libs
here=$( dirname $( readlink -fn $0))
cd $here

exec &> ../../var/longpress.log
set -x

[ -s 'var/lircd.pid' ] && kill $( cat 'var/lircd.pid') &> /dev/null \
    && sleep 0.5 || :

rm -f *.out var/*.log

../../../daemons/lircd -O $PWD/lirc_options.conf \
                       -Dtrace lircd.conf &> var/lircd.log &
sleep 1.0
../../tools/irtestcase  -l lircrc  -p devlirc -t durations.txt &> longpress.out
diff /tmp/irtestcase/codes.log codes.txt || exit 1
diff -w /tmp/irtestcase/app_strings.log strings.txt || exit 1


