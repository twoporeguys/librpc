#!/bin/bash

case "`uname -s`" in
    Darwin*)
    if [ -x /opt/local/bin/port ]; then
        echo "I am a Mac.  Using macports to install stuff with sudo"
        sudo port install glib2 glib-networking libsoup yajl libusb libyaml yajl python36 py36-pip cmake lcov pkgconfig py36-gobject3 doxygen py36-cython
    elif [ -x /usr/local/bin/brew ]; then
        echo "I am a Mac.  Brew installing stuff."
        echo "Don't worry about warnings of things already installed."
        brew install glib libsoup yajl libusb libyaml yajl python3 cmake pkg-config gtk+3
        # pip3 is install by default with homebrew's python formula
        pip3 install cython
    else
        echo "I am a Mac but you have neither macports or brew installed."
        exit 1
    fi
    ;;
    Linux*)
        echo "I am some kind of Linux, hopefully Ubuntu."
        apt-get -y install \
            cmake clang libglib2.0-dev libsoup-gnome2.4-dev \
            libyajl-dev libblocksruntime-dev libyaml-dev libudev-dev \
            libusb-1.0-0-dev python3 cython3 python3-sphinx \
	    libelf-dev python3-setuptools linux-headers-generic \
	    libsystemd-dev systemd doxygen nodejs npm lcov llvm \
	    xsltproc
        ;;
    default)
        echo "Don't know how to set up for your OS."
        exit 1
esac
