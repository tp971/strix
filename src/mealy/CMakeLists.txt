# add MeMin
add_subdirectory (memin)

set (mealy_SRCS MealyMachine.cc)

set (TARGET "mealy")

add_library(${TARGET} STATIC ${mealy_SRCS})

target_link_libraries (${TARGET} memin)
target_link_libraries (${TARGET} ${Boost_LIBRARIES})
