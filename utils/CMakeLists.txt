add_executable(cpuhog cpuhog.c++)
add_executable(progress progress.c++)
set_target_properties(progress PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_library(common STATIC common.c++)
set_target_properties(common PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
