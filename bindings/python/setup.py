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
import Cython.Compiler.Options
Cython.Compiler.Options.annotate = True

from distutils.core import setup
from Cython.Distutils.extension import Extension
from Cython.Distutils import build_ext


os.environ['CC'] = 'clang'
os.environ.setdefault('DESTDIR', '/')
cflags = ['-fblocks', '-Wno-sometimes-uninitialized']
ldflags = ['-lrpc']
systemd = os.environ.get('SYSTEMD_SUPPORT') == 'ON'


if os.environ.get('CMAKE_BUILD_TYPE') == 'Debug':
    cflags += ['-g', '-O0']

if 'CMAKE_SOURCE_DIR' in os.environ:
    cflags += [
        os.path.expandvars('-I${CMAKE_SOURCE_DIR}/include'),
        os.path.expandvars('-I../../include/')
    ]
    ldflags += [
        os.path.expandvars('-L../..'),
        os.path.expandvars('-Wl,-rpath'),
        os.path.expandvars('-Wl,${CMAKE_PREFIX}/lib')
    ]


setup(
    name='librpc',
    version='1.0',
    packages=[''],
    package_data={'': ['*.html', '*.c', 'librpc.pxd']},
    cmdclass={'build_ext': build_ext},
    ext_modules=[
        Extension(
            "librpc",
            ["librpc.pyx"],
            extra_compile_args=cflags,
            extra_link_args=ldflags,
            cython_compile_time_env={'SYSTEMD_SUPPORT': systemd},
            cython_directives={'language_level': 3}
        )
    ]
)
