execute_process(
        COMMAND pkg-config systemd --variable=systemdsystemunitdir
        OUTPUT_VARIABLE SYSTEMD_UNIT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)

add_executable(rpcd
        src/main.c
        src/ws.c
        src/internal.h)

target_link_libraries(rpcd ${GLIB_LIBRARIES} librpc)
configure_file(etc/rpcd.service.in rpcd.service @ONLY)
configure_file(etc/rpcd.socket.in rpcd.socket @ONLY)

install(TARGETS rpcd DESTINATION sbin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rpcd.service DESTINATION ${SYSTEMD_UNIT_DIR} COMPONENT config)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rpcd.socket DESTINATION ${SYSTEMD_UNIT_DIR} COMPONENT config)
install(DIRECTORY idl/ DESTINATION share/idl/rpcd FILES_MATCHING PATTERN "*.yaml")
