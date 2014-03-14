[NAME]
lircd - LIRC daemon decodes infrared signals and provides them on a Unix
domain socket.

[DESCRIPTION]
The main task of lircd is to decode the infrared signals and provide
an uniform interface for client applications. Clients can connect to
lircd through a Unix domain socket which is located in
\/var/run/lirc/lircd.  Using this socket they will get the infrared
codes received by lircd and they can send commands to lircd.

[OPTIONS]
Common options:
.TP 4
.B -d, --device
Select the character device which lircd should read from. The default
is currently /dev/lirc0 on most systems.
.IP "" 4
If you're using the dev/input driver, you can use \fIname=STRING\fR or
\fIphys=STRING\fR to select the device; lircd will look in /dev/input
to find a device with a matching description. This is useful in case
the device name isn't fixed. \fISTRING\fR may contain the '*' and '?'
wildcards and '\\' to mark them as literal. Use dmesg(1) to find the
name and bus address (the names used under /dev/input/by-id are not
always usable).
.TP
.B -H, --driver
The driver to use.  Using
.I --driver help lists
all available drivers, see DRIVER LOAD PATH beelow.
.TP
.B -h, --help
Displays short thelp message.
.IP "" 0
Other options:
.TP 4
.B -a, --allow-simulate
Enable the SIMULATE command which can
be issued using irsend(1). This will allow simulating arbitrary IR events
from the command line. Use this option with caution because it will give all
users with access to the lircd socket wide control over you system.
E.g. if you have configured your system to shut down by a button press
on your remote control, everybody will be able to shut down
your system from the command line.
.TP 4
.B -c, --connect [host[:port]]
Allows you to connect to other lircd servers that provide a network
socket at the given host and port number. The number
of such connections is currently limited to 100.
The connecting lircd instance will receive IR events from the lircd
instance it connects to.
.TP 4
.B -l, --listen [[address:]port]]
Let lircd listen for network
connections on the given address/port. The default address is 0.0.0.0,
which means that connections on all network interfaces will be accepted.
The default port is 8765. No security checks are currently implemented.
The listening lircd instance will send all IR events to the connecting
lircd instances.
.TP 4
.B -L, --logfile
Select the lircd daemon log file.  The default currently is
/var/log/lircd. Note that this option will only be available if you
compiled lircd without syslog support.
.TP 4
.B -o, --output <output socket>
Select Unix domain socket, which lircd will write remote key codes to.
The default currently is \/var/run/lirc/lircd.
.TP 4
.B \-p, --permission
Gives the file permission of \/var/run/lirc/lircd if it has to be
created in octal representation. See chmod(1).
If no \-\-permission option is given when the
socket is initially created the default is to give all users read
and write permissions (0666 in octal representation). If
/var/run/lirc/lircd already exists this option has no effect.
.TP 4
.B -p, --pidfile <path>
Select the lircd daemon pid file, defaults to /var/run/lirc/lircd.pid.
.TP 4
.B -r, --release [suffix]
Enables automatic generation of release events for each button press.
 lircd will append the given suffix to the button name for each release
event. If no suffix is given the default suffix is '_UP'.
.TP
.B -R, --repeat-max <limit>
Sets an upper limit to the number of repeats when sending a signal. The
current default is 600. A SEND_START request will repeat the signal this
many times. Also, if the number of repeats in a SEND_ONCE request exceeds
this number, it will be replaced by this number.
.TP
.B -u, --uinput
Enable automatic generation
of Linux input events. lircd will open /dev/input/uinput and inject
key events to the Linux kernel. The key code depends on the name that
was given a button in the lircd config file, e.g. if the button is
named KEY_1, the '1' key code will be generated. You will find a
complete list of possible button names in /usr/include/linux/input.h.

[FILES]

The config file for lircd is located in /etc/lirc/lircd.conf. lircd
has its own log file in /var/log/lircd (beginning with LIRC version
0.6.1 you can configure lircd to use syslogd for log messages; then it
depends on your system configuration where log messages will show up).
You can make lircd reread its config file and reopen its log file by
sending the HUP signal to the program. That way you can rotate old log
files.

