# add CUDD
add_subdirectory (cudd)

# add aiger
add_subdirectory (aiger)

# add ABC (without readline)
set (READLINE_FOUND FALSE)
add_subdirectory (abc EXCLUDE_FROM_ALL)

# add OWL
add_subdirectory (owl/src/main/cpp/library)

# add MiniSat
add_subdirectory (minisat EXCLUDE_FROM_ALL)
