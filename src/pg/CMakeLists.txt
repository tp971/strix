set (pg_SRCS PGArena.cc PGSolver.cc PGSISolver.cc)

set (TARGET "pg")

add_library(${TARGET} STATIC ${pg_SRCS})

target_link_libraries (${TARGET} ltl)
target_link_libraries (${TARGET} mealy)
target_link_libraries (${TARGET} owl)
target_link_libraries (${TARGET} aut)

target_link_libraries (${TARGET} ${Boost_LIBRARIES})
