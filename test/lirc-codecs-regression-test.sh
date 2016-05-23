#!/usr/bin/env /bash
#
# This script is used for lirc protocol encode/decode regression testing
# between two versions of lirc. You'll need two source trees, both fully
# built, and a link to the remotes database
#
# Usage sketch for post 0.9.2:
#
#  -- Build and install the irpipe kernel driver according to README:
#  $ cd drivers/irpipe
#  $ cat README
#
#  -- Create a work directory
#  $ mkdir regression-test
#  $ cp test/lirc-codec-regression-tests.sh regression-test
#
#  -- Create the first built source tree
#  $ git checkout lirc-0_9_2a
#  $ ./autogen.sh
#  $ ./configure
#  $ make dist
#  $ cp lirc-0.9.2a.tar.gz regression-test
#  $ cd regression-test; tar xzf lirc-0.9.2a.tar.gz
#  $ cd lirc-0.9.2a
#  $ ./autogen.sh
#  $ ./configure
#  $ make
#
#  -- Create the second source tree from a different tag like above.
#
#  -- Setup up links for old an new source trees:
#  $ ln -s lirc-0.9.2a lirc-old
#  $ ln -s lirc-0-9-3pre1 lirc-new
#
#  -- Checkout the remotes and setup the link:
#  $ git clone http://git.code.sf.net/p/lirc-remotes/code lirc-remotes-code
#  $ ln -s lirc-remotes/remotes remotes
#
#  -- Run the tests on remotes in remotes/. A limitation in select()
#  -- makes it impossible to run more than 1000 tests in one chunk
#  -- (#109). This should be fixed in 0.9.3+.
#  $ ls remotes/*/*.lircd.conf | split -d - files
#  $ rm -rf output
#  $ for f in files*; do cat $f | ./lirc-codecs-regression-test.sh -l; done
#
#  -- Re-run tests on failed remotes:
#  $ find output -maxdepth 2 -mindepth 2 -type d \
#       | sed -e 's|output/|remotes/|' -e 's/$/.lircd.conf/' \
#       | ./lirc-codecs-regression-test.sh -l
#

# old known-good LIRC version
OLD="lirc-old"

# new LIRC version
NEW="lirc-new"

# where the config files are located -- these can be cloned from
# http://sourceforge.net/p/lirc-remotes/
REMOTES="remotes"

SKIP_PATTERN='remove.sh|lircmd|png$|jpg$|irman$|.tira|gif$|html$|list$'
SKIP_PATTERN="$SKIP_PATTERN"'|JPG$|htaccess|lircmd|lircrc'
logfile="/tmp/lircd.log"

DEBUG=${LIRC_DEBUG:-""}

BASENAME="unknown/unknown"

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
	test -d output/$BASENAME || mkdir -p output/$BASENAME
	echo $* >> output/$BASENAME/log
	echo $*
}


function run_lircd()
{
	local id=$1
	local device=$2
	local base=$3

	rm -f /tmp/lircd.${id}.log
	touch /tmp/lircd.${id}.log
	$base/daemons/lircd --pidfile=/tmp/lircd.${id}.pid \
                            --logfile=/tmp/lircd.${id}.log \
                            --output=/tmp/lircd.${id}.socket \
                            --device=$device \
                            --driver=default \
                            --plugindir=$base/plugins/.libs \
                            --debug=8 \
                            --nodaemon \
                            /tmp/lircd.${id}.conf
}


function run_irw()
{
	local irw=$1
	local socket=$2
	local logfile=$3

	rm -f irw.stdout
	mkfifo irw.stdout
	test -n "$TAIL_PID" &&  kill $TAIL_PID 2>/dev/null || :
	test -n "$IRW_PID" && kill -usr1 $IRW_PID 2>/dev/null || :
	rm -f $logfile
	stdbuf -oL $irw $socket &>$logfile &
	IRW_PID=$!

	tail -F $logfile >irw.stdout 2>/dev/null &
	TAIL_PID=$!
	disown $TAIL_PID
	exec 7<irw.stdout
}


function log_error()
{
	test -d output/$BASENAME || mkdir -p output/$BASENAME
	cp $(find output -maxdepth 1 -type f) "output/$BASENAME"
	cp $CONFIG "output/$BASENAME"
}


function wait_for_input()
{
	fd=$1
	pattern=$2
	file=$3

	while true; do
		read  -u $fd -t 5 line
		sts=$?
		if [ $sts -ge 128 ]; then
			echo "FAIL: Timeout waiting for \"$pattern\" in $file"
			log_error
			return 1
		elif [ $sts -ne 0 ]; then
			echo "Read error on $file (!)"
			return
		fi
		case $line in
			*${pattern}*)
				test -n "$DEBUG" && echo "Found: $pattern"
				return 0
				;;
			*)
				;;
		esac
	done
}


function test_one()
{
	CONFIG=$1
	BASENAME=$(dirname $CONFIG)
	BASENAME=${BASENAME/*\//}
	BASENAME=$BASENAME/$(basename $CONFIG .lircd.conf)

	# Skip invalid configs with 'one   0  0', see
	# http://sourceforge.net/p/lirc/mailman/message/32297923
	test -e $CONFIG || {
		logecho "SKIP: file $CONFIG doesn't exist."
		rm -rf output/$BASENAME
		return
	}
	grep -q '  one *0 *0' $CONFIG && {
		logecho "SKIP:  LIRCCODE config (one '0'): $CONFIG"
		rm -rf output/$BASENAME
		return
	}
	grep -q 'regression-test: skip' $CONFIG && {
		logecho "SKIP: test suppressed by comment: $CONFIG"
		rm -rf output/$BASENAME
		return
	}
	if echo $CONFIG | grep -Eq $SKIP_PATTERN; then
		logecho "SKIP: (bad name) : $CONFIG"
		rm -rf output/$BASENAME
		return
	fi

	#
	# Send
	#
	rm -f output/durations.* output/syms.*
	nr=$((nr + 1))
	logecho "INFO: Remote: $CONFIG ($nr)"
	$NEW/tools/irsimsend -U $NEW/plugins/.libs $CONFIG >output/syms.new
	mv simsend.out output/durations.new
	$NEW/tools/../tools/irpipe --text2bin --add-sync --filter  \
	    >output/durations.new.bin <output/durations.new
	$NEW/tools/irsimsend -U $NEW/plugins/.libs $CONFIG >output/syms.old
	mv simsend.out output/durations.old
	$NEW/tools/../tools/irpipe --text2bin --add-sync --filter  \
	    >output/durations.old.bin <output/durations.old

	#
	# Receive
	#
	test -n "$DEBUG" && logecho "rec1 ($CONFIG)"

	cp $CONFIG /tmp/lircd.new.conf
	kill -HUP $(cat /tmp/lircd.new.pid)
	wait_for_input 5 'Using remote' "lircd.new.log" || return 0
	run_irw $NEW/tools/irw /tmp/lircd.new.socket output/syms.receive.new
	wait_for_input 5 'accepted new client' "lircd.new.log" || return 0
	lastsym=$(tail -1 output/syms.new)
	test -n "$DEBUG" && logecho "New lastsym: $lastsym"
	test -n "$DEBUG" && logecho "Sending to irpipe1"
	cat output/durations.new.bin >/dev/irpipe1
	test -n "$DEBUG" && logecho "Waiting for lastsym: $lastsym"
	wait_for_input 7 "$lastsym" syms.receive.new || return 0

	cp $CONFIG /tmp/lircd.old.conf
	kill -HUP $(cat /tmp/lircd.old.pid)
	wait_for_input 3 'Using remote:' "lircd.old.log" || return 0
	run_irw $NEW/tools/irw /tmp/lircd.old.socket output/syms.receive.old
	wait_for_input 3 'accepted new client' "lircd.old.log" || return 0
	lastsym=$(tail -1 output/syms.old)
	test -n "$DEBUG" && logecho "New lastsym: $lastsym"
	test -n "$DEBUG" && logecho "Sending to irpipe0"
	cat output/durations.old.bin >/dev/irpipe0
	test -n "$DEBUG" && logecho "Waiting for lastsym: $lastsym"
	wait_for_input 7 "$lastsym" syms.receive.old || return 0

#
# Check results
#
	test -n "$DEBUG" && logecho "Checking results - $CONFIG"

	if ! test -s output/syms.new; then
		logecho "FAIL: irsimsend without output: $CONFIG"
		log_error; continue
	fi
	diff output/syms.old output/syms.new > syms.diff
	if test -s syms.diff; then
		logecho "FAIL: irsimsend sends different for old/new"
		log_error; continue
	fi
	if ! test -s output/syms.receive.old; then
		logecho "FAIL: nothing received using old lirc"
		log_error; continue
	fi
	if ! test -s output/syms.receive.new; then
		logecho "FAIL: nothing received using new lirc"
		log_error $CONFIG; continue
	fi
	diff output/syms.receive.old output/syms.receive.new \
		>output/syms.receive.diff
	if test -s output/syms.receive.diff; then
		sleep 1.0
		diff output/syms.receive.old output/syms.receive.new \
			>output/syms.receive.diff
	fi
	if test -s output/syms.receive.diff; then
		logecho "FAIL: old/new received data differs"
		log_error; continue
	fi
	logecho "OK: tests passed."
	rm -rf output/$BASENAME
}


function on_exit()
{
	all_pids=
	all_pids="$(cat /tmp/lircd*.pid 2>/dev/null)" || all_pids=""
	all_pids="$all_pids $LIRCD_PIDS $TAIL_PID $IRW_PID"
	test -z "$all_pids" && return
	for pid in $all_pids; do
		disown $pid &>/dev/null || :
		kill $pid &>/dev/null || :
	done

}

trap "" usr1
trap on_exit EXIT ERR

if [ "$1" = '-c' ]; then
	cleanup
fi

# Start the OLD lircd instance, logging on &3
rm -f lircd.old.stdout
mkfifo lircd.old.stdout
(run_lircd 'old' /dev/irpipe0 $OLD  >lircd.old.stdout) &
LIRCD_PIDS=$!
exec 3<lircd.old.stdout
rm -f /tmp/lircd.old.log; touch /tmp/lircd.old.log
tail -f /tmp/lircd.old.log >lircd.old.stdout &
LIRCD_PIDS="$LIRCD_PIDS $!"
wait_for_input 3  'ready'

# Start the NEW lircd instance, logging on &5
rm -f lircd.new.stdout
mkfifo lircd.new.stdout
(run_lircd 'new' /dev/irpipe1 $NEW  >lircd.new.stdout) &
LIRCD_PIDS="$LIRCD_PIDS $!"
exec 5<lircd.new.stdout
rm -f /tmp/lircd.new.log; touch /tmp/lircd.new.log
tail -f /tmp/lircd.new.log >lircd.new.stdout &
LIRCD_PIDS="$LIRCD_PIDS $!"
wait_for_input 5  'ready'

nr=0

if [ "$1" = '-l' ]; then
	while read
	do
		test -n "$REPLY" && test_one $REPLY
	done
	find output -type d -empty -delete
	exit 0
fi

find -L $REMOTES -type f -print0 -name \*.lircd.conf | xargs -0 -n 1 echo |
	while read
	do
		test_one $REPLY
	done

find output -type d -empty -delete
