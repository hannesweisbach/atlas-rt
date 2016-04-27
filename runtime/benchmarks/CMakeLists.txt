add_library(taskgen taskgen.c++)
set_target_properties(taskgen PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(taskgen gmp)

#add_executable(work work.c++)
#set_target_properties(work PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

