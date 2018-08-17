add_executable( test_IasTestAvbStreamhandler
                private/tst/avb_streamhandler/src/IasTestAlsaEngine.cpp
                private/tst/avb_streamhandler/src/IasTestAlsaStream.cpp
                private/tst/avb_streamhandler/src/IasTestAlsaWorkerThread.cpp
                private/tst/avb_streamhandler/src/IasTestAvbAudioShmProvider.cpp
                private/tst/avb_streamhandler/src/IasTestAvbAlsaMain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbHwCaptureClockDomain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbPacket.cpp
                private/tst/avb_streamhandler/src/IasTestAvbPacketPool.cpp
                private/tst/avb_streamhandler/src/IasTestAvbPtpClockDomain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbReceiveEngine.cpp
                private/tst/avb_streamhandler/src/IasTestAvbRxStreamClockDomain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbStream.cpp
#                private/tst/avb_streamhandler/src/IasTestAvbStreamHandler.cpp
                private/tst/avb_streamhandler/src/IasTestAvbStreamHandlerEnvironment.cpp
                private/tst/avb_streamhandler/src/IasTestAvbStreamId.cpp
                private/tst/avb_streamhandler/src/IasTestAvbSwClockDomain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbTSpec.cpp
                private/tst/avb_streamhandler/src/IasTestAvbVideoStream.cpp
                private/tst/avb_streamhandler/src/IasTestDiaLogger.cpp
                private/tst/avb_streamhandler/src/IasTestLibPtpDaemon.cpp
                private/tst/avb_streamhandler/src/IasTestLocalAudioBuffer.cpp
                private/tst/avb_streamhandler/src/IasTestLocalAudioBufferDesc.cpp
                private/tst/avb_streamhandler/src/IasTestLocalVideoBuffer.cpp
#                private/tst/avb_streamhandler/src/IasTestLocalVideoStream.cpp
                private/tst/avb_streamhandler/src/IasTestTestToneStream.cpp
                private/tst/avb_streamhandler/src/IasTestTransmitEngine.cpp
                private/tst/avb_streamhandler/src/IasTestAvbTransmitSequencer.cpp
                private/tst/avb_streamhandler/src/IasTestAvbClockController.cpp
                private/tst/avb_streamhandler/src/IasTestAvbClockDomain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbClockReferenceStream.cpp
                private/tst/avb_streamhandler/src/IasTestAvbAudioStream.cpp
                private/tst/avb_streamhandler/src/IasTestAvbConfigurationBase.cpp
                private/tst/avb_streamhandler/src/IasTestAvbMain.cpp
                private/tst/avb_streamhandler/src/IasTestAvbClockDriver.cpp
                )

target_compile_options( test_IasTestAvbStreamhandler PRIVATE -Wno-error )

#find_package(PkgConfig)
#pkg_check_modules(DLT REQUIRED automotive-dlt)
#pkg_check_modules(DLT REQUIRED automotive-dlt-c++)

if(${SANITIZERS_ON})
  target_compile_options( test_IasTestAvbStreamhandler PUBLIC -fsanitize=address,undefined )
  target_link_libraries( test_IasTestAvbStreamhandler asan )
  target_link_libraries( test_IasTestAvbStreamhandler ubsan )
endif()
target_link_libraries( test_IasTestAvbStreamhandler dlt )
target_link_libraries( test_IasTestAvbStreamhandler ias-media_transport-avb_streamhandler )
target_link_libraries( test_IasTestAvbStreamhandler ias-media_transport-avb_watchdog )

target_link_libraries( test_IasTestAvbStreamhandler boost_system )
target_link_libraries( test_IasTestAvbStreamhandler boost_iostreams )

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
#  set_property(TARGET test_IasTestAvbStreamhandler APPEND_STRING PROPERTY LINK_FLAGS "-Wl,-wrap,${symbol} ")
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

target_link_libraries( test_IasTestAvbStreamhandler ${GTEST_LIBRARY} )
target_link_libraries( test_IasTestAvbStreamhandler ias-media_transport-test_common )
target_link_libraries( test_IasTestAvbStreamhandler ias-audio-common )
target_link_libraries( test_IasTestAvbStreamhandler pthread )
target_link_libraries( test_IasTestAvbStreamhandler ias-media_transport-avb_config_base )
target_link_libraries( test_IasTestAvbStreamhandler ias-media_transport-avb_clockdriver )
target_link_libraries( test_IasTestAvbStreamhandler stdc++fs )
#target_link_libraries( test_IasTestAvbStreamhandler test_wrapper )

# set capabilities
add_custom_command(TARGET test_IasTestAvbStreamhandler POST_BUILD
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/setcap.sh $<TARGET_FILE:test_IasTestAvbStreamhandler>
)

add_test(TestAvbStreamhandler test_IasTestAvbStreamhandler)
