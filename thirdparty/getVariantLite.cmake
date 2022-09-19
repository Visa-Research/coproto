
set(DEP_NAME            variant-lite)
set(GIT_REPOSITORY      https://github.com/martinmoene/variant-lite.git)
set(GIT_TAG             "9499655b9c263eaef735efeeb53892c770d447e1" )

set(CLONE_DIR "${CMAKE_CURRENT_LIST_DIR}/${DEP_NAME}")
set(BUILD_DIR "${CLONE_DIR}/build/${COPROTO_CONFIG}")
set(LOG_FILE  "${CMAKE_CURRENT_LIST_DIR}/log-${DEP_NAME}.txt")


include("${CMAKE_CURRENT_LIST_DIR}/fetch.cmake")

if(NOT variant-lite_FOUND)
    find_program(GIT git REQUIRED)
    set(DOWNLOAD_CMD  ${GIT} clone ${GIT_REPOSITORY})
    set(CHECKOUT_CMD  ${GIT} checkout ${GIT_TAG})
    set(CONFIGURE_CMD ${CMAKE_COMMAND} -S ${CLONE_DIR} -B ${BUILD_DIR} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                       -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE} -DVARIANT_LITE_OPT_BUILD_TESTS=OFF)
    set(BUILD_CMD     ${CMAKE_COMMAND} --build ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE})
    set(INSTALL_CMD   ${CMAKE_COMMAND} --install ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE} --prefix ${COPROTO_STAGE})


    message("============= Building ${DEP_NAME} =============")
    if(NOT EXISTS ${CLONE_DIR})
        run(NAME "Cloning ${GIT_REPOSITORY}" CMD ${DOWNLOAD_CMD} WD ${CMAKE_CURRENT_LIST_DIR})
    endif()

    run(NAME "Checkout ${GIT_TAG} " CMD ${CHECKOUT_CMD}  WD ${CLONE_DIR})
    run(NAME "Configure"       CMD ${CONFIGURE_CMD} WD ${CLONE_DIR})
    run(NAME "Build"           CMD ${BUILD_CMD}     WD ${CLONE_DIR})
    run(NAME "Install"         CMD ${INSTALL_CMD}   WD ${CLONE_DIR})

    message("log ${LOG_FILE}\n==========================================")
else()
    message("${DEP_NAME} already fetched.")
endif()

install(CODE "
    execute_process(
        COMMAND ${SUDO} \${CMAKE_COMMAND} --install ${BUILD_DIR} --config ${CMAKE_BUILD_TYPE} --prefix \${CMAKE_INSTALL_PREFIX}
        WORKING_DIRECTORY ${CLONE_DIR}
        RESULT_VARIABLE RESULT
        COMMAND_ECHO STDOUT
    )
")



