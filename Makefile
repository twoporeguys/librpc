.PHONY: all

PYTHON_VERSION := "python3"
INSTALL_PREFIX := "/usr/local"

all:
	mkdir -p build && \
	cd build && \
	cmake .. -DBUILD_LIBUSB=ON -DPYTHON_VERSION=${PYTHON_VERSION} -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} && \
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
