add_executable(rpctool rpctool.c)
target_link_libraries(rpctool ${GLIB_LIBRARIES} librpc)
set_target_properties(rpctool PROPERTIES INSTALL_RPATH_USE_LINK_PATH ON)
install(TARGETS rpctool DESTINATION bin)
