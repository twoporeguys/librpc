fetch:
	pip install Cython

install:
	CFLAGS="-I/home/shangteus/librpc/include/"  LDFLAGS="-L/home/shangteus/librpc/" python3.6 setup.py install --home=${STAGEDIR_IN_CHROOT}${PREFIX}

clean:
	rm -rf lib node_modules
