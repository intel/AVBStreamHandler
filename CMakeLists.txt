#
# Copyright (C) 2018 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
CMAKE_MINIMUM_REQUIRED( VERSION 3.4.3 )
project (MediaTransportAvbStreamhandler)

find_package(PkgConfig)

pkg_check_modules(ASOUND REQUIRED alsa)
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(DLT REQUIRED automotive-dlt)
pkg_check_modules(DLT REQUIRED automotive-dlt-c++)

add_subdirectory( deps/audio/common )

string(REPLACE " -O3" " -O2" GPTP_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(APPEND GPTP_CXX_FLAGS " -fno-tree-vectorize")

#-----------------------------------------------------------
# build igb_avb and gptp libraries and driver before searching for libraries
#-----------------------------------------------------------
execute_process( COMMAND make lib CFLAGS=-fPIC
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/igb_avb
)
execute_process( COMMAND make kmod
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/igb_avb
)

execute_process( COMMAND make CXXFLAGS=${GPTP_CXX_FLAGS}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/deps/gptp/linux/build
)

# set release version for avb stream handler
set( MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MAJOR    1 )
set( MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MINOR    0 )
set( MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_REVISION 5 )

set( MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_STRING ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MAJOR}.${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MINOR}.${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_REVISION})

# create a version.h file
set(CURRENT_VERSION_MAJOR ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MAJOR})
set(CURRENT_VERSION_MINOR ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MINOR})
set(CURRENT_VERSION_REVISION ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_REVISION})
set(VERSION_H_INCLUDE_GUARD "MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_H")
string(TOUPPER ${VERSION_H_INCLUDE_GUARD} VERSION_H_INCLUDE_GUARD)
string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" VERSION_H_INCLUDE_GUARD ${VERSION_H_INCLUDE_GUARD})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/private/inc/avb_helper/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)
unset(CURRENT_VERSION_MAJOR)
unset(CURRENT_VERSION_MINOR)
unset(CURRENT_VERSION_REVISION)
unset(VERSION_H_INCLUDE_GUARD)

set(CMAKE_BUILD_TYPE Debug)

#uncomment the following line to enable direct DMA for RX
set( DIRECT_RX_DMA 1 CACHE STRING "global rx mode switch")

#uncomment the following line to enable features that are
#  only available for "pre-production" releases.
set( IAS_PREPRODUCTION_SW 1 CACHE STRING "pre-production mode switch")

#uncomment the following line to enable performance measurement features
#set( PERFORMANCE_MEASUREMENT 1 CACHE STRING "performance measurement features switch")

# use compiler flags being using in GP1.x:
SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pipe -g -fstack-protector-all -pie -fpie -D_FORTIFY_SOURCE=2 -fvisibility-inlines-hidden -DNDEBUG -fexceptions -fstrict-aliasing -Wall -Wextra -Wformat -Wformat-security -Wconversion -Werror -fasynchronous-unwind-tables -fno-omit-frame-pointer -std=c++11" )

if (${IAS_IS_HOST_BUILD})
  #disable position-independent executable (PIE) as gtest does not handle it correctly
  string(REPLACE " -pie" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE " -fpie" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

#------------------------------------------------------------------
# avb_streamhandler main CMakeLists
#------------------------------------------------------------------
include(GNUInstallDirs)
include( CMakeLists.lib_ptp_daemon.txt )
include( CMakeLists.avb_helper.txt )
include( CMakeLists.avb_streamhandler.txt )
include( CMakeLists.avb_configuration.txt )
include( CMakeLists.avb_configuration_reference.txt )
include( CMakeLists.avb_configuration_example.txt )
include( CMakeLists.avb_clockdriver.txt )
include( CMakeLists.avb_streamhandler_demo.txt )
include( CMakeLists.avb_video_bridge.txt )
include( CMakeLists.avb_video_debug_app.txt )
include( CMakeLists.avb_watchdog.txt )

#------------------------------------------------------------------
#Include makefile building internal tools (only on host / not needed on target)
#------------------------------------------------------------------
if (${IAS_IS_HOST_BUILD})
  include( CMakeLists.setcap_tool.txt )
  include( CMakeLists.setsched_tool.txt )
  include( CMakeLists.test_wrapper.txt )
  include( CMakeLists.test_common.txt )
endif()

#------------------------------------------------------------------
#Build the tests (only on host / not needed on target)
#------------------------------------------------------------------
if (${IAS_IS_HOST_BUILD})
  include(CMakeLists.avb_tests.txt)
endif()

#------------------------------------------------------------------
#Install
#------------------------------------------------------------------
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/deps/gptp/linux/build/obj/daemon_cl DESTINATION ${CMAKE_INSTALL_BINDIR})
install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/setcap.sh ${CMAKE_INSTALL_FULL_BINDIR}/daemon_cl)")

set(INSTALL_LIBDIR ${CMAKE_INSTALL_FULL_LIBDIR}/alsa-lib)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/public/res/51-smartx.conf.in ${CMAKE_CURRENT_SOURCE_DIR}/public/res/51-smartx.conf @ONLY)

if(ASOUND_VERSION VERSION_LESS 1.1.7)
        set(ALSA_CONF_PATH "/usr/share/alsa/alsa.conf.d")
else()
        install(DIRECTORY DESTINATION /etc/alsa/conf.d)
        set(ALSA_CONF_PATH "/etc/alsa/conf.d")
endif()

install(FILES  ${CMAKE_CURRENT_SOURCE_DIR}/public/res/51-smartx.conf DESTINATION ${ALSA_CONF_PATH})


#------------------------------------------------------------------
# create documentation for the whole stream handler here, referencing all relevant entities
#------------------------------------------------------------------
#include(doxygen/ias_defines_doxygen.cmake)

#TODO Vince add doxygen for watchdog
#IasCreateComponentDocumentation( media_transport avb_streamhandler ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MAJOR}
#                                                                   ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MINOR}
#                                                                   ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_REVISION}
#  avb_configuration_reference
#  avb_clockdriver
#  avb_streamhandler
#  avb_streamhandler_api
#)
#IasCreateDoxygenTarget()
#------------------------------------------------------------------
#IasCreateBbRecipeFile( media_transport ias-mediatransport-avbstreamhandler ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MAJOR}
#                                                                            ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_MINOR}
#                                                                            ${MEDIA_TRANSPORT_AVB_STREAMHANDLER_VERSION_REVISION}
#  SUMMARY  "Media Transport Subsystem components"
#  DESCRIPTION "Media Transport Subsystem"
#  GROUP "Media/Audio"
#  YOCTO_LAYER meta-ias-mediatransport
#  YOCTO_RECIPE_FOLDER recipes-connectivity/avbstreamhandler
#  PACKAGE  ias-mediatransport-avbconfiguration-reference
#    PACKAGE_SUMMARY "IAS AVB reference configuration"
#    PACKAGE_ENTITIES avb_configuration_reference
#  PACKAGE  ias-mediatransport-avbconfiguration-example
#    PACKAGE_SUMMARY "IAS AVB example configuration"
#    PACKAGE_ENTITIES avb_configuration_example
#  PACKAGE  ias-mediatransport-avbclockdriver
#    PACKAGE_SUMMARY "IAS AVB clock driver for IVI-BRD 2"
#    PACKAGE_ENTITIES avb_clockdriver
#  PACKAGE  ias-mediatransport-avbstreamhandler-common
#    PACKAGE_SUMMARY "IAS AVB stream handler common parts"
#    PACKAGE_ENTITIES avb_streamhandler
#    PACKAGE_ENTITIES avb_config_base
#    PACKAGE_ENTITIES setsched_tool
#  PACKAGE  ias-mediatransport-avbstreamhandler-demo
#    PACKAGE_SUMMARY "IAS AVB stream handler demo application"
#    PACKAGE_ENTITIES avb_streamhandler_demo
#    PACKAGE_BUILD_REQUIRES open-avb
#    PACKAGE_BUILD_REQUIRES alsa-lib
#    PACKAGE_BUILD_REQUIRES dlt-daemon
#)
