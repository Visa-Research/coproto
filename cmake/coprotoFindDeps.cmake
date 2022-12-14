cmake_policy(PUSH)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0045 NEW)
cmake_policy(SET CMP0074 NEW)


if(NOT coproto_FIND_QUIETLY)
    message(STATUS "Option: COPROTO_STAGE = ${COPROTO_STAGE}")
endif()

set(PUSHED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "${COPROTO_STAGE};${CMAKE_PREFIX_PATH}")

## span-lite
###########################################################################
if(NOT COPROTO_CPP20)

    if(COPROTO_FETCH_AUTO AND NOT DEFINED COPROTO_FETCH_SPAN AND COPROTO_BUILD)
        set(COPROTO_FETCH_SPAN ON)
    endif()
    if (COPROTO_FETCH_SPAN  AND COPROTO_BUILD)
        find_package(span-lite QUIET ${COPROTO_FIND_PACKAGE_OPTIONS})
        include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getSpanLite.cmake")
    endif()
    
    find_package(span-lite REQUIRED ${COPROTO_FIND_PACKAGE_OPTIONS})
endif()


## macoro
###########################################################################

if(COPROTO_FETCH_AUTO AND NOT DEFINED COPROTO_FETCH_MACORO AND COPROTO_BUILD)
    set(COPROTO_FETCH_MACORO ON)
endif()

if(NOT DEFINED COPROTO_CPP_VER)
    message(FATAL_ERROR "COPROTO_CPP_VER not defined")
endif()

set(macoro_options cpp_${COPROTO_CPP_VER})
if(COPROTO_PIC)
    set(macoro_options ${macoro_options} pic)
else()
    set(macoro_options ${macoro_options} no_pic)
endif()
if(COPROTO_ASAN)
    set(macoro_options ${macoro_options} asan)
else()
    set(macoro_options ${macoro_options} no_asan)
endif()


if(    "${COPROTO_BUILD_TYPE}" STREQUAL "Release"
    OR "${COPROTO_BUILD_TYPE}" STREQUAL "Debug"
    OR "${COPROTO_BUILD_TYPE}" STREQUAL "RelWithDebInfo" )
    set(macoro_options ${macoro_options} ${COPROTO_BUILD_TYPE} )
endif()

if (COPROTO_FETCH_MACORO)
    find_package(macoro ${COPROTO_FIND_PACKAGE_OPTIONS} COMPONENTS ${macoro_options})
    if(TARGET macoro::macoro)
        set(MACORO_FOUND ON)
    else()
        set(MACORO_FOUND OFF)
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getMacoro.cmake")
endif()
find_package(macoro REQUIRED ${COPROTO_FIND_PACKAGE_OPTIONS} COMPONENTS ${macoro_options})

if((MACORO_CPP_20 AND NOT COPROTO_CPP20) OR (NOT MACORO_CPP_20 AND COPROTO_CPP20))
    message(FATAL_ERROR "MACORO and coproto do not match on CPP 20 config")
endif()

## function2
###########################################################################

if(COPROTO_FETCH_AUTO AND NOT DEFINED COPROTO_FETCH_FUNCTION2 AND COPROTO_BUILD)
    set(COPROTO_FETCH_FUNCTION2 ON)
endif()
if (COPROTO_FETCH_FUNCTION2  AND COPROTO_BUILD)
    find_package(function2 QUIET)
    include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getFunction2.cmake")
endif()
find_package(function2 REQUIRED)
set_target_properties(function2::function2 PROPERTIES INTERFACE_COMPILE_FEATURES "")

## Boost
###########################################################################
if(COPROTO_ENABLE_BOOST AND COPROTO_FETCH_AUTO AND NOT DEFINED COPROTO_FETCH_BOOST AND COPROTO_BUILD)
    set(COPROTO_FETCH_BOOST ON)
endif()

macro(FIND_BOOST)
    set(ARGS ${ARGN})
    if(COPROTO_FETCH_BOOST)
        list(APPEND ARGS NO_DEFAULT_PATH  PATHS ${COPROTO_STAGE} )
    endif()
    option(Boost_USE_MULTITHREADED "mt boost" ON)
    option(Boost_USE_STATIC_LIBS "static boost" ON)

    if(MSVC)
        option(Boost_LIB_PREFIX "Boost_LIB_PREFIX" "lib")
    endif()
    #set(Boost_DEBUG ON)  #<---------- Real life saver
    find_package(Boost 1.77.0 COMPONENTS system thread regex ${ARGS} ${COPROTO_FIND_PACKAGE_OPTIONS})
endmacro()


if(COPROTO_FETCH_BOOST AND COPROTO_BUILD)
    FIND_BOOST(QUIET)
    include("${CMAKE_CURRENT_LIST_DIR}/../thirdparty/getBoost.cmake")
endif()


if(COPROTO_ENABLE_BOOST)

    FIND_BOOST()
    if(NOT Boost_FOUND)
        message(FATAL_ERROR "Failed to find boost 1.77. When building coproto, add -DCOPROTO_FETCH_BOOST=ON or -DCOPROTO_FETCH_AUTO=ON to auto download.")
    endif()

    message(STATUS "\n\nBoost_LIB: ${Boost_LIBRARIES}" )
    message(STATUS "Boost_INC: ${Boost_INCLUDE_DIR}\n\n" )
endif()


## OpenSSL
###########################################################################

if(COPROTO_ENABLE_OPENSSL)
    find_package(OpenSSL REQUIRED)
    
    message("OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}")
    message("OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}")
    message("OPENSSL_LIBRARIES  =${OPENSSL_LIBRARIES}")
    message("OPENSSL_VERSION    =${OPENSSL_VERSION}")
    
endif()



# resort the previous prefix path
set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})
cmake_policy(POP)

find_package(Threads REQUIRED)