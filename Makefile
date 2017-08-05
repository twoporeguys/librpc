.PHONY: all

all:
	mkdir -p build && \
	cd build && \
	cmake .. && \
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
