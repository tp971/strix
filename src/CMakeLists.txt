include_directories (${CMAKE_CURRENT_SOURCE_DIR})

# fix for numeric_limits::max() on Windows
if (WIN32)
    add_definitions (-DNOMINMAX)
endif ()

# set Boost options
set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_RUNTIME OFF)
if (WIN32)
	set (Boost_USE_STATIC_LIBS ON)
else ()
	set (Boost_USE_STATIC_LIBS OFF)
endif ()

# add Java and JNI
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include_directories (${Java_INCLUDE_DIRS})
include_directories (${JNI_INCLUDE_DIRS})

# add OWL
include_directories (${LIB_DIR}/owl/src/main/headers)
set (OWL_LIB_DIR "${LIB_DIR}/owl/build/libs")

# add Boost
find_package (Boost 1.58 REQUIRED COMPONENTS program_options filesystem iostreams system)
include_directories (${Boost_INCLUDE_DIRS})

# add OpenMP
find_package(OpenMP REQUIRED)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# add CUDD
set (CUDD_INCLUDE_DIRS
    "${CMAKE_BINARY_DIR}/lib/cudd"
    "${LIB_DIR}/cudd/cudd"
    "${LIB_DIR}/cudd/cplusplus"
    "${LIB_DIR}/cudd/util"
    "${LIB_DIR}/cudd/epd"
    "${LIB_DIR}/cudd/mtr"
    "${LIB_DIR}/cudd/st"
    "${LIB_DIR}/cudd/dddmp"
)
include_directories (${CUDD_INCLUDE_DIRS})

# add lib folder for includes for AIGER and ABC library
include_directories (${LIB_DIR})

# add folder for MiniSat includes
include_directories (${LIB_DIR}/minisat)

# add support for multi-threading
find_package (Threads)

add_subdirectory(ltl)
add_subdirectory(aut)
add_subdirectory(mealy)
add_subdirectory(pg)
add_subdirectory(aig)
add_subdirectory(strix)
