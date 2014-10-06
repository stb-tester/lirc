#!/bin/sh

##set -x
##env > testenv

basename=301_501_3100_5100_58xx_59xx

export PATH=$PATH:../../../tools
export LD_LIBRARY_PATH=../../../lib/.libs
export LIRC_PLUGIN_PATH=../../../plugins/.libs

here=$( dirname $( readlink -fn $0))
cd $here

exec &> ../../var/space-enc-1.log
set -x

irsimreceive -U $LIRC_PLUGIN_PATH  $basename.conf durations > decoded1.out
irsimsend   -U $LIRC_PLUGIN_PATH -s 100000 -l decoded1.out $basename.conf >/dev/null
irsimreceive  -U $LIRC_PLUGIN_PATH $basename.conf simsend.out > decoded2.out

diff decoded1.out decoded2.out


