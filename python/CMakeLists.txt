find_program(PYTHON "python3")

set(SETUP_PY ${CMAKE_SOURCE_DIR}/python/setup.py)
set(DEPS ${CMAKE_SOURCE_DIR}/python/librpc.pyx ${CMAKE_SOURCE_DIR}/python/librpc.pxd)
set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)

add_custom_command(OUTPUT ${OUTPUT}
        COMMAND env LDFLAGS="-L${CMAKE_CURRENT_BINARY_DIR}/.." ${PYTHON} ${SETUP_PY} build -b ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/python
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(py-librpc ALL DEPENDS ${OUTPUT})

install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install -b ${CMAKE_CURRENT_BINARY_DIR} --root=\$DESTDIR WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/python)")

