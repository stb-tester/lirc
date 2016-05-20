'''
 Simple lirc setup tool - choosers.

Classes to select device and remote, part of the MVC view
'''

import os
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk         # pylint: disable=no-name-in-module

import mvc_model
import baseview

_DEBUG = 'LIRC_DEBUG' in os.environ


def on_window_delete_event_cb(window, event):
    ''' Generic window close event. '''
    window.hide()
    return True


class RemoteSelector(baseview.Baseview):
    ''' Select remote, possibly using a dialog. '''

    def __init__(self, controller, on_select_cb):
        baseview.Baseview.__init__(self, controller.view)
        self.view = controller.view
        self.controller = controller
        self.on_select_cb = on_select_cb
        self.prev_window = \
            self.view.builder.get_object('preconfig_select_window')
        self.next_window = \
            self.view.builder.get_object('main_window')

    def select(self, remotes):
        ''' Select a remote with a driver attribute 'driver' '''

        def build_treeview(remotes):
            ''' Construct the remotes liststore treeview. '''
            treeview = self._create_treeview('drv_select_remote_view',
                                             ['Remote'])
            treeview.get_model().clear()
            for l in sorted(remotes):
                treeview.get_model().append([l])
            return treeview

        def on_select_next_cb(button, data=None):
            ''' User pushed 'Next' button. '''
            lbl = self.view.builder.get_object('drv_select_remote_lbl')
            self.on_select_cb(lbl.get_text())
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

        l = self.view.builder.get_object('drv_select_remote_main_lbl')
        l.set_text("Select remote configuration for driver")
        w = self.view.builder.get_object('drv_select_remote_window')
        treeview = build_treeview(remotes)
        if not self.test_and_set_connected('drv_select_remote_window'):
            w.connect('delete-event', on_window_delete_event_cb)
            treeview.get_selection().connect('changed',
                                             on_treeview_change_cb)
            b = self.view.builder.get_object('drv_select_remote_next_btn')
            b.connect('clicked', on_select_next_cb)
            b = self.view.builder.get_object('drv_select_remote_back_btn')
            b.connect('clicked', on_select_back_cb)
            b.set_sensitive(True)
        w.show_all()


class DeviceSelector(baseview.Baseview):
    ''' Base class for selecting lircd  --device option setup. '''

    help_label = None
    device_label = 'default driver on %device'
    intro_label = 'Select device for the TBD driver.'

    def __init__(self, device_list, view, on_select_cb):
        baseview.Baseview.__init__(self, view)
        self.label_by_device = device_list.label_by_device
        self.driver_id = device_list.driver_id
        self.view = view
        self.group = None
        self.view.load_configs()
        self.on_select_cb = on_select_cb
        w = view.builder.get_object('select_dev_window')
        self.reconnect(w, 'delete-event', on_window_delete_event_cb)
        b = view.builder.get_object('select_dev_ok_btn')
        self.reconnect(b, 'clicked', self.on_ok_btn_clicked_cb)

    def on_option_btn_toggled_cb(self, button, device):
        ''' User selected a device. '''
        self.view.model.set_device('/dev/' + device)

    def on_help_clicked_cb(self, button, device):
        ''' Display dmesg output for given device. '''
        lines = mvc_model.get_dmesg_help(device)
        if lines:
            self.view.show_text('\n'.join(lines))
        else:
            self.view.show_text(
                'No dmesg info found for ' + device)

    def on_ok_btn_clicked_cb(self, button, data=None):
        ''' User clicked OK, go ahead and select active device. '''
        for b in self.group.get_group():
            if b.get_active():
                self.on_select_cb(self.driver_id, b.lirc_name)
                break
        else:
            if _DEBUG:
                print("No active button?!")
        button.get_toplevel().hide()
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
            self.reconnect(radio_buttons[device],
                           'toggled',
                           self.on_option_btn_toggled_cb,
                           device)
            size = len(radio_buttons)
            grid.attach(radio_buttons[device], 0, size, 1, 1)
            radio_buttons[device].lirc_name = device
            if self.help_label:
                help_buttons[device] = Gtk.Button(self.help_label)
                self.reconnect(help_buttons[device],
                               'clicked',
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

    help_label = 'dmesg info'
    intro_label = 'Select /dev/lirc device for the default driver.'
    device_label = 'Default driver on %device'


class SerialDeviceSelector(DeviceSelector):
    ''' Let user select a device for a userspace driver. '''

    def __init__(self, device_list, view, on_select_cb):
        DeviceSelector.__init__(self, device_list, view, on_select_cb)
        self.intro_label = 'Select %s device for the %s driver.' \
            % (device_list.device_pattern, device_list.driver_id)
        self.device_label = '%s driver on %s' \
            % (device_list.device_pattern, device_list.driver_id)


class GenericDeviceSelector(DeviceSelector):
    ''' Let user select a device for a generic userspace driver. '''

    def __init__(self, device_list, view, on_select_cb):
        DeviceSelector.__init__(self, device_list, view, on_select_cb)
        self.intro_label = 'Select %s device for the %s driver.' \
            % (device_list.device_pattern, device_list.driver_id)
        self.device_label = '%s driver on %s' \
            % (device_list.device_pattern, device_list.driver_id)


class UdpPortSelector(DeviceSelector):
    ''' Let user select a udp port  for a udp driver. '''

    def show_dialog(self):
        ''' No options, just run the dialog.... '''
        self.view.show_select_udp_port_window()


class AutoDeviceSelector(DeviceSelector):
    ''' Show a message for automatically selected devices. '''

    def show_dialog(self):
        ''' No options, just display the message.... '''
        self.view.show_info("Driver sets device automatically.")


def factory(device_list, view, on_select_cb):
    ''' Return a DeviceSelector handling device_list (a DeviceList).'''
    pattern = device_list.device_pattern
    if 'tty' in pattern:
        return SerialDeviceSelector(device_list, view, on_select_cb)
    elif 'lirc' in pattern:
        return LircDeviceSelector(device_list, view, on_select_cb)
    elif 'event' in pattern:
        return EventDeviceSelector(device_list, view, on_select_cb)
    elif pattern == 'udp_port':
        return UdpPortSelector(device_list, view, on_select_cb)
    elif pattern == 'auto':
        return AutoDeviceSelector(device_list, view, on_select_cb)
    else:
        return GenericDeviceSelector(device_list, view, on_select_cb)


# vim: set expandtab ts=4 sw=4:
