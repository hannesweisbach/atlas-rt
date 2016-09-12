add_subdirectory(tests)
add_subdirectory(benchmarks)

add_library(atlas atlas-clock.c++)
set_target_properties(atlas PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

configure_file(syscalls.h.in syscalls.h)
install(FILES atlas.h DESTINATION include/atlas)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syscalls.h DESTINATION include/atlas)
