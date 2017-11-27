cmake_minimum_required(VERSION 2.8)
project(librpc)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

set(PYTHON_VERSION python3 CACHE STRING "Target Python version for py-librpc")
option(RPC_DEBUG "Enable debug output")
option(BUNDLED_BLOCKS_RUNTIME "Use bundled BlocksRuntime")
option(BUILD_DOC "Create and install HTML API documentation")
option(BUILD_TESTS "Build and install tests")
option(BUILD_EXAMPLES "Build and install example programs" ON)
option(BUILD_PYTHON "Build and install Python extension" ON)
option(BUILD_JSON "Build and install JSON serializer" ON)
option(BUILD_LIBUSB "Build and install libusb transport")
if(LINUX)
    option(BUILD_BUS "Build and install bus transport" ON)
    option(BUILD_KMOD "Build and install kmod" ON)
endif()

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CPACK_PACKAGE_NAME librpc)
set(CPACK_PACKAGE_VENDOR Two Pore Guys)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 1)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER Jakub Klama <jakub.klama@twoporeguys.com>)
set(CPACK_GENERATOR DEB)
execute_process(
        COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE CPACK_PACKAGE_VERSION_PATCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
include(CPack)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GLIB REQUIRED glib-2.0>=2.40)
pkg_check_modules(SOUP REQUIRED libsoup-2.4)
pkg_check_modules(YAML REQUIRED yaml-0.1)

if(BUILD_JSON)
    pkg_check_modules(YAJL REQUIRED yajl>=2.0.0)
endif()

if(UNIX)
    pkg_check_modules(GIO REQUIRED gio-unix-2.0)
endif()

if(BUILD_LIBUSB)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
endif()

if(BUILD_BUS AND LINUX)
    pkg_check_modules(UDEV REQUIRED libudev)
endif()

if(BUNDLED_BLOCKS_RUNTIME)
    include_directories(contrib/BlocksRuntime)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-fblocks -Wall -Wextra -DRPC_TRACE")
set(CMAKE_CXX_FLAGS "-fblocks -Wall -Wextra -DRPC_TRACE")
include_directories(include)
include_directories(/usr/local/include)
include_directories (${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})
include_directories(${GIO_INCLUDE_DIRS})
link_directories(${GIO_LIBRARY_DIRS})
include_directories(${SOUP_INCLUDE_DIRS})
link_directories(${SOUP_LIBRARY_DIRS})
include_directories(${YAML_INCLUDE_DIRS})
link_directories(${YAML_LIBRARY_DIRS})

if(RPC_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRPC_DEBUG")
endif()

if(BUILD_JSON)
    include_directories(${YAJL_INCLUDE_DIRS})
    link_directories(${YAJL_LIBRARY_DIRS})
endif()

if(BUILD_LIBUSB)
    include_directories(${LIBUSB_INCLUDE_DIRS})
    link_directories(${LIBUSB_LIBRARY_DIRS})
endif()

set(HEADERS
        include/rpc/object.h
        include/rpc/connection.h
        include/rpc/server.h
        include/rpc/client.h
        include/rpc/service.h
        include/rpc/discovery.h
        include/rpc/query.h
        include/rpc/bus.h
        include/rpc/serializer.h
        include/rpc/typing.h)

set(CORE_FILES
        src/rpc_connection.c
        src/rpc_object.c
        src/rpc_server.c
        src/rpc_service.c
        src/rpc_client.c
        src/rpc_discovery.c
        src/rpc_query.c
        src/rpc_bus.c
        src/rpc_serializer.c
        src/rpc_typing.c
        src/utils.c
        src/internal.h
        src/linker_set.h
        src/memfd.h)

set(TRANSPORT_FILES
        src/transport/socket.c
        src/transport/ws.c
        src/transport/loopback.c)

set(SERIALIZER_FILES
        src/serializer/msgpack.c
        src/serializer/msgpack.h
        src/serializer/yaml.c
        src/serializer/yaml.h
        contrib/mpack/mpack.c)

set(TYPE_CLASS_FILES
        src/class/struct.c
        src/class/union.c
        src/class/typedef.c
        src/class/enum.c
        src/class/builtin.c)

set(VALIDATOR_FILES
        src/validator/string_length.c
        src/validator/string_regex.c
        src/validator/int64_range.c)

if(BUILD_JSON)
    set(SERIALIZER_FILES
            ${SERIALIZER_FILES}
            src/serializer/json.c
            src/serializer/json.h)
endif()

if(BUNDLED_BLOCKS_RUNTIME)
    set(CORE_FILES ${CORE_FILES}
            contrib/BlocksRuntime/data.c
            contrib/BlocksRuntime/runtime.c)
endif()

if(BUILD_LIBUSB)
    set(TRANSPORT_FILES ${TRANSPORT_FILES} src/transport/libusb.c)
endif()

if(BUILD_BUS AND LINUX)
    set(TRANSPORT_FILES ${TRANSPORT_FILES} src/transport/bus.c)
endif()

add_library(librpc SHARED
        ${HEADERS}
        ${TRANSPORT_FILES}
        ${SERIALIZER_FILES}
        ${TYPE_CLASS_FILES}
        ${VALIDATOR_FILES}
        ${CORE_FILES})

set_target_properties(librpc PROPERTIES PREFIX "")
#set_target_properties(librpc PROPERTIES MACOSX_RPATH ON)
target_link_libraries(librpc ${GLIB_LIBRARIES})
target_link_libraries(librpc ${GIO_LIBRARIES})
target_link_libraries(librpc ${SOUP_LIBRARIES})
target_link_libraries(librpc ${YAJL_LIBRARIES})
target_link_libraries(librpc ${YAML_LIBRARIES})

if(BUILD_LIBUSB)
    target_link_libraries(librpc ${LIBUSB_LIBRARIES})
endif()

if(BUILD_BUS AND LINUX)
    target_link_libraries(librpc ${UDEV_LIBRARIES})
endif()

if(NOT APPLE)
    target_link_libraries(librpc BlocksRuntime)
endif()

configure_file(librpc.pc.in ${CMAKE_CURRENT_BINARY_DIR}/librpc.pc @ONLY)
install(TARGETS librpc DESTINATION lib)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/librpc.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pkgconfig)

if(BUILD_TESTS)
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
            tests/method_stress.cpp
            tests/api/query.cpp
            tests/serializer/yaml.cpp)

    add_executable(catch_tests ${TEST_SOURCES})
    target_link_libraries(catch_tests Catch)
    target_link_libraries(catch_tests librpc)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples/client)
    add_subdirectory(examples/discovery)
    add_subdirectory(examples/event)
    add_subdirectory(examples/loopback)
    add_subdirectory(examples/pack-unpack)
    add_subdirectory(examples/server)
    add_subdirectory(examples/error-backtrace)
    add_subdirectory(examples/query)
    if(UNIX)
    add_subdirectory(examples/fd-client)
    add_subdirectory(examples/fd-server)
    endif()
    if(LINUX)
        add_subdirectory(examples/shm-client)
        add_subdirectory(examples/shm-server)
    endif()
    if (BUILD_BUS OR BUILD_LIBUSB)
        add_subdirectory(examples/bus)
        add_subdirectory(examples/log-reader)
    endif()
endif()

if(BUILD_PYTHON)
    add_subdirectory(python)
endif()

if(BUILD_DOC)
    find_package(Doxygen)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
    configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc ALL
            ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            COMMAND
                env BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR} sphinx-build
                ${CMAKE_SOURCE_DIR}/docs/source ${CMAKE_CURRENT_BINARY_DIR}/docs
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
endif()

if(BUILD_BUS AND BUILD_KMOD)
    add_subdirectory(kmod)
endif()

# uninstall target
if(NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
