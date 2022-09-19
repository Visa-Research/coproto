

file(GLOB_RECURSE SRCS *.cpp)

include_directories(${CMAKE_SOURCE_DIR})

add_library(coproto_tests  ${SRCS})

target_link_libraries(coproto_tests coproto)


set(COPROTO_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
configure_file(config.h.in "config.h" )
