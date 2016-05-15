add_library(llsp STATIC llsp.c)
set_target_properties(llsp PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

add_library(predictor predictor.c++)
set_target_properties(predictor PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(predictor PRIVATE llsp)
if(${Boost_SERIALIZATION_FOUND})
  set_property(SOURCE predictor.c++ PROPERTY COMPILE_DEFINITIONS "HAVE_BOOST_SERIALIZATION=1")
  target_link_libraries(predictor PRIVATE ${Boost_SERIALIZATION_LIBRARY})
else()
  message(STATUS "Boost Serialization not found - save and restore of the predictor state is disabled.")
endif()

add_executable(predictor_test testsuite.c++)
target_link_libraries(predictor_test predictor)
set_target_properties(predictor_test PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

add_subdirectory(tests)

install(TARGETS predictor DESTINATION lib)
install(FILES predictor.h DESTINATION include/atlas)
