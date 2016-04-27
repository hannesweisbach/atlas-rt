add_library(llsp STATIC llsp.c)
set_target_properties(llsp PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
add_library(predictor STATIC predictor.c++)
set_target_properties(predictor PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(predictor PRIVATE llsp)

add_executable(predictor_test testsuite.c++)
target_link_libraries(predictor_test predictor)
set_target_properties(predictor_test PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
