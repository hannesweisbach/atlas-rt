project(ATLAS-rt)
cmake_minimum_required(VERSION 2.8)

set(ATLAS_VERSION "440" CACHE STRING "ATLAS Kernel version")

set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)

if(${CMAKE_C_COMPILER_ID} MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Weverything")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")
  set(GTEST_CXX_FLAGS "${GTEST_CXX_FLAGS} -Wno-zero-as-null-pointer-constant")
  set(GTEST_CXX_FLAGS "${GTEST_CXX_FLAGS} -Wno-global-constructors")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

separate_arguments(GTEST_CXX_FLAGS)

# GTest Boilerplate begin
include(ExternalProject)
set_property(DIRECTORY PROPERTY EP_BASE Dependencies)

ExternalProject_Add(GTestExternal
    URL https://github.com/google/googletest/archive/release-1.7.0.zip
    DOWNLOAD_NO_PROGRESS 1
    CMAKE_ARGS -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    # Disable install step
    INSTALL_COMMAND ""
)

ExternalProject_Get_Property(GTestExternal source_dir binary_dir)

set(LIBPREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
set(LIBSUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(GTEST_INCLUDES "${source_dir}/include")
set(GTEST_LIBRARY  "${binary_dir}/${LIBPREFIX}gtest${LIBSUFFIX}")
set(GTEST_MAINLIB  "${binary_dir}/${LIBPREFIX}gtest_main${LIBSUFFIX}")

#work-around until https://cmake.org/Bug/view.php?id=15052 is fixed
file(MAKE_DIRECTORY ${GTEST_INCLUDES})

add_library(GTest IMPORTED STATIC GLOBAL)
set_target_properties(GTest PROPERTIES
    IMPORTED_LOCATION                 "${GTEST_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES     "${GTEST_INCLUDES}"
    INTERFACE_COMPILE_OPTIONS         "${GTEST_CXX_FLAGS}"
    IMPORTED_LINK_INTERFACE_LIBRARIES "Threads::Threads")

add_library(GTestMain IMPORTED STATIC GLOBAL)
set_target_properties(GTestMain PROPERTIES
    IMPORTED_LOCATION "${GTEST_MAINLIB}"
    IMPORTED_LINK_INTERFACE_LIBRARIES "${GTEST_LIBRARY};Threads::Threads")

add_dependencies(GTest GTestExternal)
add_dependencies(GTestMain GTestExternal)

enable_testing()
# GTest Boilerplate end


set(BUILD_SHARED_LIBS ON)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
find_library(DISPATCH_FOUND dispatch)
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  find_library(BLOCKSRUNTIME BlocksRuntime)
  if(${BLOCKSRUNTIME} MATCHES "-NOTFOUND")
    message(STATUS "BlocksRuntime not found.")
    set(FOUND_BLOCKSRUNTIME false)
  else()
    message(STATUS "BlocksRuntime found")
    set(FOUND_BLOCKSRUNTIME true)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fblocks")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fblocks")
  endif()
else()
  message(STATUS "BlocksRuntime only available for Clang")
  set(FOUND_BLOCKSRUNTIME false)
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.54.0 COMPONENTS serialization math_tr1 program_options)

if(${Boost_FOUND})
  include_directories(${Boost_INCLUDE_DIRS})
endif()
include_directories(.)
# find generated atlas/syscalls.h
include_directories(${CMAKE_CURRENT_BINARY_DIR}/atlas)

add_subdirectory(utils)
add_subdirectory(atlas)
add_subdirectory(predictor)
add_subdirectory(runtime)

