cmake_minimum_required(VERSION 3.2)
project(librpc)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CPACK_PACKAGE_NAME librpc)
set(CPACK_PACKAGE_VENDOR Two Pore Guys)
set(CPACK_PACKAGE_VERSION_MAJOR 6)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER Jakub Klama <jakub.klama@twoporeguys.com>)
set(CPACK_GENERATOR DEB)
execute_process(COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE CPACK_PACKAGE_VERSION_PATCH)
include(CPack)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GLIB REQUIRED glib-2.0>=2.52)
pkg_check_modules (GIO REQUIRED gio-unix-2.0)
pkg_check_modules(SOUP REQUIRED libsoup-2.4)
pkg_check_modules(YAJL REQUIRED yajl>=2.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-fblocks -Wall -Wextra")
set(CMAKE_CXX_FLAGS "-fblocks -Wall -Wextra")
include_directories(include)
include_directories (${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})
include_directories(${GIO_INCLUDE_DIRS})
link_directories(${GIO_LIBRARY_DIRS})
include_directories(${SOUP_INCLUDE_DIRS})
link_directories(${SOUP_LIBRARY_DIRS})
include_directories(${YAJL_INCLUDE_DIRS})
link_directories(${YAJL_LIBRARY_DIRS})

set(HEADERS
        include/rpc/service.h
        include/rpc/object.h
        include/rpc/connection.h
        include/rpc/server.h
        include/rpc/client.h
        include/rpc/discovery.h)

set(CORE_FILES
        src/rpc_connection.c
        src/rpc_object.c
        src/rpc_server.c
        src/rpc_service.c
        src/rpc_client.c
        src/rpc_discovery.c
        src/utils.c
        src/internal.h
        src/linker_set.h)

set(TRANSPORT_FILES
        src/transport/socket.c
        src/transport/ws.c
        src/transport/loopback.c)

set(SERIALIZER_FILES
        src/serializer/msgpack.c
        src/serializer/msgpack.h
        src/serializer/json.c
        src/serializer/json.h
        contrib/mpack/mpack.c)

add_library(librpc SHARED
        ${HEADERS}
        ${TRANSPORT_FILES}
        ${SERIALIZER_FILES}
        ${CORE_FILES})

set_target_properties(librpc PROPERTIES PREFIX "")
target_link_libraries(librpc ${GLIB_LIBRARIES})
target_link_libraries(librpc ${GIO_LIBRARIES})
target_link_libraries(librpc ${SOUP_LIBRARIES})
target_link_libraries(librpc ${YAJL_LIBRARIES})
if(NOT APPLE)
    target_link_libraries(librpc BlocksRuntime)
endif()

configure_file(librpc.pc.in ${CMAKE_CURRENT_BINARY_DIR}/librpc.pc @ONLY)

install(TARGETS librpc
        LIBRARY DESTINATION lib)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/librpc.pc
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pkgconfig)

# Compile tests
set(CATCH_INCLUDE_DIR src/tests)
add_library(Catch INTERFACE)

set(TEST_SOURCES
        tests/tests_main.cpp
        tests/serializer/json.cpp
        tests/api/object.cpp
        tests/api/client.cpp
        tests/api/connection.cpp
        tests/api/discovery.cpp
        tests/api/server.cpp
        tests/api/service.cpp
        tests/serializer/msgpack.cpp
        tests/transport/loopback.cpp
        tests/transport/socket.cpp
        tests/transport/ws.cpp
        tests/internal.cpp
        tests/method_stress.cpp)

add_executable(catch_tests ${TEST_SOURCES})
target_link_libraries(catch_tests Catch)
target_link_libraries(catch_tests librpc)

add_subdirectory(examples/client)
add_subdirectory(examples/discovery)
add_subdirectory(examples/event)
add_subdirectory(examples/loopback)
add_subdirectory(examples/pack-unpack)
add_subdirectory(examples/server)
