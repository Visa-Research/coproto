
set(GIT_REPOSITORY      "https://github.com/ladnir/macoro.git")
set(GIT_TAG             "179f1a5b1ee999b155211ec3eef79af33b8e7251" )

set(CLONE_DIR "${COPROTO_THIRDPARTY_CLONE_DIR}/macoro")
set(BUILD_DIR "${CLONE_DIR}/out/build/${COPROTO_CONFIG}")
set(LOG_FILE  "${CMAKE_CURRENT_LIST_DIR}/log-macoro.txt")

include("${CMAKE_CURRENT_LIST_DIR}/fetch.cmake")

if (NOT MACORO_FOUND 
    OR MACORO_DEV
    OR (
        DEFINED MACORO_FOUND AND 
        (
            (MACORO_CPP_20 AND NOT COPROTO_CPP20) OR
            (NOT MACORO_CPP_20 AND COPROTO_CPP20)
            )
        )
    )
    if(NOT DEFINED COPROTO_STAGE)
        message(FATAL_ERROR "COPROTO_STAGE not defined")
    endif()

    find_program(GIT git REQUIRED)
    set(DOWNLOAD_CMD  ${GIT} clone ${GIT_REPOSITORY})
    set(CHECKOUT_CMD  ${GIT} checkout ${GIT_TAG})
    set(CONFIGURE_CMD ${CMAKE_COMMAND} -S ${CLONE_DIR} -B ${BUILD_DIR} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                       -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} 
                       -DMACORO_FETCH_OPTIONAL=ON
                       -DMACORO_FETCH_VARIANT=ON
                       -DVERBOSE_FETCH=${VERBOSE_FETCH} 
                       -DMACORO_CPP_VER=${COPROTO_CPP_VER}
                       -DMACORO_PIC=${COPROTO_PIC}
                       -DMACORO_ASAN=${COPROTO_ASAN}
                       -DMACORO_THIRDPARTY_CLONE_DIR=${COPROTO_THIRDPARTY_CLONE_DIR}
                       )
    set(BUILD_CMD     ${CMAKE_COMMAND} --build ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE})
    set(INSTALL_CMD   ${CMAKE_COMMAND} --install ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE} --prefix ${COPROTO_STAGE})


    message("============= Building macoro =============")
    if(NOT EXISTS ${CLONE_DIR})
        run(NAME "Cloning ${GIT_REPOSITORY}" CMD ${DOWNLOAD_CMD} WD ${COPROTO_THIRDPARTY_CLONE_DIR})
    endif()

    #run(NAME "Checkout ${GIT_TAG} " CMD ${CHECKOUT_CMD}  WD ${CLONE_DIR})
    run(NAME "Configure"       CMD ${CONFIGURE_CMD} WD ${CLONE_DIR})
    run(NAME "Build"           CMD ${BUILD_CMD}     WD ${CLONE_DIR})
    run(NAME "Install"         CMD ${INSTALL_CMD}   WD ${CLONE_DIR})

    message("log ${LOG_FILE}\n==========================================")
else()
    message("macoro already fetched.")
endif()

install(CODE "
    if(NOT CMAKE_INSTALL_PREFIX STREQUAL \"${COPROTO_STAGE}\")
        execute_process(
            COMMAND ${SUDO} \${CMAKE_COMMAND} --install ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE} --prefix \${CMAKE_INSTALL_PREFIX}
            WORKING_DIRECTORY ${CLONE_DIR}
            RESULT_VARIABLE RESULT
            COMMAND_ECHO STDOUT
        )
    endif()
")
