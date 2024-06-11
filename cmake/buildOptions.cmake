include_guard(GLOBAL)

set(COPROTO_BUILD ON)


message(STATUS "Option: CMAKE_INSTALL_PREFIX    = ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Option: CMAKE_BUILD_TYPE        = ${CMAKE_BUILD_TYPE}\n\tRelease\n\tDebug\n\tRELWITHDEBINFO")
message(STATUS "Option: COPROTO_NO_SYSTEM_PATH  = ${COPROTO_NO_SYSTEM_PATH}\t  ~ do not look in system paths for dependencies")
message(STATUS "Option: COPROTO_FETCH_AUTO      = ${COPROTO_FETCH_AUTO}\t  ~ automatically fetch dependencies")
message(STATUS "Option: COPROTO_FETCH_SPAN      = ${COPROTO_FETCH_SPAN}\t  ~ always fetch span")
message(STATUS "Option: COPROTO_FETCH_FUNCTION2 = ${COPROTO_FETCH_FUNCTION2}\t  ~ always fetch function2")
message(STATUS "Option: COPROTO_FETCH_MACORO    = ${COPROTO_FETCH_MACORO}\t  ~ always fetch macoro")
message(STATUS "Option: COPROTO_FETCH_BOOST     = ${COPROTO_FETCH_BOOST}\t  ~ always fetch boost\n")


if(NOT DEFINED COPROTO_CPP_VER)
	set(COPROTO_CPP_VER 20)
endif()
if(COPROTO_CPP_VER LESS 20 AND NOT DEFINED COPROTO_ENABLE_SPAN)
	set(COPROTO_ENABLE_SPAN ON)
endif()
option(COPROTO_PIC "build the library with -fPIC" OFF)
option(COPROTO_ASAN "build the library with asan enabled." false)


option(COPROTO_ENABLE_ASSERTS "compile the library with asserts enabled" ON)

message(STATUS "Option: COPROTO_CPP_VER         = ${COPROTO_CPP_VER}")
message(STATUS "Option: COPROTO_PIC             = ${COPROTO_PIC}")
message(STATUS "Option: COPROTO_ASAN            = ${COPROTO_ASAN}")
message(STATUS "Option: COPROTO_ENABLE_BOOST    = ${COPROTO_ENABLE_BOOST}")
message(STATUS "Option: COPROTO_ENABLE_SPAN     = ${COPROTO_ENABLE_SPAN}")
message(STATUS "Option: COPROTO_ENABLE_OPENSSL  = ${COPROTO_ENABLE_OPENSSL}")

message(STATUS "Option: COPROTO_ENABLE_ASSERTS  = ${COPROTO_ENABLE_ASSERTS}\n")



set(COPROTO_BUILD_TYPE ${CMAKE_BUILD_TYPE})
if(COPROTO_CPP_VER EQUAL 20)
	set(COPROTO_CPP20 ON)
else()
	set(COPROTO_CPP20 OFF)
endif()
if(COPROTO_CPP_VER EQUAL 20)
	set(COPROTO_CPP20_FALSE OFF)
else()
	set(COPROTO_CPP20_FALSE ON)
endif()

if (NOT COPROTO_CPP_VER EQUAL 23 AND
    NOT COPROTO_CPP_VER EQUAL 20 
)

	message(FATAL_ERROR "Unsupported c++ version. COPROTO_CPP_VER=${COPROTO_CPP_VER}")
endif()
message(STATUS "Option: COPROTO_STAGE = ${COPROTO_STAGE}")

