#!/bin/bash
source ../../find-reps.bash

##set -x
##env > testenv

basename='SR-90'

export PATH=$PATH:../../../tools
export LD_LIBRARY_PATH=../../../lib/.libs
export LIRC_PLUGIN_PATH=../../../plugins/.libs

here=$( dirname $( readlink -fn $0))
cd $here

exec &> ../../var/$basename.log
set -x

irsimreceive  $basename.conf durations > decoded1.out
diff decoded.txt decoded1.out || exit 2
find_reps < decoded1.out > decoded2.out
irsimsend   -s 100000 -c 5 -l decoded2.out $basename.conf >/dev/null
irsimreceive  $basename.conf simsend.out > decoded3.out

diff decoded1.out decoded3.out
