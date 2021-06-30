set (aig_SRCS AigerConstructor.cc)

set (TARGET "aig")

add_library(${TARGET} STATIC ${aig_SRCS})

target_link_libraries (${TARGET} mealy)
target_link_libraries (${TARGET} aiger)
target_link_libraries (${TARGET} cudd)
target_link_libraries (${TARGET} libabc)
target_link_libraries (${TARGET} ${Boost_LIBRARIES})
target_link_libraries (${TARGET} ${JNI_LIBRARIES})
