add_executable(rpctool rpctool.c)
target_link_libraries(rpctool ${GLIB_LIBRARIES} librpc)
