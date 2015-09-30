''' Simple lirc setup tool - utilities. '''

import glob
import grp
import os.path
import os
import re

DEVICE_PERMISSIONS_MSG = """
The device {device} is not accessible. A quick fix during test is to run
<i>sudo chmod 666 {device}</i>, but in the long term you need to adjust
the device permissions in a more permanent way.
"""

GROUP_PERMISSIONS_MSG = """
The device {device} cannot be accessed but is accessible by group {group}.
One option is to run both lirc-setup and later on lircd with {group}
as a supplementary group. For a quick fix now, run
<i>sudo usermod -a -G {group} {user}</i> and <i>sg {group} lirc-setup</i>
to restart lirc-setup.
"""

ROOT_PERMISSIONS_MSG = """
This device only is accessible by root. You might want to
install a udev rule, see the Configuration Guide, "Adjusting
kernel device permissions". A quick fix during testing is to
run <i>sudo chmod 666 {device}</i>
"""


BAD_PROTOCOL_MSG = """
The current protocol used by {device} is not 'lirc'. In this mode, neither
tests nor actually running lircd will work. For a quick fix run
<i>sudo /bin/sh -c "echo lirc > {path}"</i>. lirc-setup will configure
the [modinit] section to handle this in regular start of the lircd service.
"""

LIRC_PROTOCOL_MSG = """
The {device} device runs the 'lirc' protocol. This will not work, in this
mode the kernel driver sends all input to /dev/lirc* devices. If you have
changed the mode while trying the default driver, just pull out and re-insert
the capture device. If this doesn't work, you need to review your kernel
configuration, in particular if there is a udev rule which changes the
protocol. Out of the box, the kernel will not enable the lirc protocol.
"""

NO_RC_DIR_MSG = """
I cannot find a /sys/class/rc dir for %s, so the protocol used
(lirc/not lirc) cannot be checked.
"""

MANY_RC_DIRS_MSG = """
There are multiple /sys/class/rc dirs holding %s. This is insane, giving up.
"""


def get_rcdir_by_device(device):
    ''' Return the /sys/class/rc directory behind a /dev/lirc  or
    /dev/input/event* device.
    '''

    basename = os.path.basename(device)
    level = 1
    rc_dirs = glob.glob("/sys/class/rc/*/" + basename)
    if len(rc_dirs) != 1:
        level = 2
        rc_dirs = glob.glob("/sys/class/rc/*/*/" + basename)
    if len(rc_dirs) == 0:
        raise LookupError(NO_RC_DIR_MSG % device)
    if len(rc_dirs) > 1:
        raise LookupError(MANY_RC_DIRS_MSG % device)
    for dummy in range(0, level):
        rc_dirs[0] = os.path.dirname(rc_dirs[0])
    return rc_dirs[0]


def check_kerneldevice(device):
    ''' Check that device has OK permissions and possibly protocol. '''
    if os.path.exists(device) and not os.access(device, os.R_OK | os.W_OK):
        msg = DEVICE_PERMISSIONS_MSG.format(device=device)
        stat = os.stat(device)
        if stat[4:6] == (0, 0):
            msg = ROOT_PERMISSIONS_MSG.format(device=device)
        elif stat[0] & 0o60 == 0o60 and stat[5] != 0:
            group = grp.getgrgid(stat[5])[0]
            msg = GROUP_PERMISSIONS_MSG.format(device=device,
                                               user=os.environ['USER'],
                                               group=group)
        return msg

    if not ("/dev/lirc" in device or "/dev/input" in device):
        return None

    try:
        rc_dir = get_rcdir_by_device(device)
    except LookupError as ex:
        return str(ex)
    path = os.path.join(rc_dir, "protocols")
    with open(path, "r") as f:
        protocols = f.readlines()[0].rstrip()
    current = re.search(r'[[](.*)[\]]', protocols).group(1)
    if current != 'lirc' and "/dev/lirc" in device:
        return BAD_PROTOCOL_MSG.format(device=device, path=path)
    elif current == 'lirc' and "/dev/input" in device:
        return LIRC_PROTOCOL_MSG.format(device=device)
    return None


# vim: set expandtab ts=4 sw=4:
