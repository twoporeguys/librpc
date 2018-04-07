Building
========

Setting up development environment
----------------------------------
librpc uses the `CMake <http://www.cmake.org>`_ build system. Currently it
runs on Linux, FreeBSD and macOS systems.

There is a makefile that wraps most of the typical CMake usage scenarios,
though. Running ``make bootstrap`` from the top level directory of the source
checkout should be enough to set up a development environment.

Compile-time configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~
Following compile-time parameters can be passed to CMake:

+==============
| Name
+----------------------------+---------+
|``RPC_DEBUG``               | ``OFF`` |
+----------------------------+---------+
| ``BUNDLED_BLOCKS_RUNTIME`` | ``OFF`` | Use bundled version of
|                            |         | libBlocksRuntime         |
+----------------------------+---------+---------------------------
| ``BUILD_DOC``              | ``OFF`` | Build documentation
+----------------------------+---------+
| ``BUILD_TESTS``            | ``ON``  | Build test suite
+----------------------------+---------+
| ``BUILD_EXAMPLES``         | ``ON``  | Build example programs
+----------------------------+---------+
| ``BUILD_PYTHON`` (``ON``/``OFF``)
+----------------------------+---------+
| ``PYTHON_VERSION`` (string)
+----------------------------+---------+
| ``BUILD_CPLUSPLUS`` (``ON``/``OFF``)
+----------------------------+---------+
| ``BUILD_JSON`` (``ON``/``OFF``)
+----------------------------+---------+
| ``BUILD_LIBUSB`` (``ON``/``OFF``)
+----------------------------+---------+
| ``BUILD_RPCTOOL`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_UBSAN`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_UBSAN_NULLABILITY`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_ASAN`` (``ON``/``OFF``)
+----------------------------+---------+
| ``ENABLE_LIBDISPATCH`` (``ON``/``OFF``)
+----------------------------+---------+

