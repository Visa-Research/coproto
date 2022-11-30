
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/coprotoFindDeps.cmake")


add_library(coproto STATIC 
    "Common/CLP.cpp"
    "Common/error_code.cpp"
    "Common/Util.cpp"
    "Socket/SocketScheduler.cpp"
    "Socket/AsioSocket.cpp"
)
target_include_directories(coproto PUBLIC 
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
                    $<INSTALL_INTERFACE:include>) 
target_include_directories(coproto PUBLIC 
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
                    $<INSTALL_INTERFACE:include>) 

if(NOT MSVC)
    target_compile_options(coproto PRIVATE "-Wfatal-errors")#
endif()

target_link_libraries(coproto Threads::Threads)


if(COPROTO_CPP20)
    if(MSVC)
        target_compile_options(coproto PUBLIC
            "/std:c++${COPROTO_CPP_VER}"
            "/fsanitize=address"
        )
    
    else()
        target_compile_options(coproto PUBLIC
            "-std=c++${COPROTO_CPP_VER}" "-fcoroutines"
        )
    endif()
else()
    if(APPLE)
        target_compile_options(coproto PUBLIC
            "-std=c++${COPROTO_CPP_VER}"
        )
    else()
        set_target_properties(coproto
            PROPERTIES
                CXX_STANDARD ${COPROTO_CPP_VER}
                CXX_STANDARD_REQUIRED YES
                CXX_EXTENSIONS NO
        )
    endif()
    target_link_libraries(coproto nonstd::span-lite)
endif()

if(COPROTO_ENABLE_BOOST)
    target_link_libraries(coproto Boost::boost Boost::system Boost::thread)
    
    if(COPROTO_ENABLE_OPENSSL)
        target_link_libraries(coproto OpenSSL::SSL)
    endif()

    target_compile_definitions(coproto PUBLIC "_WIN32_WINNT=0x601")
endif()

target_link_libraries(coproto function2::function2)
target_link_libraries(coproto macoro::macoro)
configure_file(config.h.in "config.h" )
configure_file("${CMAKE_CURRENT_LIST_DIR}/../cmake/preamble.cmake" "preamble.cmake" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../cmake/coprotoFindDeps.cmake" "coprotoFindDeps.cmake" COPYONLY)


#############################################
#            Install                        #
#############################################

# generate the config file that is includes the exports
configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/../cmake/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/coproto
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

if(NOT DEFINED coproto_VERSION_MAJOR)
    message("\n\n\n\n warning, coproto_VERSION_MAJOR not defined ${coproto_VERSION_MAJOR}")
endif()

set_property(TARGET coproto PROPERTY VERSION ${coproto_VERSION})

# generate the version file for the config file
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/../cmake/buildOptions.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/buildOptions.cmake")
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/../cmake/ConfigVersion.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfigVersion.cmake" COPYONLY)
#write_basic_package_version_file(
#  "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfigVersion.cmake"
#  VERSION "${coproto_VERSION_MAJOR}.${coproto_VERSION_MINOR}.${coproto_VERSION_PATCH}"
#  COMPATIBILITY AnyNewerVersion
#)

# install the configuration file
install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/buildOptions.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfigVersion.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/preamble.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/coprotoFindDeps.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/coproto
)

# install library
install(
    TARGETS coproto 
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    EXPORT coprotoTargets)

# install headers
install(DIRECTORY . DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/coproto FILES_MATCHING PATTERN "*.h")


#install config header
install( FILES
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/coproto")

# install config
install(EXPORT coprotoTargets
  FILE coprotoTargets.cmake
  NAMESPACE coproto::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/coproto
)
 export(EXPORT coprotoTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/coprotoTargets.cmake"
       NAMESPACE coproto::
)



#set(HDR 
#    "./Common/CLP.h"
#    "./Common/Defines.h"
#    "./Common/error_code.h"
#    "./Common/Function.h"
#    "./Common/InlinePoly.h"
#    "./Common/Name.h"
#    "./Common/Optional.h"
#    "./Common/Queue.h"
#    "./Common/Result.h"
#    "./Common/span.h"
#    "./Common/TypeTraits.h"
#    "./Common/Util.h"
#    "./Common/Variant.h"
#    "./Evaluator/EvalResult.h"
#    "./Evaluator/Evaluate.h"
#    "./Evaluator/LocalEvaluator.h"
#    "./Executor/Executor.h"
#    "./Executor/Frame.h"
#    "./Executor/MTExecutor.h"
#    "./Executor/Scheduler.h"
#    "./Socket/SocketScheduler.h"
#    "./Executor/Stack.h"
#    "./Executor/STExecutor.h"
#    "./Executor/Strand.h"
#    "./Executor/Token.h"
#    "./Proto/Async.h"
#    "./Proto/Buffers.h"
#    "./Proto/CoroProto.h"
#    "./Proto/EndOfRound.h"
#    "./Proto/Future.h"
#    "./Proto/LambdaProto.h"
#    "./Proto/LambdaProtoMacro.h"
#    "./Proto/Proto.h"
#    "./Proto/Resume.h"
#)
#