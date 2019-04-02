export CC := clang
export CXX := clang++
PYTHON_VERSION ?= python3
PREFIX ?= /usr/local
BUILD_CPLUSPLUS ?= OFF
BUILD_PYTHON ?= ON
BUILD_TESTS ?= OFF
BUILD_CLIENT ?= OFF
RPC_DEBUG ?= OFF
BUILD_TYPE ?= Release
BUILD_XPC ?= OFF
ENABLE_LIBDISPATCH ?= OFF
ENABLE_RPATH ?= ON
BUILD_DOC ?= OFF

.PHONY: all clean bootstrap build build-cov install uninstall test

all: build

build:
	mkdir -p build
	cd build && cmake .. \
	    -DBUILD_LIBUSB=ON \
	    -DPYTHON_VERSION=$(PYTHON_VERSION) \
	    -DRPC_DEBUG=$(RPC_DEBUG) \
	    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	    -DCMAKE_INSTALL_PREFIX=$(PREFIX) \
	    -DBUILD_CLIENT=$(BUILD_CLIENT) \
	    -DBUILD_PYTHON=$(BUILD_PYTHON) \
	    -DBUILD_TESTS=$(BUILD_TESTS) \
	    -DBUILD_CPLUSPLUS=$(BUILD_CPLUSPLUS) \
	    -DBUILD_XPC=$(BUILD_XPC) \
	    -DENABLE_LIBDISPATCH=$(ENABLE_LIBDISPATCH) \
	    -DENABLE_RPATH=$(ENABLE_RPATH) \
	    -DBUILD_DOC=$(BUILD_DOC)
	make -C build

build-arm:
	mkdir -p build
	cd build && cmake .. \
	    -DBUILD_LIBUSB=OFF \
	    -DPYTHON_VERSION=$(PYTHON_VERSION) \
	    -DRPC_DEBUG=$(RPC_DEBUG) \
	    -DCMAKE_BUILD_TYPE=Debug \
	    -DCMAKE_INSTALL_PREFIX=$(PREFIX) \
	    -DBUILD_CLIENT=$(BUILD_CLIENT) \
	    -DBUILD_PYTHON=OFF \
	    -DBUILD_TESTS=ON \
	    -DBUILD_CPLUSPLUS=$(BUILD_CPLUSPLUS) \
	    -DENABLE_RPATH=$(ENABLE_RPATH) \
	    -DBUILD_DOC=$(BUILD_DOC)
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
	    -DBUILD_DOC=$(BUILD_DOC)
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
	gtester --keep-going -o test-results.xml ./build-cov/test_suite || true
	lcov \
	    --capture \
	    --gcov-tool $(abspath llvm-gcov.sh) \
	    --directory build-cov \
	    -o librpc.cov
	genhtml librpc.cov -o coverage-report
	xsltproc -o junit-test-results.xml gtester.xsl test-results.xml

benchmark:
	mkdir -p build/benchmarks
	cd build/benchmarks && cmake ../../tests/benchmarks
	cd build/benchmarks && make
	cd build/benchmarks && ../../tests/benchmarks/run.py -o ../../output
