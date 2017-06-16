#!/bin/bash

##set -x
##env > testenv

export PATH=$PATH:../../../tools
export LD_LIBRARY_PATH=../../../lib/.libs
export LIRC_PLUGIN_PATH=../../../plugins/.libs

here=$( dirname $( readlink -fn $0))
cd $here

exec &> ../../var/space-enc-1.log
set -x

irsimreceive  119420.conf durations > decoded1.out
irsimsend   -l decoded1 119420.conf  >/dev/null
irsimreceive  119420.conf simsend.out > decoded2.out

diff decoded1.out decoded2.out


