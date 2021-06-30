set (strix_SRCS main.cc OptionParser.cc)

set (TARGET "strix")

add_executable (${TARGET} ${strix_SRCS})

target_link_libraries (${TARGET} ${Boost_LIBRARIES})

target_link_libraries (${TARGET} ltl)
target_link_libraries (${TARGET} mealy)
target_link_libraries (${TARGET} pg)
target_link_libraries (${TARGET} aig)
target_link_libraries (${TARGET} owl)
target_link_libraries (${TARGET} ${JNI_LIBRARIES})
target_link_libraries (${TARGET} ${CMAKE_THREAD_LIBS_INIT})
