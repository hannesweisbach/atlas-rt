add_library(atlas-dispatch dispatch.c++)
set_target_properties(atlas-dispatch PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(atlas-dispatch PRIVATE Threads::Threads)
target_link_libraries(atlas-dispatch PRIVATE predictor)
if(FOUND_BLOCKSRUNTIME)
target_link_libraries(atlas-dispatch PRIVATE ${BLOCKSRUNTIME_LIB})
endif()

add_subdirectory(tests)
