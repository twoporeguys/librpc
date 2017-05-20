fetch:
	npm install
	npm install -g browserify

install:
	npm install --unsafe-perm -g .
	NODE_PATH=node_modules browserify --standalone middleware -r librpc-client -o module.js

clean:
	rm -rf lib node_modules
