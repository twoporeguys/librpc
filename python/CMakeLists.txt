add_custom_target(py-librpc COMMAND cd ${CMAKE_SOURCE_DIR}/python/ && CC=clang python3 setup.py install --prefix=${CMAKE_INSTALL_PREFIX})
