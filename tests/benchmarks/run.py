#!/usr/bin/env python3
#
# Copyright 2018 Two Pore Guys, Inc.
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
import time
import subprocess
import psutil
import matplotlib.pyplot as plt


SOCKET_PATH = 'unix:///tmp/benchmark.sock'
MESSAGE_SIZES = [2**n for n in range(4, 22)]


def parse_result(result):
    return {k: float(v) for k, v in (i.split('=') for i in result.split())}


def librpc_step(msgsize, cycles):
    server = subprocess.Popen(
        [
            './librpc-server',
            '-u', SOCKET_PATH,
            '-s', str(msgsize)
        ],
        stdout=subprocess.DEVNULL
    )

    time.sleep(1)
    client = subprocess.Popen(
        [
            './librpc-client',
            '-u', SOCKET_PATH,
            '-q', '-c', str(cycles)
        ],
        stdout=subprocess.PIPE
    )

    result, _ = client.communicate()
    client.wait()
    server.terminate()
    server.wait()
    return result.decode('utf-8').strip()


def librpc_shmem_step(msgsize, cycles):
    server = subprocess.Popen(
        [
            './librpc-server',
            '-u', SOCKET_PATH,
            '-s', str(msgsize), '-m'
        ],
        stdout=subprocess.DEVNULL
    )

    time.sleep(1)
    client = subprocess.Popen(
        [
            './librpc-client',
            '-u', SOCKET_PATH,
            '-q', '-c', str(cycles), '-m'
        ],
        stdout=subprocess.PIPE
    )

    result, _ = client.communicate()
    client.wait()
    server.terminate()
    server.wait()
    return result.decode('utf-8').strip()


def dbus_step(msgsize, cycles):
    server = subprocess.Popen(
        [
            './dbus-server',
            '-s', str(msgsize)
        ],
        stdout=subprocess.DEVNULL
    )

    time.sleep(1)
    client = subprocess.Popen(
        [
            './dbus-client',
            '-q', '-c', str(cycles)
        ],
        stdout=subprocess.PIPE
    )

    result, _ = client.communicate()
    client.wait()
    server.terminate()
    server.wait()
    return result.decode('utf-8').strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-o',
        '--output',
        metavar='DIR',
        help='Output directory',
        default='output'
    )

    parser.add_argument(
        '-c',
        '--cycles',
        metavar='N',
        help='Number of cycles',
        default=5000,
        type=int
    )

    librpc_results = []
    librpc_shmem_results = []
    dbus_results = []

    args = parser.parse_args()
    os.makedirs(args.output, exist_ok=True)

    for msgsize in MESSAGE_SIZES:
        print('Running for message size {0}'.format(msgsize))
        librpc_results.append(parse_result(librpc_step(msgsize, args.cycles)))
        librpc_shmem_results.append(parse_result(librpc_shmem_step(msgsize, args.cycles)))
        dbus_results.append(parse_result(dbus_step(msgsize, args.cycles)))

    print('Generating plots...')
    plt.figure(1)
    plt.plot(MESSAGE_SIZES, [i['bps'] for i in librpc_results], 'r', label='librpc')
    plt.plot(MESSAGE_SIZES, [i['bps'] for i in librpc_shmem_results], 'g', label='librpc-shmem')
    plt.plot(MESSAGE_SIZES, [i['bps'] for i in dbus_results], 'b', label='d-bus')

    plt.xscale('log', basex=2)
    plt.yscale('log', basey=2)
    plt.xlabel('Message size (bytes)')
    plt.ylabel('Bytes per second')
    plt.grid('on')
    lgd = plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
    plt.savefig(
        os.path.join(args.output, 'bytes_per_second.png'),
        bbox_extra_artists=(lgd,),
        bbox_inches='tight'
    )

    plt.figure(2)
    plt.plot(MESSAGE_SIZES, [i['pps'] for i in librpc_results], 'r', label='librpc')
    plt.plot(MESSAGE_SIZES, [i['pps'] for i in librpc_shmem_results], 'g', label='librpc-shmem')
    plt.plot(MESSAGE_SIZES, [i['pps'] for i in dbus_results], 'b', label='d-bus')

    plt.xscale('log', basex=2)
    plt.xlabel('Message size (bytes)')
    plt.ylabel('Packets per second')
    plt.grid('on')
    lgd = plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
    plt.savefig(
        os.path.join(args.output, 'packets_per_second.png'),
        bbox_extra_artists=(lgd,),
        bbox_inches='tight'
    )

    plt.figure(3)
    plt.plot(MESSAGE_SIZES, [i['lat'] for i in librpc_results], 'r', label='librpc')
    plt.plot(MESSAGE_SIZES, [i['lat'] for i in librpc_shmem_results], 'g', label='librpc-shmem')
    plt.plot(MESSAGE_SIZES, [i['lat'] for i in dbus_results], 'b', label='d-bus')

    plt.xscale('log', basex=2)
    plt.yscale('log')
    plt.xlabel('Message size (bytes)')
    plt.ylabel('Average latency (seconds)')
    plt.grid('on')
    lgd = plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
    plt.savefig(
        os.path.join(args.output, 'latency.png'),
        bbox_extra_artists=(lgd,),
        bbox_inches='tight'
    )

    print('Plots saved to "{0}" directory'.format(args.output))


if __name__ == '__main__':
    main()
