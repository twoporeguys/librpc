set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)
set(DEPS librpc.c librpc.h librpc_usb.c Makefile)
set(KERNEL_BUILD /lib/modules/4.10.0-26-generic/build)

add_custom_command(OUTPUT ${OUTPUT}
        COMMAND cp -a ${CMAKE_SOURCE_DIR}/kmod/* ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND make -C ${KERNEL_BUILD} M=${CMAKE_CURRENT_BINARY_DIR} modules
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(bus-kmod ALL DEPENDS ${OUTPUT})
