''' Simple lirc setup tool - view part. '''

import os.path
import os
import shutil
import subprocess
import sys
import textwrap

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Vte', '2.91')
from gi.repository import Gtk         # pylint: disable=no-name-in-module
from gi.repository import Vte         # pylint: disable=no-name-in-module
from gi.repository import GLib        # pylint: disable=no-name-in-module

import baseview
import config
import util

from baseview import _on_window_delete_event_cb
from mvc_model import RESULTS_DIR

REMOTES_LIST_URL = "http://lirc-remotes.sourceforge.net/remotes.list"
_DEBUG = 'LIRC_DEBUG' in os.environ

NO_REMOTE_INFO = """
If you select this option, you need to provide a lircd.conf file
later, either by finding it elsewhere or by recording your own using
the irrecord tool. Normally you want to install this file in
/etc/lirc/lircd.conf.d."""

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
For some capture devices which cannot be used with the default or the devinput
driver lirc has specific support, often a lirc driver and/or kernel
configuration of the standard driver(s). If you can find your device here
it will configure this support. """

MANUAL_DEVICE_INFO = """
If you are happy with the driver but wants to change the device you can
do it here. """

LOCAL_FILE_INFO = """
If you already have a suitable lircd.conf file at hand, you can use that
instead of fetching one from the remotes database.
"""

MAIN_HELP = """

<big>LIRC Configuration Tool Help.</big>

NOTE: This tool is in alpha stage! Here are numerous bugs and shortcomings.
Please report issues on the mailing list or issue tracker at
https://sourceforge.net/projects/lirc/!

The tool allows you to configure lirc. This is done in three steps.

In the first step you should select a driver which can handle your
capture device e. g., a usb dongle or a DIY serial device. This is the
top pane of the window. After selecting driver and device you should
use the Test button to verify your settings so far.

In the second step you should select a configuration file which corresponds
to your remote. You can search for remotes from existing ones, browse brands,
use an existing file or select to not use any pre-configured file. In the
last case you probably wants to record your own configuration file using
irrecord(1) later. This is the bottom pane of the window.

Actually, it doesn't really matter if you select remote or capture device
first. You can do it in any order. However, testing the driver makes a
lot of sense before testing the remote configuration.

If you select a remote which only can be used with a specific capture
device the capture device will be updated automagically. Likewise, if you
select a capture device which requires a specific remote the remote will be
updated.

In the last step you should install the configuration. This will write a set
of configuration files to the results directory, normally lirc-setup.conf.d.
You should then install these files into their proper locations, see the
README file in the results directory."""

CONFIG_GUIDE_URL = \
    "file://@docdir@/lirc.org/html/configuration-guide.html#permissions" \
    .replace("@docdir@", config.DOCDIR)

MODE2_HELP_MSG = """
Run the command (push button) and then push buttons on your remote. There
should be either be pulses and spaces:
             pulse 450
             space 450
             pulse 850
             space 950
             pulse 450
             space 450
             pulse 450
or codes:
             code: 0x146f9a0000
             code: 0x146f9a0000
             code: 0x146f9a0000
             code: 0x146f9a0000
             code: 0x146f9a0000
             code: 0x146f9a0000

If mode2 cannot start with messages like "Cannot initiate device" it can
be a hardware problem. However, it might also be that you cannot access
the device. See the
<a href="@guide_url@">Configuration Manual, Adjusting Device Permissions </a>.
""".replace("@guide_url@", CONFIG_GUIDE_URL)

IRW_HELP_MSG = """
Run the command (push button) and then push buttons on your remote. There
should be decoded output, like
000000037ff07be1 00 KEY_UP mceusb
000000037ff07be1 01 KEY_UP mceusb
000000037ff07be1 02 KEY_UP mceusb
000000037ff07be1 03 KEY_UP mceusb
000000037ff07be1 04 KEY_UP mceusb
000000037ff07be1 05 KEY_UP mceusb
000000037ff07be0 00 KEY_DOWN mceusb
000000037ff07be0 01 KEY_DOWN mceusb
000000037ff07be0 02 KEY_DOWN mceusb
000000037ff07be0 03 KEY_DOWN mceusb

Note how the second digit is incremented for each repeated button. This
is important for correct operation.
"""

RESULTS_DIR_MSG = """
I can clear it now if you press <b>Yes</b>. Use <b>No</b> to exit and
manually take care of the files or use another directory."""

NO_DIR_BROWSER_MSG = """
There is no configured directory browser Fix using <i> xdg-mime default
nautilus.desktop inode/directory</i> or so.
"""


def _get_lines_by_letter(lines):
    '''' Return a dictionary keyed with first letter of lines. '''

    all_keys = [chr(c) for c in range(ord('a'), ord('z') + 1)]
    all_keys.extend([chr(c) for c in range(ord('0'), ord('9') + 1)])
    lines_by_letter = {}
    for c in all_keys:
        lines_by_letter[c] = []
    for l in lines:
        lines_by_letter[l[0].lower()].extend([l])
    return lines_by_letter


class DecodeTestWindow(Gtk.Window):
    ''' Window for running irw test in a Vte terminal. '''

    def __init__(self, model, view):

        def show_command():
            """ User clicked Show Command button. """
            lines = textwrap.wrap(self.command, 60,
                                  break_long_words=False,
                                  break_on_hyphens=False)
            text = ' \\\n'.join(lines)
            self.view.show_info("lircd + irw command string", text)

        def do_quit():
            """ User clicked "Quit" button. """
            cmd = self.kill + '\n'
            self.terminal.feed_child(cmd, len(cmd))
            self.destroy()

        Gtk.Window.__init__(self, title="Test decoding")
        self.view = view
        ok, self.command, self.kill = model.get_irwcheck()
        if not ok:
            view.show_error("Test setup error", self.command)
            return
        self.command += '\n'
        self.set_default_size(600, 300)
        self.terminal = Vte.Terminal()
        self.terminal.spawn_sync(
            Vte.PtyFlags.DEFAULT,
            os.getcwd(),
            ['/bin/sh'],
            [],
            GLib.SpawnFlags.DO_NOT_REAP_CHILD,
            None,
            None,
        )
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)

        scroller = Gtk.ScrolledWindow()
        scroller.set_hexpand(True)
        scroller.set_vexpand(True)
        scroller.add(self.terminal)
        vbox.pack_start(scroller, False, True, 1)

        self.run_btn = Gtk.Button("Run command")
        self.quit_btn = Gtk.Button("Quit")
        self.cmd_btn = Gtk.Button("Show command")
        help_btn = Gtk.Button("Help")
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        hbox.set_spacing(10)
        hbox.pack_end(self.run_btn, False, True, 10)
        hbox.pack_end(self.quit_btn, False, True, 10)
        hbox.pack_end(self.cmd_btn, False, True, 10)
        hbox.pack_start(help_btn, False, True, 10)
        vbox.pack_start(hbox, False, True, 2)

        self.add(vbox)
        self.set_focus(self.run_btn)
        self.run_btn.connect("clicked", self.input_to_term)
        self.quit_btn.connect("clicked", lambda b: do_quit())
        self.cmd_btn.connect("clicked", lambda b: show_command())
        help_btn.connect(
            "clicked", lambda b: view.show_info("mode2 help", IRW_HELP_MSG))
        self.connect("delete-event", lambda e, v: do_quit())

    def input_to_term(self, dummy):
        """ User clicked Run Command button. """
        length = len(self.command)
        self.terminal.feed_child(self.command, length)


class DeviceTestWindow(Gtk.Window):
    ''' Window for running mode2 test in a Vte terminal. '''

    def __init__(self, model, view):

        def show_command():
            """ User clicked Show Command button. """
            lines = textwrap.wrap(self.command, 60,
                                  break_long_words=False,
                                  break_on_hyphens=False)
            text = ' \\\n'.join(lines)
            self.view.show_info("mode2 command string", text)

        def show_log():
            """ User clicked View log button. """
            path = os.path.expanduser("~/.cache/mode2.log")
            if not os.path.exists(path) or os.stat(path)[6] < 10:
                self.view.show_info("No log available")
                return
            with open(path, "r") as f:
                log = '\n'.join(f.readlines())
            self.view.show_info("mode2 logfile", log)

        Gtk.Window.__init__(self, title="Test device and driver setup")
        self.view = view
        self.command = model.get_mode2() + '\n'
        self.set_default_size(600, 300)
        self.terminal = Vte.Terminal()
        self.terminal.spawn_sync(
            Vte.PtyFlags.DEFAULT,
            os.getcwd(),
            ['/bin/sh'],
            [],
            GLib.SpawnFlags.DO_NOT_REAP_CHILD,
            None,
            None,
        )
        self.run_btn = Gtk.Button("Run command")
        self.cmd_btn = Gtk.Button("Show command")
        self.log_btn = Gtk.Button("View log")
        self.run_btn.connect("clicked", self.input_to_term)
        self.quit_btn = Gtk.Button("Quit")
        self.quit_btn.connect("clicked", lambda b: self.destroy())
        self.cmd_btn.connect("clicked", lambda b: show_command())
        self.log_btn.connect("clicked", lambda b: show_log())
        self.help_btn = Gtk.Button("Help")
        self.help_btn.connect("clicked",
                              lambda b: view.show_info("mode2 help",
                                                       MODE2_HELP_MSG))
        self.set_focus(self.run_btn)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        scroller = Gtk.ScrolledWindow()
        scroller.set_hexpand(True)
        scroller.set_vexpand(True)
        scroller.add(self.terminal)
        vbox.pack_start(scroller, False, True, 1)
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        hbox.set_spacing(5)
        hbox.pack_end(self.run_btn, False, True, 5)
        hbox.pack_end(self.cmd_btn, False, True, 5)
        hbox.pack_end(self.log_btn, False, True, 5)
        hbox.pack_end(self.quit_btn, False, True, 5)
        hbox.pack_start(self.help_btn, False, True, 5)
        vbox.pack_start(hbox, False, True, 2)
        self.add(vbox)

    def input_to_term(self, dummy):
        """ User clicked Run Command button. """
        length = len(self.command)
        self.terminal.feed_child(self.command, length)


class View(baseview.Baseview):
    ''' The View part of the MVC pattern. '''

    def __init__(self, builder, model):
        self.builder = builder
        baseview.Baseview.__init__(self, self)
        self.model = model
        self.controller = None
        self.selected = None
        self._main_window_connect()
        self.test_window = None
        model.add_listener(self.on_model_change)
        self.on_model_change()

    def _main_window_connect(self):
        ''' Connect signals for main window. '''

        def on_view_config_btn_cb(btn):
            ''' User pressed the view config file button. '''
            label = self.builder.get_object('selected_remote_lbl')
            self.controller.show_remote(label.get_text())
            return True

        def on_info_btn_clicked(txt):
            ''' Handle ? help button  buttons. '''
            lbl = self.builder.get_object('selected_remote_lbl').get_text()
            if _DEBUG:
                print("Selected remote label: " + lbl)
            self.show_text(txt, "lirc: help")

        def on_search_config_btn_clicked_cb(btn, data=None):
            ''' User clicked "Search remote" button. '''
            lbl = self.builder.get_object('search_config_entry')
            self.controller.select_remote(lbl.get_text())

        def on_local_file_choosen_cb(btn):
            ''' User selected a file in the Use Loca File dialog.'''
            path = btn.get_filename()
            self.model.set_remote(path)

        def on_device_test_clicked_cb(btn, data=None):
            ''' User cliecked "Test" for capture device. '''
            msg = util.check_kerneldevice(self.model.config.device)
            if msg:
                self.show_warning('Bad device', msg)
            self.test_window = DeviceTestWindow(self.model, self)
            self.test_window.show_all()

        def on_config_test_clicked_cb(btn, data=None):
            ''' User clicked "Test" for decoding test. '''
            self.test_window = DecodeTestWindow(self.model, self)
            self.test_window.show_all()

        # def on_driver_info_clicked_cb(btn, data=None):
        #    ''' User clicked 'Info' for current driver. '''
        #    info = self.model.driver_info()
        #    if info:
        #        self.show_link_info("Driver info", info)

        clicked_connects = [
            ('preconfig_device_btn', lambda b: self.show_preconfig_dialog()),
            ('view_config_btn', on_view_config_btn_cb),
            ('view_driver_btn', lambda b: self.show_driver()),
            ('driver_info_btn', lambda b: self.show_link_info(
                "Driver info", self.model.driver_info())),
            ('main_browse_btn', lambda b: self.show_config_browse_cb()),
            ('no_remote_btn', lambda b: self.model.set_manual_remote()),
            ('devinput_btn', lambda b: self.controller.show_devinput()),
            ('default_btn', lambda b: self.controller.show_default()),
            ('device_test_btn', on_device_test_clicked_cb),
            ('config_test_btn', on_config_test_clicked_cb),
            ('exit_btn', lambda b: Gtk.main_quit()),
            ('search_btn', on_search_config_btn_clicked_cb),
            ('install_btn', lambda b: self.controller.write_results()),
            ('restart_btn', lambda b: self.controller.restart()),
            ('manual_device_help_btn',
             lambda b: on_info_btn_clicked(MANUAL_DEVICE_INFO)),
            ('manual_device_btn',
             lambda b: self.on_manual_device_clicked_cb()),
            ('no_remote_help_btn',
             lambda b: on_info_btn_clicked(NO_REMOTE_INFO)),
            ('devinput_help_btn',
             lambda b: on_info_btn_clicked(DEVINPUT_INFO)),
            ('default_help_btn',
             lambda b: on_info_btn_clicked(DEFAULT_INFO)),
            ('local_file_help_btn',
             lambda b: on_info_btn_clicked(LOCAL_FILE_INFO)),
            ('main_help_btn', lambda b: on_info_btn_clicked(MAIN_HELP)),
            ('preconfig_help_btn',
             lambda b: on_info_btn_clicked(PRECONFIG_INFO))
        ]
        w = self.builder.get_object('main_window')
        w.connect('delete-event', lambda w, e: Gtk.main_quit())
        b = self.builder.get_object("remote_choose_button")
        filter1 = Gtk.FileFilter()
        filter1.set_name('Configuration files')
        filter1.add_pattern('*lircd.conf')
        b.add_filter(filter1)
        filter2 = Gtk.FileFilter()
        filter2.set_name('All files')
        filter2.add_pattern('*')
        b.add_filter(filter2)
        b.connect('file-set', on_local_file_choosen_cb)
        for btn_name, handler in clicked_connects:
            self.builder.get_object(btn_name).connect('clicked', handler)

    def show_text(self, text, title="lirc: show file", on_ok_cb=None):
        ''' Read-only text display in a textview. '''

        def cb_on_view_ok_btn_clicked(button, data=None):
            ''' OK button on view_some_text window. '''
            button.get_toplevel().hide()
            if on_ok_cb:
                on_ok_cb()
            else:
                return True

        text = text.replace("&", "&amp;")
        self.builder.get_object("show_text_label").set_markup(text)
        w = self.builder.get_object('view_text_window')
        w.set_title(title)
        if not self.test_and_set_connected('view_text_window'):
            w.connect('delete-event', _on_window_delete_event_cb)
            b = self.builder.get_object('view_text_ok_btn')
            b.connect('clicked', cb_on_view_ok_btn_clicked)
            w.set_focus(b)
        w.show_all()

    def set_controller(self, controller):
        ''' Set the controller, a circular dependency. '''
        self.controller = controller

    def on_model_change(self):
        ''' Update view to match model.'''
        text = "(None)"
        sense_conf = False
        if self.model.has_lircd_conf():
            text = self.model.config.lircd_conf
            if not self.model.is_remote_manual():
                sense_conf = True
        self.builder.get_object('selected_remote_lbl').set_text(text)
        self.builder.get_object('drv_select_remote_lbl').set_text(text)
        self.builder.get_object('view_config_btn').set_sensitive(sense_conf)

        text = 'Selected capture device (None)'
        sensitive = True if self.model.has_label() else False
        if sensitive:
            text = self.model.config.label
        self.builder.get_object('view_driver_btn').set_sensitive(sensitive)
        self.builder.get_object('device_test_btn').set_sensitive(sensitive)
        self.builder.get_object('config_test_btn').set_sensitive(
            self.model.is_testable())
        self.builder.get_object('selected_driver_lbl').set_text(text)
        self.builder.get_object('selected_driver_lbl').queue_draw()
        btn = self.builder.get_object('install_btn')
        btn.set_sensitive(self.model.is_installable())
        infobtn = self.builder.get_object('driver_info_btn')
        infobtn.set_sensitive(bool(self.model.driver_info()))
        devicebtn = self.builder.get_object('manual_device_btn')
        devicebtn.set_sensitive(bool(self.model.config.device))

    def show_resultdir(self, header, text, results_dir):
        """ Show data for written config files. """
        # pylint: disable=not-callable
        buttons = ("View results", Gtk.ResponseType.APPLY,
                   "OK", Gtk.ResponseType.OK)
        d = Gtk.MessageDialog(self.builder.get_object('main_window'),
                              0,
                              Gtk.MessageType.INFO,
                              buttons,
                              header)
        d.format_secondary_markup(text)
        response = d.run()
        if response == Gtk.ResponseType.APPLY:
            if 'XDG_CURRENT_DESKTOP' in os.environ:
                if os.environ['XDG_CURRENT_DESKTOP'] == 'GNOME':
                    subprocess.Popen(["nautilus", results_dir])
            else:
                try:
                    browser = subprocess.check_output(
                        "xdg-mime query default inode/directory", shell=True)
                except (OSError, subprocess.CalledProcessError):
                    browser = None
                if not browser:
                    self.show_warning("Bad desktop config",
                                      NO_DIR_BROWSER_MSG)
                else:
                    subprocess.check_call(["xdg-open", results_dir])
        d.destroy()

    def load_configs(self):
        ''' Load config files into model. -> control... '''
        if not self.model.db:
            self.view.show_warning("Cannot find the configuration files")

    def show_driver(self):
        ''' Display data for current driver. '''

        def format_path(cf, key):
            ''' Format a lircd.conf/lircmd.conf entry. '''
            if not hasattr(cf, key) or not getattr(cf, key):
                return 'Not set'
            return os.path.basename(getattr(cf, key))

        items = [
            ('Driver', 'driver'),
            ('lircd.conf file', 'lircd_conf'),
            ('lircmd.conf file', 'lircmd_conf'),
            ('lircd --device option', 'device'),
            ('Kernel setup code', 'modinit'),
            ('Kernel modprobe config', 'modprobe'),
        ]
        s = ''
        for label, key in items:
            s += "%-24s: " % label
            if key in ['lircd_conf', 'lircmd_conf']:
                s += format_path(self.model.config, key)
            elif key == 'driver':
                s += str(getattr(self.model.config, 'driver')['id'])
            else:
                w = str(getattr(self.model.config, key))
                s += w if w else 'Not set'
            s += "\n"
        s = "<tt>" + s + "</tt>"
        self.show_info('lirc: Driver configuration', s)

    def show_select_lpt_window(self):
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
            button.get_toplevel().hide()
            self.model.set_lpt_parms(irq, iobase, btn_id)

        for btn in ports.keys():
            b = self.builder.get_object(btn + '_lpt_btn')
            b.lirc_id = btn
        if not self.test_and_set_connected('select_lpt_window'):
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
                return True
            else:
                self.model.set_device(numtext)
                self.controller.modprobe_done(None)
                cf = self.model.db.configs['udp']
                self.model.set_config(cf)
                self.model.config.device = numtext
            button.get_toplevel().hide()

        def on_udp_port_back_cb(button, data=None):
            ''' User clicked the Back button. '''
            button.get_toplevel().hide()

        if not self.test_and_set_connected('select_udp_port_window'):
            b = self.builder.get_object('udp_port_next_btn')
            b.connect('clicked', on_udp_port_next_cb)
            b = self.builder.get_object('udp_port_back_btn')
            b.connect('clicked', on_udp_port_back_cb)
        w = self.builder.get_object('select_udp_port_window')
        entry = self.builder.get_object('udp_port_entry')
        entry.set_text("8765")
        w.show_all()

    def show_select_com_window(self):
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
            self.model.set_serial_parms(irq, iobase, btn_id)
            button.get_toplevel().hide()

        def on_com_select_back_cb(button, data=None):
            ''' User clicked the Back button. '''
            button.get_toplevel().hide()

        for btn in ports.keys():
            b = self.builder.get_object(btn + '_btn')
            b.lirc_id = btn
        if not self.test_and_set_connected('select_com_window'):
            b = self.builder.get_object('select_com_next_btn')
            b.connect('clicked', on_com_select_next_cb)
            b = self.builder.get_object('select_com_back_btn')
            b.connect('clicked', on_com_select_back_cb)
            self.connected.add('select_com_window')
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
        if not self.test_and_set_connected('preconfig_window'):
            w.connect('delete-event', _on_window_delete_event_cb)
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

            treeview = self._create_treeview('preconfig_items_view',
                                             ['Configuration'])
            treeview.get_selection().connect('changed',
                                             on_treeview_change_cb)
            liststore = treeview.get_model()
            liststore.clear()
            db = self.model.db
            labels = [db.configs[l]['label']
                      for l in db.configs.keys()
                      if db.configs[l]['menu'] == menu]

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
            try:
                cf = self.model.db.find_config('label', label.get_text())
            except KeyError:
                self.show_error("This driver is not available.")
            else:
                self.model.set_config(cf)
                button.get_toplevel().hide()
                self.builder.get_object('preconfig_window').hide()
                self.controller.start_check()
            return True

        w = self.builder.get_object('preconfig_select_window')
        if not self.test_and_set_connected('preconfig_select_window'):
            build_treeview(menu)
            w.connect('delete-event', _on_window_delete_event_cb)
            b = self.builder.get_object('preconfig_select_back_btn')
            b.connect('clicked', lambda b: w.hide())
            b = self.builder.get_object('preconfig_select_next_btn')
            b.connect('clicked', on_preconfig_next_clicked_cb)
        w.show_all()

    def show_single_remote(self, line):
        ''' Display search results of a single match. '''

        def on_next_btn_clicked_cb(btn, id_):
            ''' User clicked "Next" button. '''
            self.controller.set_remote(id_)
            btn.get_toplevel().hide()

        words = line.split(';')
        id_ = words[0] + '/' + words[1]
        s = "Path: " + id_
        self.builder.get_object('single_remote_config_lbl').set_text(s)
        s = "Supported remotes: " + words[4]
        self.builder.get_object('single_remote_supports_lbl').set_text(s)
        w = self.builder.get_object('single_remote_select_window')
        if not self.test_and_set_connected('single_remote_select_window'):
            btn = self.builder.get_object('single_remote_next_btn')
            btn.connect('clicked', on_next_btn_clicked_cb, id_)
            btn = self.builder.get_object('single_remote_back_btn')
            btn.connect('clicked', lambda b: w.hide())
        w.show_all()

    def show_search_results_select(self, lines):
        ''' User  has entered a search pattern, let her choose match.'''

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

        def load_liststore(treeview, found):
            ''' Reload the liststore data model. '''
            liststore = treeview.get_model()
            liststore.clear()
            for l in found:
                words = l.split(';')
                liststore.append([words[0], words[1], words[4]])
            treeview.set_model(liststore)

        treeview = self._create_treeview('search_results_view',
                                         ['vendor', 'lircd.conf', 'device'])
        load_liststore(treeview, lines)
        if not self.test_and_set_connected('search_select_window'):
            treeview.get_selection().connect('changed', on_select_change_cb)
            b = self.builder.get_object('search_back_btn')
            b.connect('clicked', lambda b, d=None: b.get_toplevel().hide())
            b = self.builder.get_object('search_next_btn')
            b.connect('clicked', on_search_next_btn_cb)
        w = self.builder.get_object('search_select_window')
        w.show_all()

    def results_dir_reset(self, errmsg):
        """ Handle non-empty results dir on startup. """
        d = Gtk.MessageDialog(self.builder.get_object('main_window'),
                              0,
                              Gtk.MessageType.INFO,
                              Gtk.ButtonsType.YES_NO,
                              errmsg)
        d.format_secondary_markup(RESULTS_DIR_MSG)
        reply = d.run()
        if reply == Gtk.ResponseType.YES:
            shutil.rmtree(RESULTS_DIR)
            d.destroy()
        elif reply == Gtk.ResponseType.NO:
            sys.exit(0)

    def show_config_browse_cb(self, button=None, data=None):
        ''' User clicked browse configs button. '''

        def build_treeview():
            ''' Construct the remotes browse liststore treeview. '''
            treeview = self._create_treeview('config_browse_view', ['path'])
            treestore = Gtk.TreeStore(str)
            treeview.set_model(treestore)
            return treeview

        def fill_treeview(treeview):
            ''' Fill the treestore with browse options. '''
            treestore = treeview.get_model()
            if hasattr(treestore, 'lirc_is_inited'):
                return
            treestore.clear()
            lines = self.model.get_remotes_list(self)
            lines_by_letter = _get_lines_by_letter(lines)
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
            self.controller.show_remote(lbl.get_text())

        w = self.builder.get_object('config_browse_window')
        if not self.test_and_set_connected('config_browse_window'):
            w.connect('delete-event', _on_window_delete_event_cb)
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

    def on_manual_device_clicked_cb(self, button=None):
        """ User clicked on "Manual device update". """

        def on_ok_cb(button):
            """ Indeed: user clicked OK. """
            entry = self.builder.get_object("manual_device_entry")
            self.model.set_device(entry.get_text())
            w = self.builder.get_object("manual_device_window")
            w.hide()

        def on_cancel_cb(button):
            """ Indeed: user clicked Cancel. """
            w = self.builder.get_object("manual_device_window")
            w.hide()

        w = self.builder.get_object("manual_device_window")
        entry = self.builder.get_object("manual_device_entry")
        entry.set_text(self.model.config.device)
        ok = self.builder.get_object("manual_device_ok_btn")
        ok.connect('clicked', on_ok_cb)
        cancel = self.builder.get_object("manual_device_cancel_btn")
        cancel.connect('clicked', on_cancel_cb)
        w.show_all()


# vim: set expandtab ts=4 sw=4:
