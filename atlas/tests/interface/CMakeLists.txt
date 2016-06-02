add_executable(np np.c++)
target_link_libraries(np common Threads::Threads)
set_target_properties(np PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(submit submit.c++)
target_link_libraries(submit common Threads::Threads)
set_target_properties(submit PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(update update.c++)
target_link_libraries(update common Threads::Threads)
set_target_properties(update PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(delete delete.c++)
target_link_libraries(delete common Threads::Threads)
set_target_properties(delete PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(next next.c++)
target_link_libraries(next Threads::Threads GTest)
set_target_properties(next PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

if(${Boost_PROGRAM_OPTIONS_FOUND})
  add_executable(threadpool threadpool.c++)
  target_link_libraries(threadpool common Threads::Threads ${Boost_PROGRAM_OPTIONS_LIBRARY})
  set_target_properties(threadpool PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
else()
  message(STATUS "next and threadpool tests not avilable - Boost program_options missing.")
endif()
