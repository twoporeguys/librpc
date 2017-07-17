set(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/timestamp)
set(DEPS librpc.c librpc.h librpc_usb.c Makefile)

if (DEFINED ENV{KERNEL_BUILD_DIR})
        set(KERNEL_BUILD $ENV{KERNEL_BUILD_DIR})
else()
        execute_process(
                COMMAND uname -r
                OUTPUT_VARIABLE UNAME_R
                OUTPUT_STRIP_TRAILING_WHITESPACE)

        set(KERNEL_BUILD /lib/modules/${UNAME_R}/build)
endif()

add_custom_command(OUTPUT ${OUTPUT}
        COMMAND cp -a ${CMAKE_SOURCE_DIR}/kmod/* ${CMAKE_CURRENT_BINARY_DIR}/
        COMMAND make -C ${KERNEL_BUILD} M=${CMAKE_CURRENT_BINARY_DIR} modules
        COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
        DEPENDS ${DEPS}
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(bus-kmod ALL DEPENDS ${OUTPUT})
install(CODE "execute_process(COMMAND make -C ${KERNEL_BUILD} M=${CMAKE_CURRENT_BINARY_DIR} modules_install)")
