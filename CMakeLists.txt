cmake_minimum_required (VERSION 3.11)

set (PROJECT_NAME "strix")
project (${PROJECT_NAME} C CXX)

set (CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: None Debug Release."
       FORCE)
endif ()

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin CACHE PATH "Output directory for binary")

# set warning level
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # using GCC or Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # using Intel C++
    set_property (TARGET ${TARGET} APPEND_STRING PROPERTY COMPILE_FLAGS -W3)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++
    if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string (REGEX REPLACE "/W[0-4]" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else ()
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    endif ()
endif ()

# add external libraries
set (LIB_DIR ${PROJECT_SOURCE_DIR}/lib)
add_subdirectory (${LIB_DIR})

# add main source
add_subdirectory (${PROJECT_SOURCE_DIR}/src)

# enable testing
enable_testing()

# add tests
set (TARGET_BINARY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME})
add_subdirectory (${PROJECT_SOURCE_DIR}/test)
