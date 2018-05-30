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
import librpc
import signal
import time
import errno


@librpc.interface('com.twoporeguys.librpc.ExampleServer')
class ExampleServer(librpc.Service):
    def __init__(self):
        super(ExampleServer, self).__init__('/example', 'Example service')
        self.__name = "haha"

    @librpc.prop
    def name(self):
        return self.__name

    @name.setter
    def setname(self, value):
        print('Setting name to {0}'.format(value))
        self.__name = value

    @librpc.method
    def hello(self, string):
        return 'Hello {0}'.format(string)

    @librpc.method
    def adder(self, n1, n2):
        print('adder called with args: {0}, {1}'.format(n1, n2))
        return n1 + n2

    @librpc.method
    def subtracter(self, n1, n2):
        print('subtracter called with args: {0}, {1}'.format(n1, n2))
        return n1 - n2

    @librpc.method
    def streamer(self):
        for i in range(0, 10):
            time.sleep(0.1)
            yield {'index': i}

    @librpc.method
    def error(self):
        raise librpc.RpcException(errno.EPERM, 'Not allowed here')

    @librpc.method
    def slacker(self):
        time.sleep(30)
        return "Oh hey.. You're still here?"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--uri', help='Server URI')
    args = parser.parse_args()
    t = librpc.Typing()
    context = librpc.Context()
    server = librpc.Server(args.uri, context)

    context.register_instance(ExampleServer())

    def sigint(signo, frame):
        server.close()

    signal.signal(signal.SIGINT, sigint)
    server.resume()
    signal.pause()


if __name__ == '__main__':
    main()
