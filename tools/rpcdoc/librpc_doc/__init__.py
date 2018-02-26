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

import os
import argparse
import glob
import shutil
import mako
import mako.template
import mako.lookup
import librpc
from pkg_resources import resource_string


curdir = os.path.abspath(os.path.dirname(__file__))
templates_path = os.path.join(curdir, 'templates')
lookup = mako.lookup.TemplateLookup(directories=[templates_path])


CLASS_NAMES = {
    librpc.TypeClass.STRUCT: 'struct',
    librpc.TypeClass.UNION: 'union',
    librpc.TypeClass.ENUM: 'enum',
    librpc.TypeClass.TYPEDEF: 'type',
    librpc.TypeClass.BUILTIN: 'builtin'
}


def generate_index(name, typing):
    entries = typing.types
    types = (t for t in entries if t.is_builtin)
    typedefs = (t for t in entries if t.is_typedef)
    structures = (t for t in entries if t.is_struct or t.is_union or t.is_enum)

    t = lookup.get_template('index.mako')
    return t.render(
        name=name,
        interfaces=sorted(typing.interfaces, key=lambda t: t.name),
        types=sorted(types, key=lambda t: t.name),
        typedefs=sorted(typedefs, key=lambda t: t.name),
        structs=sorted(structures, key=lambda t: t.name)
    )


def generate_interface(iface):
    methods = (m for m in iface.members if type(m) is librpc.Method)
    properties = (m for m in iface.members if type(m) is librpc.Property)
    events = []

    t = lookup.get_template('interface.mako')
    return t.render(
        iface=iface,
        methods=methods,
        properties=properties,
        events=events
    )


def generate_type(typ):
    t = lookup.get_template('type.mako')
    return t.render(t=typ)


def generate_file(outdir, name, contents):
    with open(os.path.join(outdir, name), 'w') as f:
        f.write(contents)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--name', metavar='NAME', help='Project name', required=True)
    parser.add_argument('-f', metavar='FILE', action='append', help='IDL file')
    parser.add_argument('-d', metavar='DIRECTORY', action='append', help='IDL directory')
    parser.add_argument('-o', metavar='DIRECTORY', help='Output directory', required=True)
    args = parser.parse_args()

    typing = librpc.Typing()
    outdir = args.o

    for f in args.f or []:
        typing.load_types(f)

    for d in args.d or []:
        for f in glob.iglob('{0}/**/*.yaml'.format(d), recursive=True):
            try:
                typing.load_types(f)
            except librpc.LibException as err:
                print('Processing {0} failed: {1}'.format(f, str(err)))
                continue

    if not os.path.exists(args.o):
        os.makedirs(args.o, exist_ok=True)

    # Copy the CSS file
    shutil.copy(os.path.join(curdir, 'assets/main.css'), outdir)

    for t in typing.types:
        generate_file(outdir, 'type-{0}.html'.format(t.name), generate_type(t))

    for i in typing.interfaces:
        generate_file(outdir, 'interface-{0}.html'.format(i.name), generate_interface(i))

    generate_file(outdir, 'index.html', generate_index(args.name, typing))


if __name__ == '__main__':
    main()
