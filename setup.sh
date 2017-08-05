#!/bin/bash

echo "Don't worry about warnings of things already installed."
brew install --upgrade glib
brew install libsoup yajl
brew install pygobject3 --with-python3 gtk+3
