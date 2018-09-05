#!/usr/bin/env python3
#
# Copyright 2017 Two Pore Guys, Inc.
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

import gi
gi.require_version('Gtk', '3.0')

import argparse
import librpc
import signal
import json
from gi.repository import Gtk, GLib, Pango


class Context(object):
    def __init__(self, uri):
        self.uri = uri
        self.client = librpc.Client()
        self.client.connect(uri)


class MainWindow(Gtk.Window):
    def __init__(self, context):
        super().__init__(title=context.uri)
        self.context = context
        self.client = context.client
        self.content = ContentPane(self)
        self.store = Gtk.TreeStore(str, object, object)
        self.sorted_model = Gtk.TreeModelSort(self.store)
        self.sorted_model.set_sort_column_id(0, Gtk.SortType.ASCENDING)
        self.tree = Gtk.TreeView(self.sorted_model)
        self.paned = Gtk.HPaned()
        self.content_box = Gtk.Frame()
        self.init_treeview()
        self.enumerate_tree_level(self.client.instances['/'], None)
        self.paned.add1(scrolled(self.tree))
        self.paned.add2(self.content_box)
        self.add(self.paned)
        self.content_box.add(self.content)
        self.connect('delete-event', Gtk.main_quit)
        self.tree.get_selection().connect('changed', self.on_tree_selection)
        self.set_size_request(800, 600)

    def enumerate_tree_level(self, instance, parent_iter):
        for name, inst in instance.children.items():
            short_name = name[len(instance.name) + 2:]

            if not short_name:
                continue

            if '/' in short_name:
                continue

            it = self.store.append(parent_iter, (name, inst, None))
            self.enumerate_tree_level(inst, it)
            self.enumerate_interfaces(inst, it)

    def enumerate_interfaces(self, instance, iter):
        for name, iface in instance.interfaces.items():
            it = self.store.append(iter, (name, iface, None))
            self.enumerate_members(iface, it)

    def enumerate_members(self, interface, iter):
        for prop in interface.properties:
            self.store.append(iter, (prop.name, prop, interface))

        for name, method in interface.methods.items():
            self.store.append(iter, (name, method, interface))

    def init_treeview(self):
        column = Gtk.TreeViewColumn("Name")
        label = Gtk.CellRendererText()
        column.pack_start(label, True)
        column.add_attribute(label, "text", 0)
        self.tree.append_column(column)

    def on_tree_selection(self, selection):
        model, treeiter = selection.get_selected()
        if treeiter is not None:
            item = model[treeiter][1]
            interface = model[treeiter][2]
            self.content_box.remove(self.content)
            self.content = ContentPane(self, item, interface)
            self.content_box.add(self.content)
            self.content_box.show_all()


class ContentPane(Gtk.Box):
    def __init__(self, window, item=None, interface=None):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.window = window
        self.location = Gtk.Label()
        self.description = Gtk.Label()
        self.description.set_markup('none')
        self.content = None
        location = 'Unknown'

        if not item:
            self.location.set_markup('Please select item from the tree view')

        if isinstance(item, librpc.RemoteObject):
            self.location.set_markup('Instance: {0}'.format(item.path))

        if isinstance(item, librpc.RemoteInterface):
            location = \
                'Instance: {0}\n' \
                'Interface: {1}'.format(
                    item.instance.path,
                    item.interface
                )

            if item.typed and item.typed.description:
                self.description.set_markup(item.typed.description)

        if isinstance(item, librpc.RemoteProperty):
            location = \
                'Instance: {0}\n' \
                'Interface: {1}\n' \
                'Property: {2}'.format(
                    interface.path,
                    interface.interface,
                    item.name
                )

            self.content = PropertyPane(interface, item)
            if item.typed:
                self.description.set_markup(item.typed.description)

        if callable(item):
            location = \
                'Instance: {0}\n' \
                'Interface: {1}\n' \
                'Method: {2}'.format(
                    interface.path,
                    interface.interface,
                    item.name
                )

            if item.typed:
                self.description.set_markup(item.typed.description)

        self.location.set_markup(location)
        self.pack_start(self.location, False, True, 0)
        self.pack_start(self.description, False, True, 0)

        if self.content:
            self.pack_start(self.content, True, True, 0)

        self.show_all()


class InterfacePane(Gtk.Box):
    def __init__(self, item):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)


class PropertyPane(Gtk.Box):
    def __init__(self, interface, item):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.interface = interface
        self.item = item
        self.serializer = librpc.Serializer('json')
        self.font = Pango.FontDescription('monospace 10')
        self.current = self.read()
        self.new = Gtk.TreeView()
        self.actions = Gtk.ActionBar()
        self.change = Gtk.Button('Set value')
        self.actions.pack_start(self.change)
        self.pack_start(Gtk.Label('Current value:'), False, True, 0)
        self.pack_start(scrolled(self.current), True, True, 0)
        self.pack_start(Gtk.Label('New value:'), False, True, 0)
        self.pack_start(scrolled(self.new), True, True, 0)
        self.pack_start(self.actions, False, False, 0)

        self.show_all()

    def read(self):
        try:
            value = self.item.getter(self.interface)
            if isinstance(value, librpc.BaseStruct):
                return StructRenderer(value)
        except librpc.RpcException as err:
            value = err

        dump = self.serializer.dumps(value)
        text = json.dumps(json.loads(dump), indent=4, sort_keys=True)
        result = Gtk.TextView()
        result.set_editable(False)
        result.get_buffer().set_text(text)
        result.modify_font(self.font)
        return result

    def change(self):
        pass


class MethodPane(Gtk.Box):
    def __init__(self, interface, item):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.interface = interface
        self.item = item


class StructRenderer(Gtk.TreeView):
    def __init__(self, item):
        super().__init__(model=Gtk.TreeStore(str, str, str))
        self.store = self.get_model()
        self.init_treeview()
        self.populate_members(item, None)
        self.show_all()

    def init_treeview(self):
        column = Gtk.TreeViewColumn("Name")
        label = Gtk.CellRendererText()
        column.pack_start(label, True)
        column.add_attribute(label, "text", 0)
        self.append_column(column)

        column = Gtk.TreeViewColumn("Description")
        label = Gtk.CellRendererText()
        column.pack_start(label, True)
        column.add_attribute(label, "text", 1)
        self.append_column(column)

        column = Gtk.TreeViewColumn("Value")
        label = Gtk.CellRendererText()
        column.pack_start(label, True)
        column.add_attribute(label, "text", 2)
        self.append_column(column)

    def populate_members(self, item, parent_iter):
        for m in item.typei.type.members:
            value = item[m.name]
            if isinstance(value, librpc.BaseStruct):
                it = self.store.append(parent_iter, (m.name, m.description, ''))
                self.populate_members(value, it)
                continue

            self.store.append(parent_iter, (m.name, m.description, str(value)))


def scrolled(widget):
    ret = Gtk.ScrolledWindow()
    ret.set_vexpand(True)
    ret.add(widget)
    return ret


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uri')
    args = parser.parse_args()
    ctx = Context(args.uri)
    win = MainWindow(ctx)
    win.show_all()

    try:
        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGINT, Gtk.main_quit)
        Gtk.main()
    except KeyboardInterrupt:
        Gtk.main_quit()


if __name__ == '__main__':
    main()
