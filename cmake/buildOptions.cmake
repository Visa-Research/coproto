


message(STATUS "Option: CMAKE_INSTALL_PREFIX    = ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Option: CMAKE_BUILD_TYPE        = ${CMAKE_BUILD_TYPE}\n\tRelease\n\tDebug\n\tRelWithDebInfo")
message(STATUS "Option: COPROTO_FETCH_AUTO      = ${COPROTO_FETCH_AUTO}")
message(STATUS "Option: COPROTO_FETCH_SPAN      = ${COPROTO_FETCH_SPAN}")
message(STATUS "Option: COPROTO_FETCH_FUNCTION2 = ${COPROTO_FETCH_FUNCTION2}")
message(STATUS "Option: COPROTO_FETCH_MACORO    = ${COPROTO_FETCH_MACORO}")
message(STATUS "Option: COPROTO_FETCH_BOOST     = ${COPROTO_FETCH_BOOST}\n")


if(NOT DEFINED COPROTO_CPP_VER)
	set(COPROTO_CPP_VER 14)
endif()

option(COPROTO_ENABLE_ASSERTS "compile the library with asserts enabled" ON)

message(STATUS "Option: COPROTO_CPP_VER         = ${COPROTO_CPP_VER}")
message(STATUS "Option: COPROTO_ENABLE_BOOST    = ${COPROTO_ENABLE_BOOST}")
message(STATUS "Option: COPROTO_ENABLE_OPENSSL  = ${COPROTO_ENABLE_OPENSSL}")

message(STATUS "Option: COPROTO_ENABLE_ASSERTS  = ${COPROTO_ENABLE_ASSERTS}\n")

option(COPROTO_ASAN "build the library with asan enabled." false)
option(COPROTO_PIC "build the library with -fPIC" OFF)


set(COPROTO_BUILD_TYPE ${CMAKE_BUILD_TYPE})
if(COPROTO_CPP_VER EQUAL 20)
	set(COPROTO_CPP20 ON)
else()
	set(COPROTO_CPP20 OFF)
endif()

if(NOT COPROTO_CPP_VER EQUAL 20 AND
	NOT COPROTO_CPP_VER EQUAL 17 AND
	NOT COPROTO_CPP_VER EQUAL 14
)

	message(FATAL_ERROR "Unknown c++ version. COPROTO_CPP_VER=${COPROTO_CPP_VER}")
endif()
message(STATUS "Option: COPROTO_STAGE = ${COPROTO_STAGE}")

