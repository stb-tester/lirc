#!/bin/sh

function find_reps()
{

    awk ' 
    BEGIN                   { reps = -1; button = "__nil" 
                              first_reps = -1; first_button = "__nil" 
                              print_on_exit = 1
                            }
    first_button == "__nil" { first_button = $3; 
                              first_reps = 0
                            }
    $3 == first_button      { first_reps += 1}
    $3 != button            { button = $3 
                              if (reps != first_reps && reps != -1) {
                                  print "Button " button \
                                             " repeats " reps " times"
                                  print "But first button repeats " \
                                      first_reps " times"
                                  print_on_exit = 0
                                  exit(2)
                              }
                              reps = 0
                              print $0
                            }
    $3 == button            { reps += 1}
    END                     { if (print_on_exit)
                                  print "All keys repeated: " first_reps  \
                                      >/dev/stderr
                            }'

}


##set -x
##env > testenv

basename='SR-90'

export PATH=$PATH:../../../tools
export LD_LIBRARY_PATH=../../../lib/.libs
export LIRC_PLUGIN_PATH=../../../plugins/.libs

here=$( dirname $( readlink -fn $0))
cd $here

##exec &> ../../var/$basename.log
##set -x

irsimreceive -U $LIRC_PLUGIN_PATH  $basename.conf durations > decoded1.out
diff decoded.txt decoded1.out || exit 2
find_reps < decoded1.out > decoded2.out
irsimsend   -U $LIRC_PLUGIN_PATH -s 100000 -c 5 -l decoded2.out $basename.conf >/dev/null
irsimreceive  -U $LIRC_PLUGIN_PATH $basename.conf simsend.out > decoded3.out

diff decoded1.out decoded3.out
