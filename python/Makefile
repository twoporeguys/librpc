PYTHONPATH = ${STAGEDIR_IN_CHROOT}${PYTHONPATH}

install:
	CC=clang python3.6 setup.py install --home=${STAGEDIR_IN_CHROOT}${PREFIX}
