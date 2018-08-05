cmake_minimum_required(VERSION 2.8)
include(GNUInstallDirs)
include(CPack)
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
option(BUILD_CPLUSPLUS "Build and install C++ library")
option(BUILD_JSON "Build and install JSON serializer" ON)
option(BUILD_LIBUSB "Build and install libusb transport")
option(BUILD_XPC "Build and install XPC transport")
option(BUILD_RPCTOOL "Build and install rpctool" ON)
option(BUILD_RPCD "Build and install rpcd" ON)
option(BUILD_RPCDOC "Build and install rpcdoc" ON)
option(ENABLE_UBSAN "Enable undefined behavior sanitizer")
option(ENABLE_UBSAN_NULLABILITY "Enable nullability sanitizer")
option(ENABLE_ASAN "Enable address sanitizer")
option(ENABLE_LIBDISPATCH "Enable libdispatch support")
option(ENABLE_COVERAGE "Enable code coverage")

if(LINUX)
    option(ENABLE_SYSTEMD "Enable systemd support" ON)
    option(BUILD_BUS "Build and install bus transport" ON)
    option(BUILD_KMOD "Build and install kmod" ON)
endif()

set(PKGCONFIG_C_FLAGS "-fblocks")
set(CPACK_PACKAGE_NAME librpc)
set(CPACK_PACKAGE_VENDOR Two Pore Guys)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 3)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER Jakub Klama <jakub.klama@twoporeguys.com>)
set(CPACK_GENERATOR DEB)

execute_process(
        COMMAND git describe --exact-match HEAD
        RESULT_VARIABLE RELEASE_TAG
        OUTPUT_QUIET
        ERROR_QUIET
)

if(NOT RELEASE_TAG)
    execute_process(
            COMMAND git rev-list --count HEAD
            OUTPUT_VARIABLE CPACK_PACKAGE_VERSION_PATCH
            OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

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

if(ENABLE_SYSTEMD)
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
endif()

if(BUNDLED_BLOCKS_RUNTIME)
    include_directories(contrib/BlocksRuntime)
endif()

if (ENABLE_UBSAN)
    set(PKGCONFIG_C_FLAGS "${PKGCONFIG_C_FLAGS} -fsanitize=undefined -fsanitize=integer")
endif()

if (ENABLE_UBSAN_NULLABILITY)
    set(PKGCONFIG_C_FLAGS "${PKGCONFIG_C_FLAGS} -fsanitize=nullability")
endif()

if (ENABLE_ASAN)
    set(PKGCONFIG_C_FLAGS "${PKGCONFIG_C_FLAGS} -fsanitize=address")
endif()

if (ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

function(python_install SETUP_PY)
    if(EXISTS "$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/lib/${PYTHON_VERSION}/dist-packages")
        install(CODE "
        execute_process(
            COMMAND ${PYTHON} ${SETUP_PY} install
                --record python_manifest.txt
                --root=/\$ENV{DESTDIR}
                --install-lib=${CMAKE_INSTALL_PREFIX}/lib/${PYTHON_VERSION}/dist-packages
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )")
    elseif(DEFINED "$ENV{CUSTOM_PREFIX}")
        install(CODE "
        execute_process(
            COMMAND ${PYTHON} ${SETUP_PY} install
                --record python_manifest.txt
                --root=/\$ENV{DESTDIR}
                --prefix=${CMAKE_INSTALL_PREFIX}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )")
    else()
        install(CODE "
        execute_process(
            COMMAND ${PYTHON} ${SETUP_PY} install
                --record python_manifest.txt
                --root=/\$ENV{DESTDIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )")
    endif()
endfunction()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-fblocks -Wall -Wextra -Wno-unused-parameter -DRPC_TRACE ${PKGCONFIG_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-fblocks -Wall -Wextra -Wno-unused-parameter -DRPC_TRACE ${PKGCONFIG_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
include_directories(include)
include_directories(/usr/local/include)
include_directories(${CMAKE_BINARY_DIR}/include)
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

if(ENABLE_LIBDISPATCH)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DLIBDISPATCH_SUPPORT")
endif()

if(BUILD_JSON)
    include_directories(${YAJL_INCLUDE_DIRS})
    link_directories(${YAJL_LIBRARY_DIRS})
endif()

if(BUILD_LIBUSB)
    include_directories(${LIBUSB_INCLUDE_DIRS})
    link_directories(${LIBUSB_LIBRARY_DIRS})
endif()

if(ENABLE_SYSTEMD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSYSTEMD_SUPPORT")
    include_directories(${SYSTEMD_INCLUDE_DIRS})
    link_directories(${SYSTEMD_LIBRARY_DIRS})
endif()

set(HEADERS
        include/rpc/object.h
        include/rpc/connection.h
        include/rpc/server.h
        include/rpc/client.h
        include/rpc/service.h
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
        src/rpc_query.c
        src/rpc_bus.c
        src/rpc_serializer.c
        src/rpc_typing.c
        src/rpc_rpcd_client.c
        src/utils.c
        src/internal.h
        src/linker_set.h
        src/memfd.h)

set(TRANSPORT_FILES
        src/transport/socket.c
        src/transport/ws.c
        src/transport/loopback.c
        src/transport/fd.c)

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

if(BUILD_XPC)
    set(TRANSPORT_FILES ${TRANSPORT_FILES} src/transport/xpc.c)
endif()

add_library(librpc SHARED
        ${HEADERS}
        ${TRANSPORT_FILES}
        ${SERIALIZER_FILES}
        ${TYPE_CLASS_FILES}
        ${VALIDATOR_FILES}
        ${CORE_FILES})

set_target_properties(librpc PROPERTIES PREFIX "")
set_target_properties(librpc PROPERTIES SOVERSION 1)
set_target_properties(librpc PROPERTIES MACOSX_RPATH ON)
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

if(ENABLE_LIBDISPATCH AND NOT APPLE)
	target_link_libraries(librpc dispatch)
endif()

if(ENABLE_SYSTEMD)
    target_link_libraries(librpc ${SYSTEMD_LIBRARIES})
endif()

configure_file(librpc.pc.in ${CMAKE_CURRENT_BINARY_DIR}/librpc.pc @ONLY)
configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/rpc/config.h)
install(TARGETS librpc DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(DIRECTORY idl/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/idl)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/librpc.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/share/pkgconfig)

if(BUILD_TESTS)
	set(TEST_SOURCES
			src/linker_set.h
			tests/tests.h
			tests/main.c
			tests/api/bus.c
			tests/api/client.c
			tests/api/connection.c
			tests/api/object.c
			tests/api/query.c
			tests/api/serializer.c
			tests/api/server.c
			tests/api/service.c
			tests/api/typing.c
			tests/internal.c
			tests/transport.c)

add_executable(test_suite ${TEST_SOURCES})
target_link_libraries(test_suite ${GLIB_LIBRARIES})
target_link_libraries(test_suite librpc)
endif()

if(BUILD_EXAMPLES)
	add_subdirectory(examples/client)
	add_subdirectory(examples/event)
	add_subdirectory(examples/loopback)
	add_subdirectory(examples/pack-unpack)
	add_subdirectory(examples/server)
	add_subdirectory(examples/error-backtrace)
	add_subdirectory(examples/query)
	add_subdirectory(examples/validation)

    if(UNIX)
        add_subdirectory(examples/fd-client)
        add_subdirectory(examples/fd-server)
        add_subdirectory(examples/fd-transport)
    endif()

    if(LINUX)
        add_subdirectory(examples/shm-client)
        add_subdirectory(examples/shm-server)
    endif()

    if (BUILD_BUS OR BUILD_LIBUSB)
        add_subdirectory(examples/bus)
        add_subdirectory(examples/log-reader)
    endif()

    if (ENABLE_LIBDISPATCH)
        add_subdirectory(examples/libdispatch)
    endif()
endif()

if(BUILD_RPCTOOL)
    add_subdirectory(tools/rpctool)
endif()

if(BUILD_RPCD AND LINUX)
    add_subdirectory(tools/rpcd)
endif()

if(BUILD_RPCDOC)
    add_subdirectory(tools/rpcdoc)
endif()

if(BUILD_PYTHON)
    add_subdirectory(bindings/python)
endif()

if(BUILD_CPLUSPLUS)
    add_subdirectory(bindings/c++)
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
