set (CMAKE_CXX_STANDARD 14)

set(HEADERS
        include/librpc.hh)

set(SOURCE_FILES
        src/rpc_object.cc
        src/rpc_connection.cc)

add_library(librpcpp SHARED
        ${HEADERS}
        ${SOURCE_FILES})

set_target_properties(librpcpp PROPERTIES PREFIX "")
set_target_properties(librpc PROPERTIES MACOSX_RPATH ON)
target_link_libraries(librpcpp librpc)

add_subdirectory(examples/object)
