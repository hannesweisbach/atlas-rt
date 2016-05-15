add_executable(serialization serialization.c++)
target_link_libraries(serialization predictor GTest)
set_target_properties(serialization PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_compile_options(serialization PRIVATE -Wno-global-constructors)
