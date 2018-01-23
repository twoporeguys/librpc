VERSION ?= patch

export PATH := ./node_modules/.bin:${PATH}

package: build _package

build: _clean _build

clean: _clean

test: _test

deps: _deps

stats: _stats

publish: build _version _package _publish

# Real commands
_clean:
	rm -rf ./dist ./package ./test-result.xml ./stats.json 

_deps:
	npm install

_test: _deps
_test:
	nyc --reporter=none mocha --opts mocha.opts > test-result.xml
	nyc report

_build: _deps
	webpack --progress --profile -p

_copy_assets:
	cp -rf ./src/ ./package.json .npmignore ./dist/

_package: _deps _copy_assets
	mkdir package
	cd package; npm pack ../dist/

_version: _deps
	npm version $(VERSION)

_publish: _deps
	npm publish ./dist/

_stats:
	webpack -p --json > stats.json
