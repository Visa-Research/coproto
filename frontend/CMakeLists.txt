

file(GLOB_RECURSE SRCS *.cpp)

include_directories(${CMAKE_SOURCE_DIR})


add_executable(frontend  ${SRCS})

target_link_libraries(frontend coproto coproto_tests)


if(MSVC)
    target_compile_options( frontend PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/std=c++${COPROTO_CPP_VER}>
    )
else()
    target_compile_options( frontend PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++${COPROTO_CPP_VER}>
    )
endif()