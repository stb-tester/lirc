[DRIVER LOADING]
Drivers are loaded dynamically. The directory used for this is determined by (falling
priority):
.IP \- 2
The --plugindir option if available.
.IP \- 2
The 'plugindir' entry in  the [lircd] section of the lirc_options.conf file.
.IP \- 2
The environment variable LIRC_PLUGINDIR.
.IP \- 2
A hardcoded default (usually /usr/lib[64]/lirc/plugins).

