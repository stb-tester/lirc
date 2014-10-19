#!/usr/bin/env python3

''' Simple lirc setup tool. '''

YAML_MSG = '''
"Cannot import the yaml library. Please install the python3
yaml package, on many distributions known as python3-PyYAML. It is also
available as a pypi package at https://pypi.python.org/pypi/PyYAML.'''

import ast
import configparser
import glob
import os
import os.path
import subprocess
import sys
import urllib.error          # pylint: disable=no-name-in-module,F0401,E0611
import urllib.request        # pylint: disable=no-name-in-module,F0401,E0611
try:
    import yaml
except ImportError:
    print(YAML_MSG)
    sys.exit(1)

from gi.repository import Gtk         # pylint: disable=no-name-in-module
from gi.repository.Pango import FontDescription  # pylint: disable=F0401,E0611

REMOTES_LIST = os.path.expanduser('~/.cache/remotes.list')
REMOTES_DIR = os.path.expanduser("~/.cache/remotes.pickles")
REMOTES_LIST_URL = "http://lirc-remotes.sourceforge.net/remotes.list"
REMOTES_DIR_URL = 'http://lirc-remotes.sourceforge.net/remotes.pickle'
_REMOTES_BASE_URI = "http://sf.net/p/lirc-remotes/code/ci/master/tree/remotes"
_OPTIONS_PATH = "/etc/lirc/lirc_options.conf"
_RESULTS_DIR = 'lirc-setup.conf.d'
_MODINIT_PATH = "lirc-modinit.conf"
_DEBUG = 'LIRC_DEBUG' in os.environ

_USAGE = "Usage: lirc-setup [results directory]"

NO_REMOTE_INFO = """
If you select this option, you need to provide a lircd.conf file
later, either by finding it elsewhere or by recording your own using
the irrecord tool. Normally you want to install this file as
/etc/lirc/lircd.conf."""

DEVINPUT_INFO = """
The devinput driver uses the linux kernel decoding rather than lirc's.
It does not support as many devices as lirc, but for supported devices
this is an easy setup. It has some limitations, notably it does not
support sending IR signals (ir blasting)."""

DEFAULT_INFO = """
The default driver uses the kernel IR drivers, but does it's own
decoding. It can be used with most devices supported by the kernel
and also some other devices, notably many serial and parallell ones.
It needs to be configured which is normally more work than the devinput
driver, but supports more devices and full functionality."""

PRECONFIG_INFO = """
For some devices which cannot be used with the default or the devinput
driver lirc has specific support, often a lirc driver and/or kernel
configuration of the standard driver(s). If you can find your remote here
it will configure this support. """

MAIN_HELP = """
NOTE: This tool is in early alpha stage! Here are numerous bugs and
shortcomings. Please report issues on the mailing list or in the
issue tracker at https://sourceforge.net/projects/lirc/!

The tool allows you to configure lirc. This is done in three steps.

In the first you should select a configuration file which corresponds to
your remote. You can search for remotes from existing ones, browse brands
or select to not use any pre-configured remote. In the last case you
probably wants to record your own configuration file using irrecord(1)
later.  This is the top pane of the window

In the second step you should select a driver which can handle your
capture device e. g., a usb dongle or a home-made serial device.  This
is the bottom pane of the window.

Actually, it doesn't really matter if you select remote or capture device
first. You can do it in any order.

If you select a remote which only can be used with a specific capture
device the capture device will be updated automagically. Likewise, if you
select a capture device which requires a specific remote the remote will be
updated.

In the last step you should install the configuration. This will write a set
of configuration files to the results directory, normally lirc-setup.conf.d.
You should then install these files into their proper locations, see the
README file in the results directory."""

README = """
This is some configuration files created by lirc-setup. To install them,
try the following commands:

sudo cp lircd_options.conf /etc/lirc/lircd_options.conf
sudo cp lirc-modinit.conf /etc/modprobe.d
sudo cp lircd.conf /etc/lirc/lircd.conf
sudo cp lircmd.conf /etc/lirc/lircmd.conf

Of course, if you already have a working configuration don't forget to
make backup copies as required! Note that all files are not always present.
"""


def _hasitem(dict_, key_):
    ''' Test if dict contains a non-null value for key. '''
    return key_ in dict_ and dict_[key_]


def _here(path):
    ' Return path added to current dir for __file__. '
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), path)


def _parse_options():
    ''' Parse command line optios into a returned dict. '''
    options = {}
    if len(sys.argv) == 1:
        options['results_dir'] = _RESULTS_DIR
    elif len(sys.argv) == 2:
        options['results_dir'] = sys.argv[1]
    else:
        sys.stderr.write(_USAGE)
        sys.exit(1)
    return options

# Model stuff


def find_rc(lirc):
    ''' Return the /sys/class/rc/rc* device corresponding to lirc device. '''
    lirc = os.path.basename(lirc)
    for rc in glob.glob('/sys/class/rc/rc*'):
        if os.path.exists(os.path.join(rc, lirc)):
            return rc
    return None


def download_file(view, url, path):
    ''' Download location url to a file. '''
    try:
        urllib.request.urlretrieve(url, path)
    except urllib.error.HTTPError as ex:
        text = "Cannot download %s : %s" % (url, str(ex))
        view.show_warning('Download error', text)


def get_bundled_remotes(config, view):
    ''' Return the bundled remote file for a device config dict,
    a possibly empty list of remote ids.
    '''
    found = []
    if _hasitem(view.model.config, 'driver'):
        driver = view.model.config['driver']
        for l in view.model.get_remotes_list(view):
            words = l.split(';')
            if words[7] == driver:
                found.append(words[0] + "/" + words[1])
    if 'supports' not in config:
        return found
    if config['supports'] == 'bundled' and 'lircd_conf' in config:
        found = [config['lircd_cond']] + found
    return found


def get_dmesg_help(device):
    ''' Return dmesg lines matching device.'''
    lines = subprocess.check_output('dmesg').decode('utf-8').split('\n')
    rc = find_rc(device)
    if rc:
        rc = os.path.basename(rc)
    else:
        rc = 'fjdsk@$'
    dev = os.path.basename(device)
    return [l for l in lines if dev in l or rc in l]


def get_lines_by_letter(lines):
    '''' Return a dictionary keyed with first letter of lines. '''

    all_keys = [chr(c) for c in range(ord('a'), ord('z') + 1)]
    all_keys.extend([chr(c) for c in range(ord('0'), ord('9') + 1)])
    lines_by_letter = {}
    for c in all_keys:
        lines_by_letter[c] = []
    for l in lines:
        lines_by_letter[l[0].lower()].extend([l])
    return lines_by_letter


def list_ttys():
    ''' List all currently used ttys on this host. '''

    def driver_path(s):
        ''' Path to driver directory for given /class/tty/x device,. '''
        return os.path.join(s, 'device', 'driver')

    # (Not tested)
    # def is_inactive_5250(s):
    #     try:
    #         driver = os.path.basename(os.readlink(driver_path(s)))
    #     except (IOError, OSError):
    #         return False
    #     return driver != 'serial5250'

    syslist = glob.glob("/sys/class/tty/*")
    syslist = [s for s in syslist if os.path.exists(driver_path(s))]
    # syslist = [s for s in syslist if not is_inactive_5250(s)]
    devices = ["/dev/" + os.path.basename(s) for s in syslist]
    return devices


def _write_results(config, result_dir, view):
    ''' Write the set of new configuration files into results directory, '''
    # pylint: disable=too-many-branches

    def write_modinit(log):
        ''' Possibly write the modprobe.d config file. '''
        if not _hasitem(config, 'modsetup'):
            return log
        modinit = '# Generated by lirc-setup\n'
        modinit += config['modsetup'] + '\n'
        path = os.path.join(result_dir, _MODINIT_PATH)
        with open(path, 'w') as f:
            f.write(modinit)
        log += 'Info: modprobe.d configuration: %s\n' % config['modsetup']
        return log

    def write_blacklist(log):
        ''' Possibly update blacklist in the /etc/modprobe.d file. '''
        if not _hasitem(config, 'blacklist'):
            return log
        blacklist = '# Generated by lirc-setup\n'
        blacklist += config['blacklist'] + '\n'
        path = os.path.join(result_dir, _MODINIT_PATH)
        with open(path, 'a') as f:
            f.write(blacklist)
        log += \
             'Info: modprobe.d blacklist config: %s \n' % config['blacklist']
        return log

    def write_options(options, log):
        ''' Update options in new lirc_options.conf. '''
        inited = False
        if not options.has_section('lircd'):
            options.add_section('lircd')
        if not _hasitem(config, 'lircd_conf'):
            log += 'Warning: No lircd.conf found, requierd by lircd.\n'
        for opt in ['device', 'lircd_conf', 'lircmd_conf']:
            if not _hasitem(config, opt):
                continue
            if not inited:
                log += 'Info: new values in lircd_options.conf\n<tt>'
                inited = True
            options.set('lircd', opt, config[opt])
            log += "%-16s: %s\n" % (opt, config[opt])
        if _hasitem(config, 'modprobe'):
            if not options.has_section('modprobe'):
                options.add_section('modprobe')
            options.set('modprobe', 'modules', config['modprobe'])
            log += "%-16s: %s\n" % ('modules', config['modprobe'])
        log += '</tt>'
        path = os.path.join(result_dir, 'lirc_options.conf')
        with open(path, 'w') as f:
            f.write("# Generated by lirc-setup\n")
            options.write(f)
        return log

    def get_configfiles(log):
        ''' Download lircd.conf and perhaps lircmd.conf, '''

        def error(ex, uri):
            ''' Handle download error. '''
            text = "Cannot download %s : %s" % (uri, str(ex))
            view.show_error("Download error", text)

        if 'lircd_conf' not in config or not config['lircd_conf']:
            text = "No lircd.conf defined, skipping"
            view.show_warning("Download error", text)
            return log
        for item in ['lircd_conf', 'lircmd_conf']:
            if item not in config:
                continue
            uri = os.path.join(_REMOTES_BASE_URI, config[item])
            path = os.path.join(result_dir, item.replace('_', '.'))
            try:
                urllib.request.urlretrieve(uri + '?format=raw', path)
                log += 'Info: Downloaded %s to %s\n' % (str(uri), str(path))
            except urllib.error.HTTPError as ex:
                error(ex, uri)
        return log

    options = configparser.RawConfigParser()
    options.read(_OPTIONS_PATH)
    log = 'Writing installation files in %s\n' % result_dir
    log = write_options(options, log)
    log = write_modinit(log)
    log = write_blacklist(log)
    log = get_configfiles(log)
    path = os.path.join(result_dir, 'README')
    with open(path, 'w') as f:
        f.write(README)
    view.show_info('Installation files written', log)


def _check_resultsdir(dirpath):
    ''' Check that dirpath is ok, return possibly empty error message.'''
    if not os.path.exists(dirpath):
        try:
            os.makedirs(dirpath)
            return ''
        except os.error as err:
            return "Cannot create directory %s:" % dirpath + str(err)
    elif not os.listdir(dirpath):
        if os.access(dirpath, os.W_OK):
            return ''
        else:
            return "Directory %s is not writeable" % dirpath
    else:
        return "Directory %s is in the way. Please remove it or use" \
            " another directory." % dirpath


def _check_modules(config):
    ''' Check modules options, return results as a string. '''
    if 'modules' not in config or not config['modules']:
        return ''
    with open('/proc/modules') as f:
        all_modules = f.readlines()
    all_modules = [m.split()[0] for m in all_modules]

    modules = ast.literal_eval(config['modules'])
    if not isinstance(modules, list):
        modules = []

    s = ''
    for m in modules:
        cmd = ['sh -c "find /lib/modules/$(uname -r) -name %s.ko"' % m]
        try:
            found = subprocess.check_output(cmd, shell=True).decode('utf-8')
        except (OSError, subprocess.CalledProcessError):
            found = []
        else:
            found = found.strip()
        if m in found:
            s += 'modules: %s: OK, module exists\n' % m
            if m in all_modules:
                s += 'modules: %s: OK, module is loaded\n' % m
            else:
                s += 'modules: %s: Info, module is not loaded.\n' % m
                if 'modprobe' not in config:
                    config['modprobe'] = list()
                config['modprobe'].extend([m])
        else:
            s += 'modules: %s: Error, module does not exist\n' % m
    return s


class DeviceListModel(object):
    ''' The list of devices corresponding to the device: wildcard in config.'''
    AUTO_CONFIG = "atilibusb"   # A driver configuration with device: auto.

    def __init__(self, config):
        self.driver_id = config['id']
        self.config = config
        self.device_pattern = config['device']
        self.label_by_device = {}
        self.list_devices()
        if not self.label_by_device:
            self.label_by_device = {}

    def list_devices(self):
        ''' List all available devices. '''
        assert self is True, 'Invalid call to abstract list_devices()'

    def is_direct_installable(self):
        ''' Return True  if this can be installed without a dialog. '''
        return len(self.label_by_device) == 1

    def is_empty(self):
        ''' Return true if there is no matching device.'''
        return len(self.label_by_device) == 0


class EventDeviceListModel(DeviceListModel):
    ''' List of devinput /dev/input/ devices . '''

    def __init__(self, model):
        config = model.find_config('id', 'devinput')
        DeviceListModel.__init__(self, config)

    def list_devices(self):
        ''' Return a dict label_by_device, labels from /input/by-id. '''
        self.label_by_device = {}
        oldcwd = os.getcwd()
        try:
            os.chdir("/dev/input/by-id/")
            for l in glob.glob("/dev/input/by-id/*"):
                try:
                    device = os.path.realpath(os.readlink(l))
                except OSError:
                    device = l
                self.label_by_device[device] = os.path.basename(l)
        except FileNotFoundError:    # pylint: disable=undefined-variable
            pass
        finally:
            os.chdir(oldcwd)


class LircDeviceListModel(DeviceListModel):
    '''  /dev/lirc? device list. '''

    def __init__(self, model):
        config = model.find_config('id', 'default')
        DeviceListModel.__init__(self, config)

    def list_devices(self):
        ''' Return a dict label_by_device, labels for /dev/lirc devices. '''
        self.label_by_device = {}
        for dev in glob.glob('/dev/lirc?'):
            rc = find_rc(dev)
            self.label_by_device[dev] = "%s (%s)" % (dev, rc)


class GenericDeviceListModel(DeviceListModel):
    ''' Generic /dev/xxx* device list. '''

    def list_devices(self):
        ''' Return a dict label_by_device, labels for matching devices. '''
        self.label_by_device = {}
        for match in glob.glob(self.config['device']):
            self.label_by_device[match] = match


class SerialDeviceListModel(DeviceListModel):
    ''' Let user select a device for a userspace serial driver. '''

    def list_devices(self):
        ''' Return a dict label_by_device, labels for matching devices. '''
        self.label_by_device = {}
        for dev in list_ttys():
            rc = find_rc(dev)
            if rc:
                self.label_by_device[dev] = "%s (%s)" % (dev, rc)
            else:
                self.label_by_device[dev] = dev


class UdpPortDeviceList(DeviceListModel):
    ''' Dummy list for the udp driver port. '''

    def __init__(self, model):
        config = model.find_config('id', 'udp')
        DeviceListModel.__init__(self, config)

    def list_devices(self):
        self.label_by_device = {'default port 8765': '8765'}

    def is_direct_installable(self):
        return False

    def is_empty(self):
        ''' Return true if there is no matching device.'''
        return False


class AutoDeviceList(DeviceListModel):
    ''' Dummy list for the driver: 'auto'  entries. '''

    def __init__(self, model):
        config = model.find_config('id', self.AUTO_CONFIG)
        DeviceListModel.__init__(self, config)

    def list_devices(self):
        self.label_by_device = \
            {'automatic device': 'automatically probed device'}

    def is_direct_installable(self):
        return True

    def is_empty(self):
        return False


def device_list_factory(config, model):
    ''' Given a device: wildcard from config, return a DeviceList. '''

    device_wildcard = config['device']
    if device_wildcard.startswith('/dev/input'):
        return EventDeviceListModel(model)
    elif device_wildcard.startswith('/dev/lirc'):
        return LircDeviceListModel(model)
    elif device_wildcard.startswith('/dev/tty'):
        return SerialDeviceListModel(config)
    elif device_wildcard == 'auto':
        return AutoDeviceList(model)
    elif device_wildcard == 'udp_port':
        return UdpPortDeviceList(model)
    else:
        return GenericDeviceListModel(config)

# View


def on_window_delete_event_cb(window, event):
    ''' Generic window close event. '''
    window.hide()
    return True


def show_text(builder, text, title="lirc: show file", on_ok_cb=None):
    ''' Read-only text display in a textview. '''

    def cb_on_view_ok_btn_clicked(button, data=None):
        ''' OK button on view_some_text window. '''
        button.get_toplevel().hide()
        if on_ok_cb:
            on_ok_cb()
        else:
            return True

    textview = builder.get_object("view_text_view")
    textview.modify_font(FontDescription("Monospace"))
    buf = textview.get_buffer()
    buf.set_text(text)
    w = builder.get_object('view_text_window')
    w.set_title(title)
    w.connect('delete-event', on_window_delete_event_cb)
    b = builder.get_object('view_text_ok_btn')
    b.connect('clicked', cb_on_view_ok_btn_clicked)
    w.set_size_request(600, 600)
    w.show_all()


class RemoteSelector(object):
    ''' Select remote, possibly using a dialog. '''

    def __init__(self, controller):
        self.view = controller.view
        self.controller = controller
        self.prev_window = \
            self.view.builder.get_object('preconfig_select_window')
        self.next_window = \
            self.view.builder.get_object('main_window')

    def select(self, remotes):
        ''' Select a remote with a driver attribute 'driver' '''

        def build_treeview(remotes):
            ''' Construct the remotes liststore treeview. '''
            treeview = self.view.builder.get_object('drv_select_remote_view')
            treeview.set_vscroll_policy(Gtk.ScrollablePolicy.NATURAL)
            if len(treeview.get_columns()) == 0:
                liststore = Gtk.ListStore(str)
                treeview.set_model(liststore)
                renderer = Gtk.CellRendererText()
                column = Gtk.TreeViewColumn('Remote', renderer, text=0)
                column.clickable = True
                treeview.append_column(column)
                treeview.get_selection().connect('changed',
                                                 on_treeview_change_cb)
            liststore = treeview.get_model()
            liststore.clear()
            for l in sorted(remotes):
                liststore.append([l])
            return treeview

        def on_select_next_cb(button, data=None):
            ''' User pushed 'Next' button. '''
            lbl = self.view.builder.get_object('drv_select_remote_lbl')
            self.controller.lircd_conf_done(lbl.get_text())
            button.get_toplevel().hide()
            self.next_window.show()

        def on_select_back_cb(button, data=None):
            ''' User pushed 'Back' button. '''
            button.get_toplevel().hide()
            self.prev_window.show()

        def on_treeview_change_cb(selection, data=None):
            ''' User selected a row i. e., a remote. '''
            (model, iter_) = selection.get_selected()
            if not iter_:
                return
            label = self.view.builder.get_object('drv_select_remote_lbl')
            label.set_text(model[iter_][0])
            b = self.view.builder.get_object('drv_select_remote_next_btn')
            b.set_sensitive(True)

        if len(remotes) == 0:
            return
        if len(remotes) == 1:
            self.controller.model.set_remote(remotes[0])
            return
        build_treeview(remotes)
        w = self.view.builder.get_object('drv_select_remote_window')
        w.connect('delete-event', on_window_delete_event_cb)
        l = self.view.builder.get_object('drv_select_remote_main_lbl')
        l.set_text("Select remote configuration for driver")
        b = self.view.builder.get_object('drv_select_remote_next_btn')
        b.connect('clicked', on_select_next_cb)
        b = self.view.builder.get_object('drv_select_remote_back_btn')
        b.connect('clicked', on_select_back_cb)
        b.set_sensitive(True)
        w.show_all()


class DeviceSelector(object):
    ''' Base class for selecting lircd  --device option setup. '''

    help_label = 'dmesg info'
    device_label = 'default driver on %device'
    intro_label = 'Select device for the TBD driver.'

    def __init__(self, device_list, view, add_help=False):
        self.label_by_device = device_list.label_by_device
        self.driver_id = device_list.driver_id
        self.view = view
        self.add_help = add_help
        self.group = None
        self.view.load_configs()
        self.on_ok_cb = None
        w = view.builder.get_object('select_dev_window')
        w.connect('delete-event', on_window_delete_event_cb)
        b = view.builder.get_object('select_dev_ok_btn')
        b.connect('clicked', self.on_ok_btn_clicked_cb)

    def on_option_btn_toggled_cb(self, button, device):
        ''' User selected a device. '''
        self.view.model.set_device('/dev/' + device)

    def on_help_clicked_cb(self, button, device):
        ''' Display dmesg output for given device. '''
        lines = get_dmesg_help(device)
        if lines:
            show_text(self.view.builder, '\n'.join(lines))
        else:
            show_text(self.view.builder,
                      'No dmesg info found for ' + device['device'])

    def on_ok_btn_clicked_cb(self, button, data=None):
        ''' User clicked OK, go ahead and select active device. '''
        for b in self.group.get_group():
            if b.get_active():
                device_dict = \
                    dict(self.view.model.find_config('id', self.driver_id))
                device_dict['label'] += ' on ' + b.lirc_name
                device_dict['device'] = b.lirc_name
                self.view.model.set_capture_device(device_dict)
                break
        else:
            if _DEBUG:
                print("No active button?!")
        btn = self.view.builder.get_object('install_btn')
        btn.set_sensitive(self.view.model.is_installable())
        button.get_toplevel().hide()
        if self.on_ok_cb:
            self.on_ok_cb()                 # pylint: disable=E1102
        return True

    def get_dialog_widget(self):
        ''' Return a grid with radio buttons selectable options. '''
        # pylint: disable=not-callable
        grid = Gtk.Grid()
        grid.set_column_spacing(10)
        radio_buttons = {}
        help_buttons = {}
        group = None
        for device, label in self.label_by_device.items():
            if len(radio_buttons) == 0:
                radio_buttons[device] = Gtk.RadioButton(label)
                group = radio_buttons[device]
            else:
                radio_buttons[device] = \
                    Gtk.RadioButton.new_with_label_from_widget(group,
                                                               label)
            radio_buttons[device].connect('toggled',
                                          self.on_option_btn_toggled_cb,
                                          device)
            size = len(radio_buttons)
            grid.attach(radio_buttons[device], 0, size, 1, 1)
            radio_buttons[device].lirc_name = device
            if self.add_help:
                help_buttons[device] = Gtk.Button(self.help_label)
                help_buttons[device].connect('clicked',
                                             self.on_help_clicked_cb,
                                             device)
                grid.attach(help_buttons[device], 1, size, 1, 1)

        self.group = group
        l = self.view.builder.get_object('select_dev_label')
        l.set_text(self.intro_label)
        return grid

    def show_dialog(self):
        ''' Add the dialog widget (default options) '''
        parent = self.view.builder.get_object('select_dev_list_port')
        childs = parent.get_children()
        if childs:
            parent.remove(childs[0])
        widget = self.get_dialog_widget()
        parent.add(widget)
        self.view.builder.get_object('select_dev_window').show_all()


class EventDeviceSelector(DeviceSelector):
    ''' Let user select an event device. '''

    intro_label = 'Select /dev/input device for the devinput driver.'
    device_label = 'Devinput driver on %device'


class LircDeviceSelector(DeviceSelector):
    ''' Let user select a /dev/lirc? device. '''

    intro_label = 'Select /dev/lirc device for the default driver.'
    device_label = 'Default driver on %device'

    def __init__(self, device_list, view):
        DeviceSelector.__init__(self, device_list, view, True)


class SerialDeviceSelector(DeviceSelector):
    ''' Let user select a device for a userspace driver. '''

    def __init__(self, device_list, view, on_ok_cb=None):
        self.on_ok_cb = on_ok_cb
        DeviceSelector.__init__(self, device_list, view, False)
        self.intro_label = 'Select %s device for the %s driver.' \
            % (device_list.device_pattern, device_list.driver_id)
        self.device_label = '%s driver on %s' \
            % (device_list.device_pattern, device_list.driver_id)


class GenericDeviceSelector(DeviceSelector):
    ''' Let user select a device for a generic userspace driver. '''

    def __init__(self, device_list, view):
        DeviceSelector.__init__(self, device_list, view, False)
        self.intro_label = 'Select %s device for the %s driver.' \
            % (device_list.device_pattern, device_list.driver_id)
        self.device_label = '%s driver on %s' \
            % (device_list.device_pattern, device_list.driver_id)


class UdpPortSelector(DeviceSelector):
    ''' Let user select a udp port  for a udp driver. '''

    def show_dialog(self):
        ''' No options, just display the message.... '''
        print("udp port dialog.")
        self.view.show_select_udp_port_window()


class AutoDeviceSelector(DeviceSelector):
    ''' Show a message for automatically selected devices. '''

    def show_dialog(self):
        ''' No options, just display the message.... '''
        self.view.show_info("Driver sets device automatically.")


def selector_factory(device_list, view):
    ''' Return a DeviceSelector handling config (a dict).'''
    pattern = device_list.device_pattern
    if 'tty' in pattern:
        return SerialDeviceSelector(device_list, view)
    elif 'lirc' in pattern:
        return LircDeviceSelector(device_list, view)
    elif 'event' in pattern:
        return EventDeviceSelector(device_list, view)
    elif pattern == 'udp_port':
        return UdpPortSelector(device_list, view)
    elif pattern == 'auto':
        return AutoDeviceSelector(device_list, view)
    else:
        return GenericDeviceSelector(device_list, view)


class Model(object):
    ''' The basic model is the selected remote, driver and device. '''

    NO_SUCH_REMOTE = None
    NO_SUCH_DRIVER = 0
    DEVICE_ATTR = ['modinit', 'lircmd_conf', 'driver', 'device',
                   'label', 'modprobe', 'conflicts', 'modules', 'supports']

    def __init__(self):
        self.config = {}
        self.configs = {}
        self.device_list = None
        self.remotes_index = None
        self._listeners = []

    def reset(self):
        ''' Reset to pristine state... '''
        Model.__init__(self)

    def _call_listeners(self):
        ''' Call all listeners. '''
        for listener in self._listeners:
            listener()

    def add_listener(self, listener):
        ''' Add function to be called when model changes.'''
        self._listeners.append(listener)

    def set_remote(self, remote):
        ''' Update the remote configuration file. '''
        self.config['lircd_conf'] = remote
        self._call_listeners()

    def clear_remote(self):
        ''' Unset the lircd_conf status. '''
        self.config['lircd.conf'] = ''
        self._call_listeners()

    def set_capture_device(self, device):
        ''' Given a device dict update selected capture device. '''

        for key in self.DEVICE_ATTR:
            if key in device:
                self.config[key] = str(device[key])
        self._call_listeners()

    def clear_capture_device(self):
        ''' Indeed: unset the capture device. '''
        for key in self.DEVICE_ATTR:
            if key in self.config:
                del self.config[key]
        self._call_listeners()

    def set_device(self, device):
        ''' Update the device part in config.'''
        self.config['device'] = device
        self._call_listeners()

    def get_remotes_list(self, view):
        ''' Download and return the directory file as a list of lines. '''
        if not self.remotes_index:
            if not os.path.exists(REMOTES_LIST):
                download_file(view, REMOTES_LIST_URL, REMOTES_LIST)
            with open(REMOTES_LIST) as f:
                list_ = f.read()
            self.remotes_index = [l for l in list_.split('\n') if l]
        return self.remotes_index

    def is_installable(self):
        ''' Do we have a configuration which can be installed? '''
        for item in ['lircd_conf', 'driver']:
            if item not in self.config or not self.config[item]:
                break
        else:
            return 'any' not in self.config['lircd_conf']
        return False

    def get_bundled_driver(self, remote, view):
        ''' Return the bundled capture device file for remote,
        possibly None (no such remote) or 0 (no such driver)
        '''
        for l in self.get_remotes_list(view):
            words = l.split(';')
            key = words[0] + "/" + words[1]
            if key == remote:
                if words[7] == 'no_driver':
                    return None
                config = self.find_config('id', words[7])
                if not config:
                    return self.NO_SUCH_DRIVER
                return config
        return self.NO_SUCH_REMOTE

    def has_lircd_conf(self):
        ''' Return if there is a valid lircd_conf in config. '''
        if 'lircd_conf' not in self.config:
            return False
        cf = self.config['lircd_conf']
        if not cf:
            return False
        cf = cf.replace('(', '').replace(')', '').replace(' ', '').lower()
        if not cf or cf == 'any' or cf == 'none':
            return False
        return True

    def has_label(self):
        ''' Test if there is a valid driver label in config. '''
        if 'label' not in self.config:
            return False
        lbl = self.config['label']
        if not lbl or lbl.lower().endswith('(none)'):
            return False
        return True

    def load_configs(self):
        ''' Load config files into self.configs. '''
        if self.configs:
            return
        self.configs = {}
        if os.path.exists(_here('configs')):
            configs = _here('configs')
        elif os.path.exists(_here('../configs')):
            configs = _here('../configs')
        else:
            return None
        for path in glob.glob(configs + '/*.conf'):
            with open(path) as f:
                cf = yaml.load(f.read())
            self.configs[cf['config']['id']] = cf['config']

    def get_configs(self):
        ''' Return all configurations. '''
        self.load_configs()
        return self.configs

    def find_config(self, key, value):
        ''' Return item (a dict) in configs where config[key] == value. '''
        self.load_configs()
        found = \
            [c for c in self.configs.values() if key in c and c[key] == value]
        if len(found) > 1:
            print("find_config: not properly found %s, %s): " % (key, value)
                  + ', '.join([c['id'] for c in found]))
            return None
        elif not found:
            print("find_config: Nothing  found for %s, %s): " % (key, value))
            return None
        return dict(found[0])


class Controller(object):
    '''  Kernel module options and sanity checks. '''
    CHECK_START = 1                  # << Initial setup
    CHECK_DEVICE = 2                 # << Configure device wildcard
    CHECK_REQUIRED = 3               # << Info in required modules
    CHECK_INIT = 4                   # << Define module parameters
    CHECK_LIRCD_CONF = 5             # << Setup the lircd_conf
    CHECK_DONE = 6
    CHECK_DIALOG = 7                 # << In dialog

    def __init__(self, model, view):
        self.model = model
        self.view = view
        self.state = self.CHECK_DEVICE
        self.cli_options = {}

    def check_required(self):
        ''' Check that required modules are present. '''
        self.state = self.CHECK_INIT
        text = _check_modules(self.model.config)
        if text:
            self.state = self.CHECK_DIALOG
            show_text(self.view.builder,
                      text,
                      "lirc: module check",
                      lambda: self.check(self.CHECK_INIT))
        else:
            self.check()

    def modinit_done(self):
        ''' Signal that kernel module GUI configuration is completed. '''
        self.check(self.CHECK_LIRCD_CONF)

    def check_modinit(self):
        ''' Let user define kernel module parameters. '''
        modinit = self.model.config['modinit'].split()
        self.state = self.CHECK_LIRCD_CONF
        if modinit[0].endswith('select_module_tty'):
            self.state = self.CHECK_DIALOG
            self.view.show_select_com_window(modinit[2])
        elif modinit[0].endswith('select_lpt_port'):
            self.state = self.CHECK_DIALOG
            self.view.show_select_lpt_window(modinit[2])
        else:
            self.check()

    def configure_device(self, config):
        ''' Configure the device for a given config entry. '''
        device_list = device_list_factory(config, self.model)
        if device_list.is_empty():
            self.view.show_warning(
                "No device found",
                'The %s driver can not be used since a suitable'
                ' device matching  %s cannot be found.' %
                (config['id'], config['device']))
            self.check(self.CHECK_REQUIRED)
        elif device_list.is_direct_installable():
            device = list(device_list.label_by_device.keys())[0]
            config['label'] += ' on ' + device
            config['device'] = device
            self.model.set_capture_device(config)
            self.check(self.CHECK_REQUIRED)
        else:
            gui = selector_factory(device_list, self.view)
            gui.on_ok_cb = lambda: self.check(self.CHECK_REQUIRED)
            gui.show_dialog()

    def check_device(self):
        ''' Check device, possibly let user select the one to use. '''
        config = self.model.config
        if 'device' not in config or config['device'] in [None, 'None']:
            self.state = self.CHECK_REQUIRED
            config['device'] = 'None'
            self.model.set_capture_device(config)
            self.check()
        device_dict = {'id': config['driver'],
                       'device': config['device'],
                       'label': config['driver']}
        self.state = self.CHECK_DIALOG
        self.configure_device(device_dict)

    def check_lircd_conf(self):
        ''' Possibly  install a lircd.conf for this driver...'''
        self.state = self.CHECK_DONE
        remotes = get_bundled_remotes(self.model.config, self.view)
        if len(remotes) == 0:
            return
        elif len(remotes) == 1:
            self.set_remote(remotes[0])
        else:
            selector = RemoteSelector(self)
            selector.select(remotes)

    def lircd_conf_done(self, remote):
        ''' Set the remote and run next FSM state. '''
        self.model.set_remote(remote)
        self.check(self.CHECK_DONE)

    def show_devinput(self):
        ''' Configure the devinput driver i. e., the event device. '''
        config = self.model.find_config('id', 'devinput')
        self.model.clear_capture_device()
        self.configure_device(config)

    def show_default(self):
        ''' Configure the devinput driver i. e., the event device. '''
        self.model.clear_capture_device()
        config = self.model.find_config('id', 'default')
        self.configure_device(config)

    def check(self, new_state=None):
        ''' Main FSM entry running actual check(s). '''
        if _DEBUG:
            print("Controller check, state: %d" % self.state)
        if new_state:
            self.state = new_state
        if self.state in [self.CHECK_DONE, self.CHECK_DIALOG]:
            return
        elif self.state == self.CHECK_DEVICE:
            self.check_device()
        elif self.state == self.CHECK_REQUIRED:
            if 'modules' in self.model.config and self.model.config['modules']:
                self.check_required()
            else:
                self.state = self.CHECK_INIT
                self.check()
        elif self.state == self.CHECK_INIT:
            if 'modinit' in self.model.config and self.model.config['modinit']:
                self.check_modinit()
            else:
                self.state = self.CHECK_LIRCD_CONF
                self.check()
        elif self.state == self.CHECK_LIRCD_CONF:
            self.check_lircd_conf()

    def set_remote(self, remote):
        ''' Update the remote. '''
        self.model.set_remote(remote)
        config = self.model.get_bundled_driver(remote, self.view)
        if not config:
            return
        device_list = device_list_factory(config, self.model)
        if device_list.is_empty():
            self.view.show_warning(
                "No device found",
                'The %s driver can not be used since a suitable'
                ' device matching %s cannot be found.' %
                (self.config['id'], self.config['device']))
        elif device_list.is_direct_installable():
            self.model.set_device(list(device_list.label_by_device.keys())[0])
        else:
            self.view.show_device_dialog(device_list)

    def start(self):
        ''' Start the thing... '''
        self.cli_options = _parse_options()
        errmsg = _check_resultsdir(self.cli_options['results_dir'])
        if errmsg:
            self.view.bad_startdir(errmsg)

    def write_results(self):
        ''' Write configuration files into resultdir. '''
        _write_results(self.model.config,
                       self.cli_options['results_dir'],
                       self.view)


class View(object):
    ''' The View part of the MVC pattern. '''

    def __init__(self, builder, model):
        self.builder = builder
        self.model = model
        self.controller = None
        self.selected = None
        self.cli_options = _parse_options()
        self._main_window_connect()
        model.add_listener(self._on_model_change)

    def _message_dialog(self, header, kind, body, exit_):
        ''' Return a standard error/warning/info dialog. '''
        # pylint: disable=not-callable
        d = Gtk.MessageDialog(self.builder.get_object('main_window'),
                              0,
                              kind,
                              Gtk.ButtonsType.OK,
                              header)
        if body:
            d.format_secondary_markup(body)
        d.run()
        d.destroy()
        if exit_:
            sys.exit()

    def show_warning(self, header, body=None, exit_=False):
        ''' Display standard warning dialog. '''
        self._message_dialog(header, Gtk.MessageType.WARNING, body, exit_)

    def show_error(self, header, body=None, exit_=False):
        ''' Display standard error dialog. '''
        self._message_dialog(header, Gtk.MessageType.ERROR, body, exit_)

    def show_info(self, header, body=None, exit_=False):
        ''' Display standard error dialog. '''
        self._message_dialog(header, Gtk.MessageType.INFO, body, exit_)

    def show_device_dialog(self, device_list):
        ''' Let user select which device to use. '''

        if isinstance(device_list, EventDeviceListModel):
            EventDeviceSelector(device_list, self).show_dialog()
        elif isinstance(device_list, LircDeviceListModel):
            LircDeviceSelector(device_list, self).show_dialog()
        elif isinstance(device_list, GenericDeviceListModel):
            GenericDeviceSelector(device_list, self).show_dialog()
        elif isinstance(device_list, SerialDeviceListModel):
            SerialDeviceSelector(device_list, self).show_dialog()
        elif isinstance(device_list, UdpPortDeviceList):
            UdpPortSelector(device_list, self).show_dialog()
        elif isinstance(device_list, AutoDeviceList):
            AutoDeviceSelector(device_list, self).show_dialog()
        else:
            self.show_warning("Unknown list model (!)")

    def set_controller(self, controller):
        ''' Set the controller, a circular dependency. '''
        self.controller = controller

    def _main_window_connect(self):
        ''' Connect signals for main window. '''

        def on_view_config_btn_cb(btn):
            ''' User pressed the view config file button. '''
            label = self.builder.get_object('selected_remote_lbl')
            self.show_remote(label.get_text())
            return True

        def on_no_remote_btn_cb(button):
            ''' User clicked 'Use no remote config' button. '''
            self.controller.set_remote('(None)')
            return True

        def on_info_btn_clicked(txt):
            ''' Handle ? help button  button . '''
            lbl = self.builder.get_object('selected_remote_lbl').get_text()
            print("Selected remote label: " + lbl)
            show_text(self.builder, txt, "lirc: help")

        clicked_connect = [
            ('view_config_btn', on_view_config_btn_cb),
            ('search_btn', lambda b: self.show_search_results_select()),
            ('main_browse_btn', lambda b: self.show_config_browse_cb()),
            ('no_remote_btn', on_no_remote_btn_cb),
            ('devinput_btn', lambda b: self.controller.show_devinput()),
            ('default_btn', lambda b: self.controller.show_default()),
            ('view_driver_btn', lambda b: self.show_driver()),
            ('preconfig_device_btn', lambda b: self.show_preconfig_dialog()),
            ('install_btn', lambda b: self.controller.write_results()),
            ('restart_btn', lambda b: self.do_restart()),
            ('exit_btn', lambda b: Gtk.main_quit()),
            ('no_remote_help_btn',
                lambda b: on_info_btn_clicked(NO_REMOTE_INFO)),
            ('devinput_help_btn',
                lambda b: on_info_btn_clicked(DEVINPUT_INFO)),
            ('default_help_btn',
                lambda b: on_info_btn_clicked(DEFAULT_INFO)),
            ('main_help_btn', lambda b: on_info_btn_clicked(MAIN_HELP)),
            ('preconfig_help_btn',
                lambda b: on_info_btn_clicked(PRECONFIG_INFO))
        ]
        w = self.builder.get_object('main_window')
        w.connect('delete-event', lambda w, e: Gtk.main_quit())
        for btn_name, handler in clicked_connect:
            self.builder.get_object(btn_name).connect('clicked', handler)

    def _on_model_change(self):
        ''' Update view to match model.'''
        text = "(None)"
        sense_conf = False
        if self.model.has_lircd_conf():
            text = self.model.config['lircd_conf']
            sense_conf = True
        self.builder.get_object('selected_remote_lbl').set_text(text)
        self.builder.get_object('drv_select_remote_lbl').set_text(text)
        self.builder.get_object('view_config_btn').set_sensitive(sense_conf)

        text = 'Selected capture device (None)'
        sensitive = True if self.model.has_label() else False
        if sensitive:
            text = self.model.config['label']
        self.builder.get_object('view_driver_btn').set_sensitive(sensitive)
        self.builder.get_object('selected_driver_lbl').set_text(text)
        self.builder.get_object('selected_driver_lbl').queue_draw()

        btn = self.builder.get_object('install_btn')
        btn.set_sensitive(self.model.is_installable())

    def load_configs(self):
        ''' Load config files into model. -> control... '''
        self.model.load_configs()
        if not self.model.configs:
            self.view.show_warning("Cannot find the configuration files")

    def show_driver(self):
        ''' Display data for current driver. '''

        items = [
            ('Driver', 'driver'),
            ('lircd.conf file', 'lircd_conf'),
            ('lircmd.conf file', 'lircmd_conf'),
            ('lircd --device option', 'device'),
            ('Kernel modules required and loaded', 'modprobe'),
            ('Kernel module setup', 'modsetup'),
            ('Blacklisted kernel modules', 'conflicts')
        ]
        s = ''
        for label, key in items:
            s += "%-36s: " % label
            s += str(self.model.config[key]) \
                if key in self.model.config else 'Not set'
            s += "\n"
        show_text(self.builder, s, 'lirc: Show driver.')

    def show_remote(self, remote):
        ''' Display remote config file in text window. '''
        # pylint: disable=no-member
        uri = _REMOTES_BASE_URI + '/' + remote + '?format=raw'
        try:
            text = urllib.request.urlopen(uri).read().decode('utf-8',
                                                             errors='ignore')
        except urllib.error.URLError as ex:
            text = "Sorry: cannot download: " + uri + ' (' + str(ex) + ')'
        show_text(self.builder, text, 'lirc: download error')

    def show_select_lpt_window(self, module):
        ''' Show window for selecting lpt1, lpt2... '''

        ports = {'lpt1': ['7', '0x378'],
                 'lpt2': ['7', '0x278'],
                 'lpt3': ['5', '0x3bc'],
                 'custom': ['0', '0']}

        def on_select_back_cb(button, data=None):
            ''' User clicked the Back button. '''
            button.get_toplevel().hide()

        def on_select_next_cb(button, data=None):
            ''' User clicked the Next button. '''
            group = self.builder.get_object('lpt1_lpt_btn').get_group()
            for b in group:
                if not b.get_active():
                    continue
                btn_id = b.lirc_id
                if btn_id == 'custom':
                    entry = self.builder.get_object('lpt_iobase_entry')
                    iobase = entry.get_text()
                    entry = self.builder.get_object('lpt_irq_entry')
                    irq = entry.get_text()
                else:
                    iobase = ports[btn_id][0]
                    irq = ports[btn_id][1]
                self.model.config['modsetup'] = \
                    '%s: iobase=%s irq=%s' % (module, iobase, irq)
            self.controller.check(Controller.CHECK_LIRCD_CONF)
            button.get_toplevel().hide()

        for btn in ports.keys():
            b = self.builder.get_object(btn + '_lpt_btn')
            b.lirc_id = btn
        b = self.builder.get_object('lpt_next_btn')
        b.connect('clicked', on_select_next_cb)
        b = self.builder.get_object('lpt_back_btn')
        b.connect('clicked', on_select_back_cb)
        w = self.builder.get_object('select_lpt_window')
        w.show_all()

    def show_select_udp_port_window(self):
        ''' Let user select port for the UDP driver. '''

        def on_udp_port_next_cb(button, data=None):
            ''' User clicked the Next button. '''
            entry = self.builder.get_object('udp_port_entry')
            numtext = entry.get_text()
            try:
                num = int(numtext)
            except ValueError:
                num = -1
            num = -1 if num > 65535 else num
            num = -1 if num <= 1024 else num
            if num == -1:
                self.show_warning("Illegal port number")
            else:
                self.model.set_device(numtext)
                self.controller.check(Controller.CHECK_LIRCD_CONF)
                device_dict = dict(self.model.find_config('id', 'udp'))
                device_dict['label'] += ' ' + numtext
                device_dict['device'] = numtext
                self.model.set_capture_device(device_dict)
            button.get_toplevel().hide()

        def on_udp_port_back_cb(button, data=None):
            ''' User clicked the Back button. '''
            button.get_toplevel().hide()

        b = self.builder.get_object('udp_port_next_btn')
        b.connect('clicked', on_udp_port_next_cb)
        b = self.builder.get_object('udp_port_back_btn')
        b.connect('clicked', on_udp_port_back_cb)
        w = self.builder.get_object('select_udp_port_window')
        w.show_all()

    def show_select_com_window(self, module):
        ''' Show window for selecting com1, com2... '''

        ports = {'com1': ['4', '0x3f8'],
                 'com2': ['3', '0x2f8'],
                 'com3': ['4', '0x3e8'],
                 'com4': ['3', '0x2e8'],
                 'custom': ['0', '0']}

        def on_com_select_next_cb(button, data=None):
            ''' User clicked the Next button. '''
            group = self.builder.get_object('com1_btn').get_group()
            for b in group:
                if not b.get_active():
                    continue
                btn_id = b.lirc_id
                if btn_id == 'custom':
                    entry = self.builder.get_object('custom_iobase_entry')
                    iobase = entry.get_text()
                    entry = self.builder.get_object('custom_irq_entry')
                    irq = entry.get_text()
                else:
                    iobase = ports[btn_id][1]
                    irq = ports[btn_id][0]
                self.model.config['modsetup'] = \
                    '%s: iobase=%s irq=%s' % (module, iobase, irq)
            self.controller.check(Controller.CHECK_LIRCD_CONF)
            button.get_toplevel().hide()

        def on_com_select_back_cb(button, data=None):
            ''' User clicked the Back button. '''
            button.get_toplevel().hide()

        for btn in ports.keys():
            b = self.builder.get_object(btn + '_btn')
            b.lirc_id = btn
        b = self.builder.get_object('select_com_next_btn')
        b.connect('clicked', on_com_select_next_cb)
        b = self.builder.get_object('select_com_back_btn')
        b.connect('clicked', on_com_select_back_cb)
        w = self.builder.get_object('select_com_window')
        w.show_all()

    def show_preconfig_dialog(self):
        ''' Show the preconfigured devices main dialog. '''

        menu_label_by_id = {
            'usb': ('preconfig_menu_1', 'USB Devices'),
            'other_serial': ('preconfig_menu_2', 'Other serial devices'),
            'tv_card': ('preconfig_menu_3', 'TV card devices'),
            'pda': ('preconfig_menu_4', 'PDA:s'),
            'other': ('preconfig_menu_5',
                      'Other (MIDI, Bluetooth, udp, etc.)'),
            'soundcard': ('preconfig_menu_6',
                          'Home-brew (soundcard input)'),
            'home_brew': ('preconfig_menu_7',
                          'Home-brew serial and parallel devices'),
            'irda': ('preconfig_menu_8', 'IRDA/CIR hardware')
        }

        def get_selected_cb(button, data=None):
            ''' Return currently selected menu option. '''
            for menu_label in menu_label_by_id.values():
                b = self.builder.get_object(menu_label[0])
                if b.get_active():
                    self.selected = b.lirc_id
                    return True
            return False

        w = self.builder.get_object('preconfig_window')
        w.connect('delete-event', on_window_delete_event_cb)
        b = self.builder.get_object('preconfig_back_btn')
        b.connect('clicked', lambda b: w.hide())
        b = self.builder.get_object('preconfig_next_btn')
        b.connect('clicked',
                  lambda b: self.show_preconfig_select(self.selected))
        for id_, menu_label in menu_label_by_id.items():
            b = self.builder.get_object(menu_label[0])
            b.connect('toggled', get_selected_cb)
            b.lirc_id = id_
        get_selected_cb(None)
        w.show_all()

    def show_preconfig_select(self, menu=None):
        ''' User has selected configuration submenu, present options. '''

        def build_treeview(menu):
            ''' Construct the configurations points liststore treeview. '''

            treeview = self.builder.get_object('preconfig_items_view')
            treeview.set_vscroll_policy(Gtk.ScrollablePolicy.NATURAL)
            if len(treeview.get_columns()) == 0:
                liststore = Gtk.ListStore(str)
                treeview.set_model(liststore)
                renderer = Gtk.CellRendererText()
                column = Gtk.TreeViewColumn('Configuration', renderer, text=0)
                column.clickable = True
                treeview.append_column(column)
                treeview.get_selection().connect('changed',
                                                 on_treeview_change_cb)
            liststore = treeview.get_model()
            liststore.clear()
            configs = self.model.get_configs()
            labels = [configs[l]['label']
                        for l in configs.keys() if configs[l]['menu'] == menu]
            for l in sorted(labels):
                liststore.append([l])

        def on_treeview_change_cb(selection, data=None):
            ''' User selected a row i. e., a config. '''
            (model, iter_) = selection.get_selected()
            if not iter_:
                return
            label = self.builder.get_object('preconfig_select_lbl')
            label.set_text(model[iter_][0])

        def on_preconfig_next_clicked_cb(button, data=None):
            ''' User pressed 'Next' button. '''
            label = self.builder.get_object('preconfig_select_lbl')
            device = self.model.find_config('label', label.get_text())
            self.model.set_capture_device(device)
            button.get_toplevel().hide()
            self.builder.get_object('preconfig_window').hide()
            self.controller.check()
            return True

        build_treeview(menu)

        w = self.builder.get_object('preconfig_select_window')
        w.connect('delete-event', on_window_delete_event_cb)
        b = self.builder.get_object('preconfig_select_back_btn')
        b.connect('clicked', lambda b: w.hide())
        b = self.builder.get_object('preconfig_select_next_btn')
        b.connect('clicked', on_preconfig_next_clicked_cb)
        w.show_all()

    def show_search_results_select(self):
        ''' User  has entered a search pattern, let her choose match.'''

        def build_treeview():
            ''' Construct the search results liststore treeview. '''

            treeview = self.builder.get_object('search_results_view')
            treeview.set_vscroll_policy(Gtk.ScrollablePolicy.NATURAL)
            if len(treeview.get_columns()) > 0:
                return treeview
            liststore = Gtk.ListStore(str, str, str)
            treeview.set_model(liststore)
            renderers = {}
            for i, colname in enumerate(['vendor', 'lircd.conf', 'device']):
                renderers[colname] = Gtk.CellRendererText()
                column = \
                    Gtk.TreeViewColumn(colname, renderers[colname], text=i)
                column.clickable = True
                treeview.append_column(column)
            treeview.get_selection().connect('changed', on_select_change_cb)
            return treeview

        def on_select_change_cb(selection, data=None):
            ''' User changed the selected lircd.conf option. '''
            (model, iter_) = selection.get_selected()
            if not iter_:
                return
            remote = model[iter_][0] + '/' + model[iter_][1]
            self.builder.get_object('selected_config_lbl').set_text(remote)
            self.builder.get_object('search_next_btn').set_sensitive(True)

        def on_search_next_btn_cb(button, data=None):
            ''' User pressed 'Next' button.'''
            label = self.builder.get_object('selected_config_lbl')
            self.controller.set_remote(label.get_text())
            button.get_toplevel().hide()
            return True

        def get_remotes(entry):
            ''' Return list of remotes matching pattern in entry. '''
            lines = self.model.get_remotes_list(self)
            entry = self.builder.get_object('search_config_entry')
            pattern = entry.get_text()
            return [l for l in lines if pattern in l]

        def load_liststore(liststore, found):
            ''' Reload the liststore data model. '''
            liststore.clear()
            for l in found:
                words = l.split(';')
                liststore.append([words[0], words[1], words[4]])

        found = get_remotes(self.builder.get_object('search_config_entry'))
        if not found:
            self.show_warning("No matching config found")
            return
        treeview = build_treeview()
        load_liststore(treeview.get_model(), found)
        b = self.builder.get_object('search_back_btn')
        b.connect('clicked', lambda b, d=None: b.get_toplevel().hide())
        b = self.builder.get_object('search_next_btn')
        b.connect('clicked', on_search_next_btn_cb)
        w = self.builder.get_object('search_select_window')
        w.show_all()

    def show_config_browse_cb(self, button=None, data=None):
        ''' User clicked browse configs button. '''

        def build_treeview():
            ''' Construct the remotes browse liststore treeview. '''

            treeview = self.builder.get_object('config_browse_view')
            if len(treeview.get_columns()) > 0:
                return treeview
            treeview.set_vscroll_policy(Gtk.ScrollablePolicy.NATURAL)
            treestore = Gtk.TreeStore(str)
            treeview.set_model(treestore)
            renderer = Gtk.CellRendererText()
            column = Gtk.TreeViewColumn('path', renderer, text=0)
            column.clickable = True
            treeview.append_column(column)
            return treeview

        def fill_treeview(treeview):
            ''' Fill the treestore with browse options. '''
            treestore = treeview.get_model()
            if hasattr(treestore, 'lirc_is_inited'):
                return
            treestore.clear()
            lines = self.model.get_remotes_list(self)
            lines_by_letter = get_lines_by_letter(lines)
            for c in sorted(lines_by_letter.keys()):
                iter_ = treestore.insert_with_values(None, -1, [0], [c])
                done = []
                for l in lines_by_letter[c]:
                    w = [l.split(';')[0]]
                    if w[0] in done:
                        continue
                    done.extend(w)
                    subiter = treestore.insert_with_values(iter_, -1, [0], w)
                    my_lines = [l for l in lines_by_letter[c]
                                    if l.startswith(w[0] + ';')]
                    for ml in my_lines:
                        words = ml.split(';')
                        label = [words[1] + "/" + words[4]]
                        treestore.insert_with_values(subiter, -1, [0], label)

            treestore.lirc_is_inited = True

        def on_select_change_cb(selection, data=None):
            ''' User changed the selected lircd.conf browse  option. '''
            (model, treeiter) = selection.get_selected()
            if not treeiter or not model[treeiter].get_parent():
                return True
            item = model[treeiter][0].split('/')[0]
            item = model[treeiter].get_parent()[0] + "/" + item
            self.controller.set_remote(item)
            lbl = self.builder.get_object('config_browse_select_lbl')
            lbl.set_text(item)
            btn = self.builder.get_object('config_browse_next_btn')
            btn.set_sensitive(True)
            return True

        def on_config_browse_next_btn_cb(button, data=None):
            ''' User presses 'Next' button. '''
            label = self.builder.get_object('config_browse_select_lbl')
            self.controller.set_remote(label.get_text())
            button.get_toplevel().hide()
            return True

        def on_config_browse_view_btn_cb(button, data=None):
            ''' User presses 'View' button. '''
            lbl = self.builder.get_object('config_browse_select_lbl')
            self.show_remote(lbl.get_text())

        w = self.builder.get_object('config_browse_window')
        w.connect('delete-event', on_window_delete_event_cb)
        b = self.builder.get_object('config_browse_back_btn')
        b.connect('clicked', lambda b, d=None: b.get_toplevel().hide())
        b = self.builder.get_object('config_browse_next_btn')
        b.connect('clicked', on_config_browse_next_btn_cb)
        b = self.builder.get_object('config_browse_view_btn')
        b.connect('clicked', on_config_browse_view_btn_cb)
        treeview = build_treeview()
        fill_treeview(treeview)
        treeview.get_selection().connect('changed', on_select_change_cb)
        w.show_all()

    def do_restart(self):
        ''' Reset state to pristine. '''
        # pylint: disable=not-callable, unused-variable
        self.model.reset()
        self.builder.get_object('main_window').hide()
        Gtk.main_quit()
        builder = Gtk.Builder()
        builder.add_from_file(_here("lirc-setup.ui"))
        View.__init__(self, builder, self.model)
        self.controller = Controller(self.model, self)
        self._on_model_change()
        self.builder.get_object('install_btn').set_sensitive(False)
        self.builder.get_object('main_window').show_all()
        Gtk.main()

    @staticmethod
    def on_delete_event_cb(window, event=None):
        ''' Main window close event. '''
        Gtk.main_quit()

    @staticmethod
    def on_quit_button_clicked_cb(widget, data=None):
        ''' User clicked 'Quit' button. '''
        Gtk.main_quit()

    def bad_startdir(self, errmsg):
        ''' Handle bad configuration directory...'''
        self.show_warning("Invalid results directory", errmsg, True)
        Gtk.main_quit()


def main():
    ''' Indeed: main program. '''
    # pylint: disable=not-callable
    model = Model()
    builder = Gtk.Builder()
    builder.add_from_file(_here("lirc-setup.ui"))
    view = View(builder, model)
    builder.connect_signals(view)
    controller = Controller(model, view)
    view.set_controller(controller)
    controller.start()
    builder.get_object('main_window').show_all()

    Gtk.main()


if __name__ == '__main__':
    main()


# vim: set expandtab ts=4 sw=4:
