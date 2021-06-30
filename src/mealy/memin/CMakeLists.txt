set (memin_SRCS DIMACSWriter.cc MeMin.cc MachineBuilder.cc)

set (TARGET "memin")

add_library(${TARGET} STATIC ${memin_SRCS})

target_link_libraries (${TARGET} ${Boost_LIBRARIES})
target_link_libraries (${TARGET} minisat-lib-static)
