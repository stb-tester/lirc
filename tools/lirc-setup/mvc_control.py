''' Simple lirc setup tool - control part. '''

from gi.repository import Gtk         # pylint: disable=no-name-in-module

import os
import urllib.error          # pylint: disable=no-name-in-module,F0401,E0611
import urllib.request        # pylint: disable=no-name-in-module,F0401,E0611

import mvc_model
import mvc_view
import selectors

_DEBUG = 'LIRC_DEBUG' in os.environ
_REMOTES_BASE_URI = "http://sf.net/p/lirc-remotes/code/ci/master/tree/remotes"


def _hasitem(dict_, key_):
    ''' Test if dict contains a non-null value for key. '''
    return key_ in dict_ and dict_[key_]


def _here(path):
    ' Return path added to current dir for __file__. '
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), path)


class Controller(object):
    '''  Kernel module options and sanity checks. '''

    CHECK_START = 1                  # << Initial setup
    CHECK_DEVICE = 2                 # << Configure device wildcard
    CHECK_REQUIRED = 3               # << Info in required modules
    CHECK_INIT = 4                   # << Define module parameters
    CHECK_LIRCD_CONF = 5             # << Setup the lircd_conf
    CHECK_NOTE = 6                   # << Display the note: message
    CHECK_DONE = 7
    CHECK_DIALOG = 8                 # << Running a dialog

    def __init__(self, model, view):
        self.model = model
        self.view = view
        self.state = self.CHECK_DEVICE
        self.cli_options = {}

    def check_required(self):
        ''' Check that required modules are present. '''
        if not _hasitem(self.model.config, 'modules'):
            self.check(self.CHECK_INIT)
            return
        text = mvc_model.check_modules(self.model.config)
        if text:
            text = "<tt>" + text + "</tt>"
            self.view.show_info("lirc: module check", text)
        self.check(self.CHECK_INIT)

    def modinit_done(self, modsetup):
        ''' Signal that kernel module GUI configuration is completed. '''
        if modsetup:
            self.model.config['modsetup'] = modsetup
        self.check(self.CHECK_LIRCD_CONF)

    def check_modinit(self):
        ''' Let user define kernel module parameters. '''
        if not _hasitem(self.model.config, 'modinit'):
            self.check(self.CHECK_LIRCD_CONF)
            return
        modinit = self.model.config['modinit'].split()
        if modinit[0].endswith('select_module_tty'):
            self.state = self.CHECK_DIALOG
            self.view.show_select_com_window(modinit[2])
        elif modinit[0].endswith('select_lpt_port'):
            self.state = self.CHECK_DIALOG
            self.view.show_select_lpt_window(modinit[2])
        else:
            self.check(self.CHECK_LIRCD_CONF)

    def configure_device(self, config, next_state=None):
        ''' Configure the device for a given config entry. '''

        def on_select_cb(driver_id, device):
            ''' Invoked when user selected a device.'''
            self.select_device_done(driver_id, device, next_state)

        device_list = mvc_model.device_list_factory(config, self.model)
        if device_list.is_empty():
            self.view.show_warning(
                "No device found",
                'The %s driver can not be used since a suitable'
                ' device matching  %s cannot be found.' %
                (config['id'], config['device']))
            self.check(next_state)
        elif device_list.is_direct_installable():
            device = list(device_list.label_by_device.keys())[0]
            label = device_list.label_by_device[device]
            msg = "Using the only available device %s (%s)" % (device, label)
            self.view.show_info(msg)
            self.select_device_done(config['id'], device, next_state)
        else:
            gui = selectors.factory(device_list, self.view, on_select_cb)
            gui.show_dialog()

    def check_device(self):
        ''' Check device, possibly let user select the one to use. '''
        config = self.model.config
        if 'device' not in config or config['device'] in [None, 'None']:
            config['device'] = 'None'
            self.model.set_capture_device(config)
            self.check(self.CHECK_REQUIRED)
        else:
            self.state = self.CHECK_DIALOG
            device_dict = {'id': config['driver'],
                           'device': config['device'],
                           'label': config['label']}
            self.configure_device(device_dict, self.CHECK_REQUIRED)

    def select_device_done(self, driver_id, device, next_state):
        ''' Callback from GUI after user selected a specific device. '''
        config = self.view.model.find_config('id', driver_id)
        self.model.set_capture_device(config)
        self.model.set_device(device)
        self.check(next_state)

    def check_lircd_conf(self):
        ''' Possibly  install a lircd.conf for this driver...'''
        remotes = mvc_model.get_bundled_remotes(self.model.config, self.view)
        if len(remotes) == 0:
            self.check(self.CHECK_NOTE)
            return
        elif len(remotes) == 1:
            self.set_remote(remotes[0], self.CHECK_NOTE)
        else:
            self.state = self.CHECK_DIALOG
            selector = selectors.RemoteSelector(self, self.lircd_conf_done)
            selector.select(remotes)

    def lircd_conf_done(self, remote):
        ''' Set the remote and run next FSM state. '''
        self.model.set_remote(remote)
        self.check(self.CHECK_NOTE)

    def check_note(self):
        ''' Display the note: message in configuration file. '''
        self.state = self.CHECK_DONE
        if _hasitem(self.model.config, 'note'):
            self.view.show_info('Note', self.model.config['note'])

    def check(self, new_state=None):
        ''' Main FSM entry running actual check(s). '''
        fsm = {
            self.CHECK_DONE: lambda: None,
            self.CHECK_DIALOG: lambda: None,
            self.CHECK_NOTE: self.check_note,
            self.CHECK_INIT: self.check_modinit,
            self.CHECK_DEVICE: self.check_device,
            self.CHECK_REQUIRED: self.check_required,
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
        config = self.model.find_config('id', 'devinput')
        self.model.set_capture_device(config)
        self.configure_device(config, self.CHECK_REQUIRED)

    def show_default(self):
        ''' Configure the default driver i. e., the default device. '''
        self.model.clear_capture_device()
        config = self.model.find_config('id', 'default')
        self.model.set_capture_device(config)
        self.configure_device(config, self.CHECK_REQUIRED)

    def start_check(self):
        ''' Run the first step of the configure dialogs. '''
        self.check(self.CHECK_DEVICE)

    def set_remote(self, remote, next_state=None):
        ''' Update the remote. '''
        config = self.model.get_bundled_driver(remote, self.view)
        if config:
            self.model.set_capture_device(config)
        self.model.set_remote(remote)
        if not config:
            return
        if self.model.config['driver'] != config['driver'] or \
            self.model.config['device'] != config['device']:
                self.configure_device(config, next_state)

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
        errmsg = mvc_model.check_resultsdir(self.cli_options['results_dir'])
        self.view.builder.get_object('main_window').show_all()
        if errmsg:
            self.view.show_warning("Invalid results directory", errmsg, True)
            Gtk.main_quit()

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
        log = mvc_model.write_results(self.model.config,
                                      self.cli_options['results_dir'],
                                      self.view)
        self.view.show_info('Installation files written', log)

    def show_remote(self, remote):
        ''' Display remote config file in text window. '''
        # pylint: disable=no-member
        uri = _REMOTES_BASE_URI + '/' + remote + '?format=raw'
        try:
            text = urllib.request.urlopen(uri).read().decode('utf-8',
                                                             errors='ignore')
        except urllib.error.URLError as ex:
            text = "Sorry: cannot download: " + uri + ' (' + str(ex) + ')'
        self.view.show_text(text, 'lirc: Remote config file')


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
