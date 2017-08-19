''' Read-only configuration database. '''
##
#   @file database.py
#   @author Alec Leamas
#   @brief python read-only view on configuration data.
#   @ingroup  database

##
#
# @defgroup database python configuration database
#
# Python access to the configuration data.
#
# The database is loaded from some YAML files:
#
#   - kernel-drivers.yaml: Static Info on the kernel drivers. Availability
#     of these drivers is checked in runtime before being imported into db.
#   - drivers.yaml, Info on the userspace drivers, collected from their
#     compiled information by configs/Makefile using lirc-lsplugins(1).
#   - confs_by_driver.yaml: Mapping of drivers -> suggested remote files,
#     created by configs/Makefile using irdb-get.
#
# The directory used to load these files is (first match used):
#   - Current directory
#   - The 'configs' dir.
#   - The ../../configs
#
# Although python cannot guarantee this, the database is designed as a
# read-only structure.
#
# A simple usage examples is doc/data2table. The lirc-setup script
# provides a more elaborated example. Data structures are basically
# documented in the yaml files.

##  @addtogroup database
#   @{


import glob
import os
import os.path
import subprocess
import sys

try:
    import yaml
except ImportError:
    _YAML_MSG = '''
"Cannot import the yaml library. Please install the python3
yaml package, on many distributions known as python3-PyYAML. It is also
available as a pypi package at https://pypi.python.org/pypi/PyYAML.'''
    print(_YAML_MSG)
    sys.exit(1)

import config


def _here(path):
    ''' Return path added to current dir for __file__. '''
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), path)


def _load_kerneldrivers(configdir):
    ''' Parse the kerneldrivers.yaml file, discard unavailable
    drivers.
    '''

    with open(os.path.join(configdir, "kernel-drivers.yaml")) as f:
        cf = yaml.load(f.read())
    drivers = cf['drivers'].copy()
    for driver in cf['drivers']:
        if driver == 'default':
            continue
        with open('/dev/null', 'w') as f:
            try:
                subprocess.check_output([config.MODINFO, driver],
                                        stderr=f)
            except subprocess.CalledProcessError:
                del drivers[driver]
    return drivers


class ItemLookupError(Exception):
    """A lookup failed, either too namy or no matches found. """
    pass


class Config(object):
    ''' The configuration selected, and it's sources. '''
    # pylint: disable=too-many-instance-attributes

    def __init__(self, cf=None):
        self.device = None        ## selected device.
        self.driver = {}          ## Read-only driver dict in db
        self.config = {}          ## Read-only config dict in db
        self.modinit = ""         ## Substituted data from driver
        self.modprobe = ""        ## Substituted data from driver
        self.lircd_conf = ""      ## Name of lircd.conf file
        self.lircmd_conf = ""     ## Name of lircmd.conf file
        self.label = ""           ## Label for driver + device
        if cf:
            for key, value in cf.items():
                setattr(self, key, value)

    @property
    def note(self):
        ''' The possible note to display when selected. '''
        if 'note' in self.config:
            return self.config['note']
        else:
            return None


class Database(object):
    ''' Reflects the *.yaml files in the configs/ directory. '''

    def __init__(self, path=None, yamlpath=None):

        devel_path = _here('../../configs')
        installed_path = os.path.join(config.DATADIR, 'lirc','configs')
        if path and os.path.exists(path):
            configdir = path
        elif path:
            raise FileNotFoundError(path)
        elif os.path.exists(devel_path):
            configdir = devel_path
        elif os.path.exists(installed_path):
            configdir = installed_path
        else:
            raise FileNotFoundError(devel_path + ':' + installed_path)
        if not yamlpath:
            yamlpath = configdir
        db = {}
        with open(os.path.join(yamlpath, "confs_by_driver.yaml")) as f:
            cf = yaml.load(f.read())
        db['lircd_by_driver'] = cf['lircd_by_driver'].copy()
        db['lircmd_by_driver'] = cf['lircmd_by_driver'].copy()

        db['kernel-drivers'] = _load_kerneldrivers(configdir)
        db['drivers'] = db['kernel-drivers'].copy()
        with open(os.path.join(yamlpath, "drivers.yaml")) as f:
            cf = yaml.load(f.read())
        db['drivers'].update(cf['drivers'].copy())
        for key, d in db['drivers'].items():
            d['id'] = key
            hint = d['device_hint']
            if not hint:
                continue
            hint = hint.strip()
            if hint.startswith('"') and hint.endswith('"'):
                hint = hint[1:-1]
                hint = hint.replace(r'\"', "@$#!")
                hint = hint.replace('"', '')
                hint = hint.replace("@$#!", '"')
                hint = hint.replace("\\\\", "\\")
            d['device_hint'] = hint

        configs = {}
        for path in glob.glob(configdir + '/*.conf'):
            with open(path) as f:
                cf = yaml.load(f.read())
            configs[cf['config']['id']] = cf['config']
        db['configs'] = configs
        self.db = db

    @property
    def kernel_drivers(self):
        ''' The kernel-drivers dictionary, drivers.yaml + kernel-drivers.yaml.
        '''
        return self.db['kernel-drivers']

    @property
    def drivers(self):
        ''' The drivers dictionary, drivers.yaml + kernel-drivers.yaml. '''
        return self.db['drivers']

    @property
    def configs(self):
        ''' Return dict of parsed config/*.conf files, keyd by id. '''
        return self.db['configs']

    def remotes_by_driver(self, driver):
        ''' Return the list of remotes suggested for a given driver. '''
        if isinstance(driver, dict):
            driver = driver['id']
        try:
            return self.db['lircd_by_driver'][driver]
        except KeyError:
            return []

    def lircmd_by_driver(self, driver):
        ''' Return list of lircmd.conf file for given driver or None. '''
        if isinstance(driver, dict):
            driver = driver['id']
        try:
            return self.db['lircmd_by_driver'][driver]
        except KeyError:
            return []

    def driver_by_remote(self, remote):
        ''' Return the driver (possibly None) suggested for a remote. '''
        for driver, files in self.db['lircd_by_driver'].items():
            if remote in files:
                return self.db['drivers'][driver]
        return None

    def find_config(self, key, value):
        ''' Return item (a config) in configs where config[key] == value. '''
        found = [c for c in self.db['configs'].values()
                 if key in c and c[key] == value]
        if len(found) > 1:
            raise ItemLookupError(
                "find_config: Too many matches for %s, %s): " % (key, value)
                + ', '.join([c['id'] for c in found]))
        elif not found:
            raise ItemLookupError(
                "find_config: Nothing  found for %s, %s): " % (key, value))
        found = dict(found[0])
        if 'device_hint' not in found:
            try:
                found['device_hint'] = \
                    self.db['drivers'][found['driver']]['device_hint']
            except KeyError:
                found['device_hint'] = \
                    self.db['kernel-drivers'][found['driver']]['device_hint']
        return found

##  @}
#   database

# vim: set expandtab ts=4 sw=4:
