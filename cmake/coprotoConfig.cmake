
# these are just pass through config file for the ones that are placed in the build directory.
include("${CMAKE_CURRENT_LIST_DIR}/preamble.cmake")



if(NOT EXISTS "${COPROTO_BUILD_DIR}")
    message(FATAL_ERROR "failed to find the coproto build directory. Looked at: ${COPROTO_BUILD_DIR}")
endif()

if(NOT COPROTO_FIND_QUIETLY)
    message("coprotoConfig.cmake: ${CMAKE_CURRENT_LIST_DIR}")
endif()

#used to find macoro
set(PUSHED_CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH "${COPROTO_BUILD_DIR}/macoro;${CMAKE_PREFIX_PATH}")
#message("\n\nhere ${CMAKE_CURRENT_LIST_FILE}\nCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}\n\n")

include("${COPROTO_BUILD_DIR}/coproto/coprotoConfig.cmake")


set(CMAKE_PREFIX_PATH ${PUSHED_CMAKE_PREFIX_PATH})