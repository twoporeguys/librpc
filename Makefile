.PHONY: all clean bootstrap install uninstall

export CC := clang
export CXX := clang++
BUILD_PYTHON := ON
PYTHON_VERSION := python3
INSTALL_PREFIX := /usr/local
PREFIX := /usr/local
BUILD_CLIENT := OFF

all:
	mkdir -p build
	cd build && cmake .. -DBUILD_LIBUSB=ON \
        -DPYTHON_VERSION=${PYTHON_VERSION} \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} \
        -DBUILD_CLIENT=${BUILD_CLIENT} \
        -DBUILD_PYTHON=${BUILD_PYTHON}
	make -C build

bootstrap:
	sh requirements.sh

clean:
	rm -rf *~ build

install:
	make -C build install
	ldconfig || true

uninstall:
	make -C build uninstall
