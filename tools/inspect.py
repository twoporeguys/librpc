#!/usr/bin/env python3

import argparse
import librpc

def instance(i):
    print('Instance {0}'.format(i.path))
    print('Interfaces:')

    for name, iface in i.interfaces.items():
        interface(name, iface)

def interface(name, iface):
    print('    Interface {0}'.format(name))
    print('    Methods:')
    if iface.methods:
        for i in iface.methods:
            print('        - {0}'.format(i))
    else:
        print('        <none>')

    print('    Properties:')
    if iface.properties:
        for i in iface.properties:
            print('        - {0}'.format(i))
    else:
        print('        <none>')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uri')
    args = parser.parse_args()

    client = librpc.Client()
    client.connect(args.uri)
    client.unpack = True

    for i in client.instances.values():
        instance(i)


if __name__ == '__main__':
    main()
