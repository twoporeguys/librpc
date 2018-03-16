VERSION ?= patch

TOOLS_PATH := ./node_modules/.bin

package: build _package

build: _clean _build

clean: _clean

test: _test

doc: _clean_doc node_modules _doc

deps: node_modules

stats: _stats

publish: build _version _package _publish

# Real commands
_clean: _clean_build _clean_package _clean_test _clean_doc _clean_stats _clean_deps

_clean_build:
	rm -rf ./dist

_clean_package:
	rm -rf ./package

_clean_test:
	rm -rf ./test-result.xml

_clean_stats:
	rm -rf ./stats.json

_clean_doc:
	rm -rf ./doc

_clean_deps:
	rm -rf ./node_modules

node_modules:
	npm install

_test: node_modules
_test:
	$(TOOLS_PATH)/nyc --reporter=none $(TOOLS_PATH)/mocha --opts mocha.opts > test-result.xml
	$(TOOLS_PATH)/nyc report

_doc:
	$(TOOLS_PATH)/typedoc --out ./doc --entryPoint 'LibRpcClient' src/index.ts

_build: node_modules
	$(TOOLS_PATH)/webpack --progress --profile -p

_copy_assets:
	cp -rf ./src/ ./package.json .npmignore ./dist/

_package: node_modules _copy_assets
	mkdir package
	cd package; npm pack ../dist/

_version: node_modules
	npm version $(VERSION)

_publish: node_modules
	npm publish ./dist/

_stats:
	$(TOOLS_PATH)/webpack -p --json > stats.json
