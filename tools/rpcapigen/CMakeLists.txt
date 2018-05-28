find_program(PYTHON "${PYTHON_VERSION}")

set(SETUP_PY ${CMAKE_SOURCE_DIR}/tools/rpcapigen/setup.py)
set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)
file(GLOB_RECURSE DEPS ${CMAKE_SOURCE_DIR}/tools/rpcapigen *)

add_custom_command(OUTPUT ${OUTPUT}
        COMMAND cp -a ${CMAKE_SOURCE_DIR}/tools/rpcapigen/* ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND ${PYTHON} ${SETUP_PY} bdist
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(rpcapigen ALL DEPENDS ${OUTPUT})
python_install(${SETUP_PY})
