''' Simple lirc setup tool - view part. '''

import os.path
import os

from gi.repository import Gtk         # pylint: disable=no-name-in-module

import baseview

from baseview import _hasitem
from baseview import _on_window_delete_event_cb

REMOTES_LIST_URL = "http://lirc-remotes.sourceforge.net/remotes.list"
_DEBUG = 'LIRC_DEBUG' in os.environ

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
For some capture devices which cannot be used with the default or the devinput
driver lirc has specific support, often a lirc driver and/or kernel
configuration of the standard driver(s). If you can find your device here
it will configure this support. """

MAIN_HELP = """

<big>LIRC Configuration Tool Help.</big>

NOTE: This tool is in alpha stage! Here are numerous bugs and shortcomings.
Please report issues on the mailing list or issue tracker at
https://sourceforge.net/projects/lirc/!

The tool allows you to configure lirc. This is done in three steps.

In the first you should select a configuration file which corresponds to
your remote. You can search for remotes from existing ones, browse brands
or select to not use any pre-configured file. In the last case you
probably wants to record your own configuration file using irrecord(1)
later.  This is the top pane of the window

In the second step you should select a driver which can handle your
capture device e. g., a usb dongle or a DIY serial device. This is the
bottom pane of the window.

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


class View(baseview.Baseview):
    ''' The View part of the MVC pattern. '''

    def __init__(self, builder, model):
        self.builder = builder
        baseview.Baseview.__init__(self, self)
        self.model = model
        self.controller = None
        self.selected = None
        self._main_window_connect()
        model.add_listener(self.on_model_change)

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

        clicked_connects = [
            ('preconfig_device_btn', lambda b: self.show_preconfig_dialog()),
            ('view_config_btn', on_view_config_btn_cb),
            ('view_driver_btn', lambda b: self.show_driver()),
            ('main_browse_btn', lambda b: self.show_config_browse_cb()),
            ('no_remote_btn', lambda b: self.model.set_manual_remote()),
            ('devinput_btn', lambda b: self.controller.show_devinput()),
            ('default_btn', lambda b: self.controller.show_default()),
            ('exit_btn', lambda b: Gtk.main_quit()),
            ('search_btn', on_search_config_btn_clicked_cb),
            ('install_btn', lambda b: self.controller.write_results()),
            ('restart_btn', lambda b: self.controller.restart()),
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
        w.show_all()

    def set_controller(self, controller):
        ''' Set the controller, a circular dependency. '''
        self.controller = controller

    def on_model_change(self):
        ''' Update view to match model.'''
        text = "(None)"
        sense_conf = False
        if self.model.has_lircd_conf():
            text = self.model.config['lircd_conf']
            if not self.model.is_remote_manual():
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

        def format_path(config, key):
            ''' Format a lircd.conf/lircmd.conf entry. '''
            if not _hasitem(config, key):
                return 'Not set'
            return os.path.basename(config[key])

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
            if key in ['lircd_conf', 'lircmd_conf']:
                s += format_path(self.model.config, key)
            else:
                s += str(self.model.config[key]) \
                    if key in self.model.config else 'Not set'
            s += "\n"
        s = "<tt>" + s + "</tt>"
        self.show_info('lirc: Driver configuration', s)

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
                modsetup = '%s: iobase=%s irq=%s' % (module, iobase, irq)
            button.get_toplevel().hide()
            self.controller.modinit_done(modsetup)

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
                self.controller.modinit_done(None)
                device_dict = dict(self.model.find_config('id', 'udp'))
                device_dict['label'] += ' ' + numtext
                device_dict['device'] = numtext
                self.model.set_capture_device(device_dict)
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
                modsetup = '%s: iobase=%s irq=%s' % (module, iobase, irq)
            self.controller.modinit_done(modsetup)
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


# vim: set expandtab ts=4 sw=4:
