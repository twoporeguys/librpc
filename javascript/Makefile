build:
	npm install
	npm install --prefix=${PREFIX}/lib --unsafe-perm -g .

install:
	NODE_PATH=${PREFIX}/lib/node_modules browserify --standalone librpc -r librpc-client -o ${BROWSERIFY_BUNDLE}

clean:
	rm -rf lib node_modules
