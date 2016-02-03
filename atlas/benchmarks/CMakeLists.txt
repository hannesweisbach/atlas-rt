add_executable(benchmark_submit submit.c++)
set_target_properties(benchmark_submit PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(benchmark_submit ${Boost_LIBRARIES} common)

include_directories(${GMP_INCLUDE_DIRS})
add_library(taskgen taskgen.c++)
set_target_properties(taskgen PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(taskgen ${GMP_LIBRARIES})

add_executable(periodic periodic_taskset.c++)
set_target_properties(periodic PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(periodic taskgen common Threads::Threads ${Boost_LIBRARIES})

add_executable(concurrent concurrent.c++)
set_target_properties(concurrent PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(concurrent taskgen common Threads::Threads ${Boost_LIBRARIES})
