.PHONY: all clean bootstrap install uninstall

export CC := clang
export CXX := clang++
PYTHON_VERSION := python3
PREFIX ?= /usr/local
BUILD_PYTHON ?= ON
BUILD_CPLUSPLUS ?= OFF
BUILD_CLIENT ?= OFF
BUILD_TYPE ?= Release
BUILD_XPC ?= OFF
ENABLE_LIBDISPATCH ?= OFF

.PHONY: build build-cov bootstrap clean install uninstall test

all: build

build:
	mkdir -p build
	cd build && cmake .. \
	    -DBUILD_LIBUSB=ON \
	    -DPYTHON_VERSION=$(PYTHON_VERSION) \
	    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	    -DCMAKE_INSTALL_PREFIX=$(PREFIX) \
	    -DBUILD_CLIENT=$(BUILD_CLIENT) \
	    -DBUILD_PYTHON=$(BUILD_PYTHON) \
	    -DBUILD_CPLUSPLUS=$(BUILD_CPLUSPLUS) \
	    -DBUILD_XPC=$(BUILD_XPC) \
	    -DENABLE_LIBDISPATCH=$(ENABLE_LIBDISPATCH)
	make -C build

build-cov:
	mkdir -p build-cov
	cd build-cov && cmake .. \
	    -DBUILD_LIBUSB=ON \
	    -DPYTHON_VERSION=python3 \
	    -DCMAKE_BUILD_TYPE=Debug \
	    -DBUILD_CLIENT=OFF \
	    -DBUILD_PYTHON=ON \
	    -DBUILD_TESTS=ON \
	    -DENABLE_LIBDISPATCH=OFF \
	    -DBUILD_XPC=$(BUILD_XPC) \
	    -DENABLE_COVERAGE=ON
	make -C build-cov

bootstrap:
	sh requirements.sh

clean:
	rm -rf *~ build build-cov

install:
	make -C build install
	@if [ "`uname -s`" = "Linux" ]; then ldconfig || true; fi

uninstall:
	make -C build uninstall

test: build-cov
	./build-cov/test_suite
	lcov --capture --directory build-cov -o librpc.cov
	genhtml librpc.cov -o coverage-report

benchmark:
	mkdir -p build/benchmarks
	cd build/benchmarks && cmake ../../tests/benchmarks
	cd build/benchmarks && make
	cd build/benchmarks && ../../tests/benchmarks/run.py
