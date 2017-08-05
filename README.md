# librpc

A general-purpose IPC/RPC library supporting asynchronous notifications,
data streaming, exchange of file descriptors and WebSockets endpoint.
Loosely based on Apple XPC interface.

## Building librpc

Install the packages required to build librpc for Ubuntu 17.04 or Mac OS 12.
adjust do your distribution if needed):

```
./requirements.sh
```

Build/install procedure:

```
$ make
$ make install  # optional
```

Please note that the only supported compiler is clang.

## Documentation

On-line documentation is available at https://twoporeguys.github.io/librpc/html/
