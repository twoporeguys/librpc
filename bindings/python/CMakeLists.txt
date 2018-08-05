find_program(PYTHON "${PYTHON_VERSION}")

option(BUILD_CLIENT "Compile and install librpc python client application")

set(SETUP_PY ${CMAKE_SOURCE_DIR}/bindings/python/setup.py)
set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)
set(DEPS
        ${CMAKE_SOURCE_DIR}/bindings/python/librpc.pyx
        ${CMAKE_SOURCE_DIR}/bindings/python/librpc.pxd
        ${CMAKE_SOURCE_DIR}/bindings/python/src/bus.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/client.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/connection.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/object.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/serializer.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/server.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/service.pxi
        ${CMAKE_SOURCE_DIR}/bindings/python/src/typing.pxi)

if(BUILD_CLIENT)
    add_subdirectory(examples/client)
endif()

add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/bindings/python
            ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E env
            CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            CMAKE_PREFIX=${CMAKE_INSTALL_PREFIX}
            CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            SYSTEMD_SUPPORT=${ENABLE_SYSTEMD}
            ${PYTHON} ${SETUP_PY} build
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(py-librpc ALL DEPENDS ${OUTPUT})
python_install(${SETUP_PY})
