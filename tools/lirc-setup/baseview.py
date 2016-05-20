''' Simple lirc setup tool - common parts of view components '''

import sys

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk         # pylint: disable=no-name-in-module


def _hasitem(dict_, key_):
    ''' Test if dict contains a non-null value for key. '''
    return key_ in dict_ and dict_[key_]


def _on_window_delete_event_cb(window, event):
    ''' Generic window close event. '''
    window.hide()
    return True


class Baseview(object):
    ''' Common parts in MVC view components. '''

    def __init__(self, view):
        self.connected = set()
        self.builder = view.builder

    def test_and_set_connected(self, window_id):
        ''' Test and set window_id in self.connected. '''
        if window_id in self.connected:
            return True
        self.connected.add(window_id)
        return False

    def reconnect(self, widget, signal, callback, data=None):
        ''' Reconnect callback as handling signal, clear old connects. '''
        if isinstance(widget, str):
            widget = self.builder.get_object(widget)
        try:
            widget.disconnect_by_func(callback)
        except TypeError:
            pass
        widget.connect(signal, callback, data)

    def _create_treeview(self, object_id, columns):
        ''' Construct a treeview with some string columns. '''

        treeview = self.builder.get_object(object_id)
        if len(treeview.get_columns()) > 0:
            return treeview
        treeview.set_vscroll_policy(Gtk.ScrollablePolicy.NATURAL)
        types = [str for c in columns]
        treeview.set_model(Gtk.ListStore(*types))
        renderers = {}
        for i, colname in enumerate(columns):
            renderers[colname] = Gtk.CellRendererText()
            column = Gtk.TreeViewColumn(colname, renderers[colname], text=i)
            column.clickable = True
            treeview.append_column(column)
        return treeview

    def _message_dialog(self, header, kind, body, exit_):
        ''' Return a standard error/warning/info dialog. '''
        # pylint: disable=not-callable
        d = Gtk.MessageDialog(self.builder.get_object('main_window'),
                              0,
                              kind,
                              Gtk.ButtonsType.OK,
                              header)
        if body:
            body = body.replace('@', ' at ').replace('|', ' pipe ')
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

    def show_link_info(self, header, body):
        ''' Show a info message, handling "See: <link>" by linking. '''
        template = 'See: <a href="{link}" title="{link}">{link}</a>'
        if body.lower().startswith('see') and (len(body.split(' ')) == 2):
            link = body.split(' ')[1]
            if link.endswith('.'):
                link = link[:-1]
            body = template.format(link=link)
        self._message_dialog(header, Gtk.MessageType.INFO, body, False)


# vim: set expandtab ts=4 sw=4:
