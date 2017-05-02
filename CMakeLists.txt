cmake_minimum_required(VERSION 3.6)
project(librpc)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GLIB REQUIRED glib-2.0)
pkg_check_modules (GIO REQUIRED gio-unix-2.0)
pkg_check_modules(SOUP REQUIRED libsoup-2.4)

set(CMAKE_C_STANDARD 11)
include_directories(include)
include_directories (${GLIB_INCLUDE_DIRS})
link_directories (${GLIB_LIBRARY_DIRS})
include_directories (${GIO_INCLUDE_DIRS})
link_directories (${GIO_LIBRARY_DIRS})
include_directories (${SOUP_INCLUDE_DIRS})
link_directories (${SOUP_LIBRARY_DIRS})


set(CORE_FILES
        src/rpc_connection.c
        src/rpc_object.c
        src/rpc_server.c
        src/internal.h
        src/utils.c
        include/rpc/service.h
        src/rpc_service.c src/linker_set.h)

set(TRANSPORT_FILES
        src/transport/socket.c
        src/transport/ws.c)

set(SERIALIZER_FILES
        src/serializer/msgpack.c
        src/serializer/msgpack.c)

add_library(librpc SHARED ${CORE_FILES} ${TRANSPORT_FILES} ${SERIALIZER_FILES})