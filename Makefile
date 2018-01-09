.PHONY: all clean bootstrap install uninstall

export CC := clang
export CXX := clang++
PYTHON_VERSION := python3
INSTALL_PREFIX := /usr/local
PREFIX := /usr/local
BUILD_PYTHON ?= ON
BUILD_CLIENT ?= OFF
ENABLE_LIBDISPATCH ?= OFF

all:
	mkdir -p build
	cd build && cmake .. -DBUILD_LIBUSB=ON \
        -DPYTHON_VERSION=$(PYTHON_VERSION) \
        -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) \
        -DBUILD_CLIENT=$(BUILD_CLIENT) \
        -DBUILD_PYTHON=$(BUILD_PYTHON) \
	-DENABLE_LIBDISPATCH=$(ENABLE_LIBDISPATCH)
	make -C build

bootstrap:
	sh requirements.sh

clean:
	rm -rf *~ build

install:
	make -C build install
	@if [ "`uname -s`" = "Linux" ]; then ldconfig || true; fi

uninstall:
	make -C build uninstall
