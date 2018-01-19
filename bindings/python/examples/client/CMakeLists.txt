find_program(PYTHON "${PYTHON_VERSION}")

set(SETUP_PY ${CMAKE_SOURCE_DIR}/python/examples/client/setup.py)
set(DEPS ${CMAKE_SOURCE_DIR}/python/examples/client/librpc_client.py)
set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)

add_custom_command(OUTPUT ${OUTPUT}
        COMMAND cp -a ${CMAKE_SOURCE_DIR}/python/examples/client/* ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND ${PYTHON} ${SETUP_PY} bdist
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(py-librpc-client ALL DEPENDS ${OUTPUT})

install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install --record python_manifest.txt --prefix=${CMAKE_INSTALL_PREFIX} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})")
