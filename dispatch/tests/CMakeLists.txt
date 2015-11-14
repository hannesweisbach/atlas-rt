add_executable(eventloop eventloop.c++)
set_target_properties(eventloop PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(eventloop dispatch)


