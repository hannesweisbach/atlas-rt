add_executable(eventloop eventloop.c++)
set_target_properties(eventloop PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(eventloop atlas-runtime)

add_executable(testcase test.c++)
set_target_properties(testcase PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(testcase atlas-runtime)

if(FOUND_BLOCKSRUNTIME)
add_executable(blockstest atlas_blocks.c++)
set_target_properties(blockstest PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(blockstest atlas-runtime GTest)
target_compile_options(blockstest PRIVATE -Wno-global-constructors)

add_executable(gcd-compat-test gcd-compat-test.c++)
set_target_properties(gcd-compat-test PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(gcd-compat-test gcd-compat GTest)
target_compile_options(gcd-compat-test PRIVATE -Wno-global-constructors)
endif()

add_executable(concurrent-queue-tests concurrent-queue-tests.c++)
set_target_properties(concurrent-queue-tests PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(concurrent-queue-tests GTest atlas-runtime)

add_executable(broken broken.c++)
set_target_properties(broken PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(broken atlas-runtime)
