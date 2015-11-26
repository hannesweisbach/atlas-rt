add_executable(benchmark_submit submit.c++)
set_target_properties(benchmark_submit PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(benchmark_submit ${Boost_LIBRARIES} common)
