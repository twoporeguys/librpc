#!/bin/bash

case $OSTYPE in
    darwin*)
        echo "I am a Mac.  Brew installing stuff."
        echo "Don't worry about warnings of things already installed."
        brew install --upgrade glib
        brew install libsoup yajl
        ;;
    *linux*)
        echo "I am some kind of linux."
        sudo apt install cmake clang libglib2.0-dev libsoup-gnome2.4-dev libyajl-dev libblocksruntime-dev
        ;;
    default)
        echo "Don't know how to set up for your OS."
        exit 1
esac
