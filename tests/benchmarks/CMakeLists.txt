cmake_minimum_required(VERSION 2.8)
project(librpc-benchmarks)

set(CMAKE_C_FLAGS "-fblocks -Wall -Wextra -Wno-unused-parameter -fno-omit-frame-pointer")

find_package (PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(LIBRPC REQUIRED librpc)
pkg_check_modules(DBUS REQUIRED dbus-1)

include_directories(${LIBRPC_INCLUDE_DIRS})
include_directories(${DBUS_INCLUDE_DIRS})
link_directories(${LIBRPC_LIBRARY_DIRS})
link_directories(${DBUS_LIBRARY_DIRS})

add_executable(librpc-server librpc-server.c)
target_link_libraries(librpc-server ${LIBRPC_LIBRARIES})
target_link_libraries(librpc-server BlocksRuntime)

add_executable(librpc-client librpc-client.c)
target_link_libraries(librpc-client ${LIBRPC_LIBRARIES})
target_link_libraries(librpc-client BlocksRuntime)

add_executable(dbus-server dbus-server.c)
target_link_libraries(dbus-server ${DBUS_LIBRARIES})

add_executable(dbus-client dbus-client.c)
target_link_libraries(dbus-client ${DBUS_LIBRARIES})
