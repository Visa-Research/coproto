

file(GLOB_RECURSE SRCS *.cpp)

include_directories(${CMAKE_SOURCE_DIR})


add_executable(frontend  ${SRCS})

target_link_libraries(frontend coproto coproto_tests)