add_library(llsp llsp.c)
add_library(predictor predictor.c++)
set_target_properties(predictor PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(predictor PRIVATE llsp)

add_executable(predictor_test testsuite.c++)
target_link_libraries(predictor_test predictor)
set_target_properties(predictor_test PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
