''' Simple lirc setup tool - control part. '''
import gi
gi.require_version('Gtk', '3.0')

from gi.repository import Gtk         # pylint: disable=no-name-in-module
from gi.repository import GObject     # pylint: disable=no-name-in-module

import grp
import os
import pwd
import sys
import urllib.error          # pylint: disable=no-name-in-module,F0401,E0611
import urllib.request        # pylint: disable=no-name-in-module,F0401,E0611

import mvc_model
import mvc_view
import choosers
import util

_DEBUG = 'LIRC_DEBUG' in os.environ
_REMOTES_BASE_URI = "http://sf.net/p/lirc-remotes/code/ci/master/tree/remotes"
_USAGE = "Usage: lirc-setup [results directory]"


MSG_NOT_IN_LIRC_GROUP = """
You are not member of the lirc group, testing with mode2 will not work. To
fix run <i>sudo usermod -aG lirc {user}; sg lirc lirc-setup</i> to add the
new group and restart lirc-setup.
"""


def _check_groups():
    """ Return error message if running user isn't member of group lirc. """
    user = pwd.getpwuid(os.geteuid()).pw_name
    try:
        if user in grp.getgrnam("lirc")[3]:
            return None
    except KeyError:
        pass
    return MSG_NOT_IN_LIRC_GROUP.format(user=user)


def _hasitem(dict_, key):
    ''' Test if dict contains a non-null value for key. '''
    return key in dict_ and dict_[key] and dict_[key] != 'None'


def _here(path):
    ' Return path added to current dir for __file__. '
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), path)


class Controller(object):
    '''  Kernel module options and sanity checks. '''

    CHECK_START = 1                  # << Initial setup
    CHECK_DEVICE = 2                 # << Configure device wildcard
    CHECK_INIT = 3                   # << Info on kernel setup
    CHECK_MODPROBE = 4               # << Define module parameters
    CHECK_LIRCD_CONF = 5             # << Setup the lircd_conf
    CHECK_NOTE = 6                   # << Display the note: message
    CHECK_DONE = 7
    CHECK_DIALOG = 8                 # << Running a dialog

    def __init__(self, model, view):
        self.model = model
        self.view = view
        self.state = self.CHECK_DEVICE
        self.cli_options = {}

    def check_modprobe(self):
        ''' Let user define kernel module parameters. '''
        # pylint: disable=bad-indentation
        driver = self.model.driver['id']
        if driver == 'lirc_serial':
            self.state = self.CHECK_DIALOG
            self.view.show_select_com_window()
        elif driver == 'lirc_parallel':
            self.state = self.CHECK_DIALOG
            self.view.show_select_lpt_window()
        elif driver == 'default':
            try:
                rc_dir = util.get_rcdir_by_device(self.model.config.device)
            except LookupError:
                pass
            else:
                self.model.config.modinit = \
                    'echo lirc > %s/protocols ' % rc_dir
        else:
            if 'modprobe' in self.model.driver \
                and self.model.driver['modprobe']:
                    self.model.config.modprobe = self.model.driver.modprobe
            self.check(self.CHECK_LIRCD_CONF)

    def modprobe_done(self, modprobe):
        ''' Signal that kernel module GUI configuration is completed. '''
        if modprobe:
            self.model.config.modprobe = modprobe
        self.check(self.CHECK_LIRCD_CONF)

    def check_modinit(self):
        ''' Check kernel setup code.'''
        # driver = self.model.config.driver['id']
        # if 'modinit' in self.model.db.drivers[driver]:
        #     modinit = self.model.db.drivers[driver]['modinit']
        #     if modinit and modinit not in [None, 'None']:
        #         self.view.show_info("Kernel setup required",
        #                             "Required setup: " + modinit)
        #         self.model.set_modinit(modinit)
        self.check(self.CHECK_MODPROBE)

    def configure_device(self, driver, next_state=None):
        ''' Configure the device for a given driver entry. '''

        def on_select_cb(driver_id, device):
            ''' Invoked when user selected a device.'''
            self.select_device_done(device, next_state)

        if isinstance(driver, str):
            self.model.driver = self.model.db.drivers[driver]
        else:
            self.model.driver = driver
        device_list = mvc_model.device_list_factory(driver, self.model)
        if device_list.is_empty():
            self.view.show_warning(
                "No device found",
                'The %s driver can not be used since a suitable'
                ' device matching  %s cannot be found.'
                ' Use "Modify device" to enable driver ' %
                (self.model.driver['id'], self.model.driver['device_hint']))
            self.check(next_state)
        elif device_list.is_direct_installable():
            device = list(device_list.label_by_device.keys())[0]
            label = device_list.label_by_device[device]
            msg = "Using the only available device %s (%s)" % (device, label)
            self.view.show_info(msg)
            self.select_device_done(device, next_state)
        else:
            gui = choosers.factory(device_list, self.view, on_select_cb)
            gui.show_dialog()

    def check_device(self):
        ''' Check device, possibly let user select the one to use. '''
        if not self.model.config.device or self.model.config.device == 'None':
            self.model.clear_capture_device()
            self.check(self.CHECK_INIT)
        else:
            self.state = self.CHECK_DIALOG
            self.configure_device(self.model.config.driver, self.CHECK_INIT)

    def select_device_done(self, device, next_state):
        ''' Callback from GUI after user selected a specific device. '''
        self.model.set_device(device)
        self.check(next_state)

    def check_lircd_conf(self):
        ''' Possibly  install a lircd.conf for this driver...'''
        remotes = mvc_model.get_bundled_remotes(self.model)
        if len(remotes) == 0:
            self.check(self.CHECK_NOTE)
            return
        elif len(remotes) == 1:
            self.set_remote(remotes[0], self.CHECK_NOTE)
        else:
            self.state = self.CHECK_DIALOG
            selector = choosers.RemoteSelector(self, self.lircd_conf_done)
            selector.select(remotes)

    def lircd_conf_done(self, remote):
        ''' Set the remote and run next FSM state. '''
        self.model.set_remote(remote)
        self.check(self.CHECK_NOTE)

    def check_note(self):
        ''' Display the note: message in configuration file. '''
        self.state = self.CHECK_DONE
        if self.model.config.note:
            self.view.show_info(self.model.config.note)

    def check(self, new_state=None):
        ''' Main FSM entry running actual check(s). '''
        fsm = {
            self.CHECK_DONE: lambda: None,
            self.CHECK_DIALOG: lambda: None,
            self.CHECK_NOTE: self.check_note,
            self.CHECK_INIT: self.check_modinit,
            self.CHECK_DEVICE: self.check_device,
            self.CHECK_MODPROBE: self.check_modprobe,
            self.CHECK_LIRCD_CONF: self.check_lircd_conf
        }
        if new_state:
            self.state = new_state
        if _DEBUG:
            print("dialog: FSM, state: %d" % self.state)
        fsm[self.state]()

    def show_devinput(self):
        ''' Configure the devinput driver i. e., the event device. '''
        self.model.clear_capture_device()
        config = self.model.db.configs['devinput']
        self.model.set_config(config)
        self.configure_device(config['driver'], self.CHECK_INIT)

    def show_default(self):
        ''' Configure the default driver i. e., the default device. '''
        self.model.clear_capture_device()
        config = self.model.db.configs['default']
        self.model.set_config(config)
        self.configure_device(config['driver'], self.CHECK_INIT)

    def start_check(self):
        ''' Run the first step of the configure dialogs. '''
        self.check(self.CHECK_DEVICE)

    def set_remote(self, remote, next_state=None):
        ''' Update the remote. '''
        driver = self.model.get_bundled_driver(remote)
        if driver:
            self.model.set_driver(driver)
        self.model.set_remote(remote)
        if not driver:
            return
        if driver:
            self.configure_device(driver, next_state)

    def select_remote(self, pattern):
        ''' User has entered a search pattern, handle it. '''
        lines = self.model.get_remotes_list(self)
        lines = [l for l in lines if pattern in l]
        if not lines:
            self.view.show_warning("No matching config found")
        elif len(lines) == 1:
            self.view.show_single_remote(lines[0])
        else:
            self.view.show_search_results_select(lines)

    def start(self):
        ''' Start the thing... '''
        self.cli_options = mvc_model.parse_options()
        if not self.cli_options:
            sys.stderr.write(_USAGE)
            sys.exit(1)
        errmsg = mvc_model.check_resultsdir(self.cli_options['results_dir'])
        if errmsg:
            self.view.results_dir_reset(errmsg)
        else:
            errmsg = _check_groups()
            if errmsg:
                self.view.show_warning("Missing lirc group", errmsg)
        self.view.builder.get_object('main_window').show_all()

    def restart(self):
        ''' Make a total reset, kills UI and makes a new. '''
        # pylint: disable=not-callable
        self.model.reset()
        self.view.builder.get_object('main_window').hide()
        Gtk.main_quit()
        builder = Gtk.Builder()
        builder.add_from_file(_here("lirc-setup.ui"))
        self.view = mvc_view.View(builder, self.model)
        cli_options = self.cli_options.copy()
        Controller.__init__(self, self.model, self.view)
        self.cli_options = cli_options
        self.view.set_controller(self)
        self.view.on_model_change()
        self.view.builder.get_object('main_window').show_all()
        Gtk.main()

    def write_results(self):
        ''' Write configuration files into resultdir. '''
        log = mvc_model.write_results(self.model,
                                      self.cli_options['results_dir'],
                                      self.view)
        self.view.show_resultdir('Installation files written',
                                 log,
                                 self.cli_options['results_dir'])

    def show_remote(self, remote):
        ''' Display remote config file in text window. '''
        # pylint: disable=no-member
        if os.path.exists(remote):
            with open(remote, "r") as f:
                text = ''.join(f.readlines())
            text = GObject.markup_escape_text(text)
        else:
            uri = _REMOTES_BASE_URI + '/' + remote + '?format=raw'
            try:
                text = \
                    urllib.request.urlopen(uri).read().decode('utf-8',
                                                              errors='ignore')
            except urllib.error.URLError as ex:
                text = "Sorry: cannot download: " + uri + ' (' + str(ex) + ')'
        text = text.replace('@', ' at ').replace('|', ' pipe ') \
            .replace('<', '[').replace('>', ']')
        self.view.show_text('<tt>' + text + '</tt>',
                            'LIRC: Remote config file')


def main():
    ''' Indeed: main program. '''
    # pylint: disable=not-callable
    model = mvc_model.Model()
    builder = Gtk.Builder()
    builder.add_from_file(_here("lirc-setup.ui"))
    view = mvc_view.View(builder, model)
    builder.connect_signals(view)
    controller = Controller(model, view)
    view.set_controller(controller)
    controller.start()

    Gtk.main()


if __name__ == '__main__':
    main()


# vim: set expandtab ts=4 sw=4:
