if(NOT ${DISPATCH_FOUND} MATCHES "-NOTFOUND" AND
   NOT ${BLOCKSRUNTIME_FOUND} MATCHES "-NOTFOUND" AND
   ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set(HAVE_GCD ON)
  add_definitions(-DHAVE_GCD)
else()
  set(HAVE_GCD OFF)
endif()

if(HAVE_GCD)
  add_library(gcd-backend STATIC gcd-backend.c++)
  set_target_properties(gcd-backend PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
  message(STATUS "${DISPATCH_FOUND}")
  target_link_libraries(gcd-backend PRIVATE ${DISPATCH_FOUND} ${CMAKE_DL_LIBS})
endif()

add_library(atlas-runtime dispatch.c++)
set_target_properties(atlas-runtime PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
if(HAVE_GCD)
  target_link_libraries(atlas-runtime PRIVATE gcd-backend)
endif()
target_link_libraries(atlas-runtime INTERFACE Threads::Threads)
target_link_libraries(atlas-runtime PRIVATE predictor)
if(FOUND_BLOCKSRUNTIME)
  target_link_libraries(atlas-runtime INTERFACE ${BLOCKSRUNTIME_LIB})
endif()

add_library(gcd-compat gcd-compat.c++)
set_target_properties(gcd-compat PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(gcd-compat PRIVATE atlas-runtime)
if(FOUND_BLOCKSRUNTIME)
  target_link_libraries(gcd-compat INTERFACE ${BLOCKSRUNTIME_LIB})
endif()

install(TARGETS atlas-runtime gcd-compat DESTINATION lib)
install(FILES dispatch.h gcd-compat.h DESTINATION include/atlas)

add_subdirectory(tests)
