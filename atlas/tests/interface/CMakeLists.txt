add_executable(np np.c++)
target_link_libraries(np common Threads::Threads)
set_target_properties(np PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(submit submit.c++)
target_link_libraries(submit common Threads::Threads)
set_target_properties(submit PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(next next.c++)
target_link_libraries(next common Threads::Threads ${Boost_LIBRARIES})
set_target_properties(next PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(update update.c++)
target_link_libraries(update common Threads::Threads)
set_target_properties(update PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(delete delete.c++)
target_link_libraries(delete common Threads::Threads)
set_target_properties(delete PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_executable(threadpool threadpool.c++)
target_link_libraries(threadpool common Threads::Threads ${Boost_LIBRARIES})
set_target_properties(threadpool PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
