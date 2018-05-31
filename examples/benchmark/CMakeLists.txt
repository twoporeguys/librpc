add_executable(benchmark-server server.c)
target_link_libraries(benchmark-server librpc)
add_executable(benchmark-client client.c)
target_link_libraries(benchmark-client librpc)
