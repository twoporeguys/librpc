#!/bin/bash

case "`uname -s`" in
    Darwin*)
        echo "I am a Mac.  Brew installing stuff."
        echo "Don't worry about warnings of things already installed."
        brew install glib libsoup yajl libusb libyaml yajl python3 cmake
        pip3 install cython
        ;;
    Linux*)
        echo "I am some kind of Linux, hopefully Ubuntu."
        apt-get -y install \
            cmake clang libglib2.0-dev libsoup-gnome2.4-dev \
            libyajl-dev libblocksruntime-dev libyaml-dev libudev-dev \
            libusb-1.0-0-dev python3 cython3 python3-sphinx
        ;;
    default)
        echo "Don't know how to set up for your OS."
        exit 1
esac
