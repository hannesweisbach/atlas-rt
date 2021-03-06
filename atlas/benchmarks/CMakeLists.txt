if(${Boost_PROGRAM_OPTIONS_FOUND})
  add_executable(benchmark_submit submit.c++)
  set_target_properties(benchmark_submit PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  target_link_libraries(benchmark_submit ${Boost_PROGRAM_OPTIONS_LIBRARY} common)
else()
  message(STATUS "submit benchmark not available - Boost program_options missing.")
endif()

if(${Boost_MATH_TR1_FOUND})
  add_library(taskgen taskgen.c++)
  set_target_properties(taskgen PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
else()
  message(STATUS "Task generation library not available - Boost math missing.")
endif()

if(${Boost_PROGRAM_OPTIONS_FOUND} AND ${Boost_MATH_TR1_FOUND})
  add_executable(periodic periodic_taskset.c++)
  set_target_properties(periodic PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  target_link_libraries(periodic taskgen common Threads::Threads ${Boost_PROGRAM_OPTIONS_LIBRARY})

  add_executable(concurrent concurrent.c++)
  set_target_properties(concurrent PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  target_link_libraries(concurrent taskgen common Threads::Threads ${Boost_PROGRAM_OPTIONS_LIBRARY})
else()
  message(STATUS "periodic and concurrent benchmarks not available - Boost math and/or program_options missing.")
endif()

add_executable(tardiness tardiness.c++)
set_target_properties(tardiness PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(tardiness Threads::Threads)
