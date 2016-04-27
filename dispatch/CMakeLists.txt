add_library(atlas-dispatch dispatch.c++)
set_target_properties(atlas-dispatch PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(atlas-dispatch INTERFACE Threads::Threads)
target_link_libraries(atlas-dispatch PRIVATE predictor)
if(FOUND_BLOCKSRUNTIME)
target_link_libraries(atlas-dispatch INTERFACE ${BLOCKSRUNTIME_LIB})
endif()

install(TARGETS atlas-dispatch DESTINATION lib)
install(FILES dispatch.h DESTINATION include/atlas)

add_subdirectory(tests)
