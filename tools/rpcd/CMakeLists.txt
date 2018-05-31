add_executable(rpcd src/main.c)
target_link_libraries(rpcd ${GLIB_LIBRARIES} librpc)
install(TARGETS rpcd DESTINATION sbin)
