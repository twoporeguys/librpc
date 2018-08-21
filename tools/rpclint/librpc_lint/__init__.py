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
import mako.lookup
import librpc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--system', action='store_true', help='Use system types')
    parser.add_argument('-f', metavar='FILE', action='append', help='IDL file')
    parser.add_argument('-d', metavar='DIRECTORY', action='append', help='IDL directory')
    args = parser.parse_args()

    typing = librpc.Typing()
    paths = []

    if args.system:
        for p in ('/usr/share/idl', '/usr/local/share/idl'):
            for f in glob.iglob('{0}/*.yaml'.format(p)):
                typing.read_file(f)
                paths.append(f)

    for f in args.f or []:
        typing.read_file(f)
        paths.append(f)

    for d in args.d or []:
        for f in glob.iglob('{0}/**/*.yaml'.format(d), recursive=True):
            try:
                typing.read_file(f)
                paths.append(f)
            except librpc.LibException as err:
                print('Processing {0} failed: {1}'.format(f, str(err)))
                continue

    for p in paths:
        typing.load_types(p)

    for typ in typing.types:
        if not typ.description:
            print('{0}: description missing'.format(typ.name))

        for member in typ.members:
            if not member.description:
                print('{0}: description missing for member "{1}"'.format(
                    typ.name,
                    member.name
                ))


if __name__ == '__main__':
    main()
