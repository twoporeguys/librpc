set -e
mkdir docs-build && cd docs-build
git clone -b gh-pages https://github.com/twoporeguys/librpc.git && cd librpc
git config --global push.default simple
git config user.name "Travis CI"
git config user.email "travis@travis-ci.org"
rm -rf *
sed "s/@CMAKE_SOURCE_DIR@/${TRAVIS_BUIL_DIR}/g" ${DOXYFILE_IN} > Doxyfile
doxygen Doxyfile

if [ -d "html" ] && [ -f "html/index.html" ]; then
	git add --all
	git commit -m "Deploy code docs to GitHub Pages (build ${TRAVIS_BUILD_NUMBER})" -m "Commit: ${TRAVIS_COMMIT}"
	git push --force https://${GH_REPO_TOKEN}@github.com/twoporeguys/librpc.git > /dev/null 2>&1
else
	echo '' >&2
	echo 'Warning: No documentation (html) files have been found!' >&2
	echo 'Warning: Not going to push the documentation to GitHub!' >&2
	exit 1
fi
