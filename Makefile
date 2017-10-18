.PHONY: all

PYTHON_VERSION := "python3"

all:
	mkdir -p build && \
	cd build && \
	cmake .. -DBUILD_LIBUSB=ON -DPYTHON_VERSION=${PYTHON_VERSION} && \
	make

.PHONY: clean
clean:
	rm -rf *~ build

PREFIX = /usr/local

.PHONY: install
install:
	make -C build install

.PHONY: uninstall
uninstall:
	make -C build uninstall
