#!/usr/bin/env python3

import sys
import yaml


with open('hardware.yaml') as f:
    hw = yaml.load(f.read())
driver_submenus =  \
    hw['lirc']['main_menu']['submenus']['driver_select']['submenus']
for menu in driver_submenus:
    try:
        for item in driver_submenus[menu]['submenus']:
            hw['lirc']['remotes'][item]['menu'] = menu
    except KeyError:
        continue
for remote in hw['lirc']['remotes']:
    path = remote + '.conf'
    with open(path, 'w') as f:
        f.write("# This is a lirc configuration for a capture device.\n")
        f.write("# See README.conf  for more.\n")
        f.write("\n")
        f.write("config:\n")
        hw['lirc']['remotes'][remote]['id'] = remote
        for key in sorted(hw['lirc']['remotes'][remote]):
            value = hw['lirc']['remotes'][remote][key]
            if key == 'device':
                if value.startswith('run_select_usb_tty'):
                    value = '/dev/ttyUSB*'
                elif value.startswith('run_select_tty'):
                    value = '/dev/tty[0-9]*'
            s = "    %-16s%s\n" % (key + ':', value)
            f.write(s)



