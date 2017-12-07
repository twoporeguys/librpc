#!/usr/bin/python3
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

import argparse
import traceback
import librpc
import os
import readline


class CommandDispatcher(object):
    def __init__(self, client):
        self.client = client
        self.current = client.instances['/']

    def cmd_show(self, *args):
        print('Interfaces:')
        for i in self.current.interfaces:
            print('    {0}'.format(i))

        print('Methods:')
        for ifname, iface in self.current.interfaces.items():
            for mname in iface.methods:
                print('    {0}.{1}'.format(ifname, mname))

        print('Properties:')
        for ifname, iface in self.current.interfaces.items():
            for pname in iface.properties:
                print('    {0}.{1}'.format(ifname, pname))

    def cmd_cd(self, *args):
        self.current = self.client.instances[args[0]]

    def cmd_pwd(self, *args):
        print(self.current.path)

    def cmd_objects(self, *args):
        for i in sorted(self.client.instances):
            print(i)

    def cmd_call(self, *args):
        iface = self.current.interfaces[args[0]]

        try:
            fn = getattr(iface, args[1])
            params = [eval(s) for s in args[2:]]
            result = fn(*params)

            if hasattr(result, '__next__'):
                for r in result:
                    print(r)
            else:
                print(result)
        except Exception:
            traceback.print_exc()

    def cmd_get(self, *args):
        iface = self.current.interfaces[args[0]]

        try:
            prop = getattr(iface, args[1])
            print(prop)
        except Exception:
            traceback.print_exc()

    def cmd_set(self, *args):
        pass

    def cmd_watch(self, *args):
        pass

    def repl(self):
        while True:
            line = input('{0}> '.format(self.current.path))
            tokens = line.split()

            if not tokens:
                continue

            try:
                fn = getattr(self, 'cmd_{0}'.format(tokens[0]))
                fn(*tokens[1:])
            except AttributeError:
                print('Command not found')
                continue


def on_error(code, args):
    print('Connection closed: {0}'.format(str(code)))
    os._exit(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uri', help='Server URI', required=True)
    args = parser.parse_args()
    client = librpc.Client()
    client.connect(args.uri)
    client.error_handler = on_error
    client.unpack = True

    try:
        cmd = CommandDispatcher(client)
        cmd.repl()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
