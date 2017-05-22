fetch:
	npm install
	npm install -g browserify

build:
	npm config set prefix ${STAGEDIR_IN_CHROOT}${PREFIX}
	npm install --unsafe-perm -g .

install:
	NODE_PATH=${STAGEDIR_IN_CHROOT}${PREFIX}/lib/node_modules browserify --standalone librpc -r librpc-client -o ${BROWSERIFY_BUNDLE}

clean:
	rm -rf lib node_modules
