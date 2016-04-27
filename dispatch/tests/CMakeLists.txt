add_executable(eventloop eventloop.c++)
set_target_properties(eventloop PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(eventloop atlas-dispatch)

add_executable(testcase test.c++)
set_target_properties(testcase PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(testcase atlas-dispatch)

if(FOUND_BLOCKSRUNTIME)
add_executable(blockstest atlas_blocks.c++)
set_target_properties(blockstest PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(blockstest atlas-dispatch)

add_executable(ctestcase test.c)
set_target_properties(ctestcase PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_link_libraries(ctestcase atlas-dispatch)
endif()

