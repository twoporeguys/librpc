find_program(PYTHON "${PYTHON_VERSION}")

option(BUILD_CLIENT "Compile and install librpc python client application")

set(SETUP_PY ${CMAKE_SOURCE_DIR}/bindings/python/setup.py)
set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)
set(DEPS
        ${CMAKE_SOURCE_DIR}/bindings/python/librpc.pyx
        ${CMAKE_SOURCE_DIR}/bindings/python/librpc.pxd)

if(BUILD_CLIENT)
    add_subdirectory(examples/client)
endif()

add_custom_command(OUTPUT ${OUTPUT}
        COMMAND cp -a ${CMAKE_SOURCE_DIR}/bindings/python/* ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND env LDFLAGS="-L${CMAKE_CURRENT_BINARY_DIR}/../.." CFLAGS="-I${CMAKE_SOURCE_DIR}/include" ${PYTHON} ${SETUP_PY} build
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(py-librpc ALL DEPENDS ${OUTPUT})

install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install --record python_manifest.txt --prefix=${CMAKE_INSTALL_PREFIX} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
