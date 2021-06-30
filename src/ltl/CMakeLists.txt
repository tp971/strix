set (ltl_SRCS Parser.cc LTLParser.cc)

set (TARGET "ltl")

add_library(${TARGET} STATIC ${ltl_SRCS})

target_link_libraries (${TARGET} owl)

target_link_libraries (${TARGET} ${Boost_LIBRARIES})
target_link_libraries (${TARGET} ${JNI_LIBRARIES})
