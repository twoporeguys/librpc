# librpc

A general-purpose IPC/RPC library supporting asynchronous notifications,
data streaming, exchange of file descriptors and WebSockets endpoint.
Loosely based on Apple XPC interface.

## Building librpc

Following packages are required to build libprc (using Ubuntu 17.04 as an example, please adjust do your distribution if needed):

```
cmake
clang
libglib2.0-dev
libsoup-gnome2.4-dev
libyajl-dev
libblocksruntime-dev
```

Build/install procedure:

```
$ mkdir -p build
$ cd build
$ cmake ..
$ make
$ make install  # optional
```

Please note that the only supported compiler is clang.