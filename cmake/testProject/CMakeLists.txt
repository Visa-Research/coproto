
cmake_minimum_required (VERSION 3.15)

project (TestProj VERSION  1.0.0)

add_executable(main "src.cpp")

message("CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}")

find_package(coproto REQUIRED HINTS ${COPROTO_HINT} COMPONENTS )

target_link_libraries(main coproto::coproto)
target_link_libraries(main coproto::coproto)

message(STATUS "COPROTO_CPP20          = ${COPROTO_CPP20}")
message(STATUS "COPROTO_ENABLE_BOOST   = ${COPROTO_ENABLE_BOOST}")
message(STATUS "COPROTO_LOGGING        = ${COPROTO_LOGGING}")




if(MSVC)
    target_compile_options( main PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/std=c++${COPROTO_CPP_VER}>
    )
else()
    target_compile_options( main PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++${COPROTO_CPP_VER}>
    )
endif()