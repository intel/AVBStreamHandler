add_executable( test_IasTestAvbHelper
                private/tst/avb_helper/src/IasTestResult.cpp
                private/tst/avb_helper/src/IasTestThread.cpp
                private/tst/avb_helper/src/main.cpp
                private/tst/avb_helper/src/IasTestAvbMain.cpp
                )

target_compile_options( test_IasTestAvbHelper PRIVATE -Wno-error )

#find_package(PkgConfig)
#pkg_check_modules(DLT REQUIRED automotive-dlt)
#pkg_check_modules(DLT REQUIRED automotive-dlt-c++)

if(${SANITIZERS_ON})
  target_compile_options( test_IasTestAvbHelper PUBLIC -fsanitize=address,undefined )
  target_link_libraries( test_IasTestAvbHelper asan )
  target_link_libraries( test_IasTestAvbHelper ubsan )
endif()
target_link_libraries( test_IasTestAvbHelper dlt )
target_link_libraries( test_IasTestAvbHelper ias-media_transport-avb_streamhandler )

target_link_libraries( test_IasTestAvbHelper ias-media_transport-avb_watchdog )
### IasAddPrivateCXXFlags( "-fno-access-control" )

  # uncomment next line to enable verbose logging
#   IasAddPrivateCompileFlags( "-DVERBOSE_TEST_PRINTOUT=1" )

###  IasUseEntity( core_libraries foundation )
###  IasUseEntity( core_libraries test_support )

###  IasUseEntity( media_transport lib_ptp_daemon )
find_library( IGB_LIB "libigb.a" PATHS "${CMAKE_CURRENT_SOURCE_DIR}/deps/OpenAvb/lib/igb")
find_library( GTEST_LIBRARY "libgtest.a" PATHS "/usr/lib/" )

###  IasUseEntity( core_libraries test_wrapper )
###  IasUseEntity( media_transport test_wrapper )
###  IasUseEntity( media_transport test_common )

###  IasEnableTestWrapper(shm_open)
###  IasEnableTestWrapper(pthread_mutexattr_init pthread_mutexattr_setpshared pthread_mutex_init pthread_mutex_lock pthread_mutex_unlock)
#include_directories("test_wrapper")
#foreach(symbol pthread_mutexattr_init pthread_mutexattr_setpshared pthread_mutex_init pthread_mutex_lock pthread_mutex_unlock)
#  set_property(TARGET test_IasTestAvbAlsa APPEND_STRING PROPERTY LINK_FLAGS "-Wl,-wrap,${symbol} ")
#  message("Symbol wrapped: ${symbol} ")
#endforeach()
###  IasEnableTestWrapper(pthread_condattr_init pthread_condattr_setpshared pthread_cond_init pthread_cond_wait)
###  IasEnableTestWrapper(mmap)
###  IasEnableTestWrapper(pthread_create)

###  IasFindPackage(COMMONAPI_LIBS COMMONAPI_INCLUDE_DIRS CommonAPI CONFIG REQUIRED)

###  IasFindPath( ASOUND_INCLUDE "sound/asound.h" )
###  IasFindLibrary( ASOUND_LIB asound )
###  IasFindLibrary(RTLIB rt)
###  IasFindLibrary(DLLIB dl)
###  IasFindLibrary( SYSTEMD_DAEMON_LIB systemd-daemon ) # for watchdog

enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

target_link_libraries( test_IasTestAvbHelper ${GTEST_LIBRARY} )
target_link_libraries( test_IasTestAvbHelper ias-media_transport-test_common )
target_link_libraries( test_IasTestAvbHelper ias-audio-common )
target_link_libraries( test_IasTestAvbHelper pthread )
target_link_libraries( test_IasTestAvbHelper stdc++fs )
#target_link_libraries( test_IasTestAvbAlsa test_wrapper )

# set capabilities
add_custom_command(TARGET test_IasTestAvbHelper POST_BUILD
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/setcap.sh $<TARGET_FILE:test_IasTestAvbHelper>
)

add_test(TestAvbHelper test_IasTestAvbHelper)
