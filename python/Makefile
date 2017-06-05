PYTHONPATH = ${STAGEDIR_IN_CHROOT}${PYTHONPATH}

install:
	CC=clang python3 setup.py install --home=${STAGEDIR_IN_CHROOT}${PREFIX}
