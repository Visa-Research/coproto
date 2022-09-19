

# these are just pass through config file for the ones that are placed in the build directory.

include("${CMAKE_CURRENT_LIST_DIR}/coprotoFindDeps.cmake")

if(NOT EXISTS "${COPROTO_BUILD_DIR}")
    message(FATAL_ERROR "failed to find the coproto build directory. Looked at: ${COPROTO_BUILD_DIR}")
endif()

include("${COPROTO_BUILD_DIR}/coproto/coprotoConfig.cmake")
