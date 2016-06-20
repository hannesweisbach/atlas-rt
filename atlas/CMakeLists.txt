add_subdirectory(tests)
add_subdirectory(benchmarks)

configure_file(syscalls.h.in syscalls.h)
install(FILES atlas.h DESTINATION include/atlas)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/syscalls.h DESTINATION include/atlas)
