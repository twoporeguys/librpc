fetch:
	npm install
	npm install -g browserify

install:
	npm config set prefix ${STAGEDIR_IN_CHROOT}${PREFIX}
	npm install --unsafe-perm -g .
	NODE_PATH=${STAGEDIR_IN_CHROOT}${PREFIX}/lib/node_modules browserify --standalone librpc -r librpc-client -o ${BROWSERIFY_BUNDLE}

clean:
	rm -rf lib node_modules
