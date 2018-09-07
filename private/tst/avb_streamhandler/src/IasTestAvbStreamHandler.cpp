/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
* @file IasTestAvbStreamHandler.cpp
* @brief Test file for IasAvbStreamHandler
* @date 2013
*/

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbReceiveEngine.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"

#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbSwClockDomain.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#include "avb_streamhandler/IasDiaLogger.hpp"
#include "avb_streamhandler/IasAvbDiagnosticPacket.hpp"
#include "avb_streamhandler/IasAvbTransmitEngine.hpp"
#include "avb_streamhandler/IasAvbAudioStream.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include "avb_streamhandler/IasTestToneStream.hpp"
#include "avb_streamhandler/IasAlsaEngine.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"

extern std::string cIasChannelNamePolicyTest;
extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;
using AvbStreamId = IasAvbStreamHandlerInterface::AvbStreamId;
using MacAddress  = IasAvbStreamHandlerInterface::MacAddress;

class IasTestAvbStreamHandler : public ::testing::Test
{
protected:
  IasTestAvbStreamHandler():
    mIasAvbStreamHandler(0),
    mAlsaAudioFormat(IasAvbAudioFormat::eIasAvbAudioFormatSaf16),
    mAlsaAudioFormatNotSupported(IasAvbAudioFormat::eIasAvbAudioFormatSafFloat)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
//    dlt_enable_local_print();
  }

  virtual ~IasTestAvbStreamHandler()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;
#if VERBOSE_TEST_PRINTOUT
    mIasAvbStreamHandler = new IasAvbStreamHandler(DLT_LOG_INFO);
#else
    mIasAvbStreamHandler = new IasAvbStreamHandler(DLT_LOG_ERROR);
#endif
    ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  }

  virtual void TearDown()
  {
    mIasAvbStreamHandler->cleanup();
    delete mIasAvbStreamHandler;
    mIasAvbStreamHandler = NULL;
    heapSpaceLeft = heapSpaceInitSize;
  }

  IasAvbProcessingResult initAvbStreamHandler(bool runSetup = true, bool clkRecovery = false, const char * profile = "UnitTests");
  IasAvbResult setConfigValue(const std::string& key, const std::string& value);
  IasAvbResult setConfigValue(const std::string& key, const uint64_t& value);

  class IasAvbStreamHandlerClientInterfaceImpl : public IasAvbStreamHandlerClientInterface
  {
  public:
    IasAvbStreamHandlerClientInterfaceImpl(){}
    virtual ~IasAvbStreamHandlerClientInterfaceImpl(){}

    void updateStreamStatus( uint64_t streamId, IasAvbStreamState status )
    {
      (void)streamId;
      (void)status;
    }

    void updateLinkStatus( bool ifUp )
    {
      (void)ifUp;
    }
  };

  class IasAvbStreamHandlerControllerInterfaceImpl : public IasAvbStreamHandlerControllerInterface
  {
  public:
    IasAvbStreamHandlerControllerInterfaceImpl(){}
    virtual ~IasAvbStreamHandlerControllerInterfaceImpl(){}

    IasAvbResult init(IasAvbStreamHandler *api)
    {
      (void)api;
      IasAvbResult ok = eIasAvbResultOk;
      return ok;
    }

    IasAvbResult cleanup()
    {
      IasAvbResult ok = eIasAvbResultOk;
      return ok;
    }

    IasAvbResult registerService(std::string const instanceName)
    {
      (void)instanceName;
      IasAvbResult ok = eIasAvbResultOk;
      return ok;
    }

    IasAvbResult unregisterService()
    {
      IasAvbResult ok = eIasAvbResultOk;
      return ok;
    }
  };


protected:
  // members
  IasAvbStreamHandler* mIasAvbStreamHandler;
  IasAvbAudioFormat mAlsaAudioFormat;
  IasAvbAudioFormat mAlsaAudioFormatNotSupported;
};

IasAvbProcessingResult IasTestAvbStreamHandler::initAvbStreamHandler(bool runSetup, bool clkRecovery, const char * profile)
{
  // IT HAS TO BE SET TO ZERO - getopt_long - DefaultConfig_passArguments - needs to be reinitialized before use
  optind = 0;

  IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", profile,
    "-n", IasSpringVilleInfo::getInterfaceName(),
    "-e", "libias-media_transport-avb_clockdriver.so"
  };

  int argc = static_cast<int>(sizeof(args) / sizeof(args[0]));
  argc = clkRecovery ? argc : argc - 2;

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mIasAvbStreamHandler->init(theConfigPlugin, runSetup, argc, (char**)args);
  return result;
}

IasAvbResult IasTestAvbStreamHandler::setConfigValue(const std::string& key, const std::string& value)
{
  IasAvbResult result;
  IAS_ASSERT(IasAvbStreamHandlerEnvironment::mInstance);
  IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
  result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
  IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
  return result;
}

IasAvbResult IasTestAvbStreamHandler::setConfigValue(const std::string& key, const uint64_t& value)
{
  IasAvbResult result;
  IAS_ASSERT(IasAvbStreamHandlerEnvironment::mInstance);
  IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
  result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
  IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
  return result;
}

TEST_F(IasTestAvbStreamHandler, init)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  bool runSetup = false;
  int32_t setupArgc = 0;
  char** setupArgv = NULL;
  ASSERT_EQ(eIasAvbProcInitializationFailed, mIasAvbStreamHandler->init(theConfigPlugin, runSetup, setupArgc, setupArgv));

  runSetup = true;
  ASSERT_EQ(eIasAvbProcInitializationFailed, mIasAvbStreamHandler->init(theConfigPlugin, runSetup, setupArgc, setupArgv));

  setupArgc = 1;
  ASSERT_EQ(eIasAvbProcInitializationFailed, mIasAvbStreamHandler->init(theConfigPlugin, runSetup, setupArgc, setupArgv));

  ASSERT_EQ(eIasAvbProcInvalidParam, mIasAvbStreamHandler->init("", runSetup, setupArgc, setupArgv));
}

TEST_F(IasTestAvbStreamHandler, init_ptpLoopCount)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  optind = 0;

  IasSpringVilleInfo::fetchData();

#ifdef IAS_HOST_BUILD
  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", "UnitTests",
    "-n", IasSpringVilleInfo::getInterfaceName(),
    "-e", "libias-media_transport-avb_clockdriver.so",
    "-k", "clock.hwcapture.nominal=93750",
    "-k", "ptp.loopcount=1"
  };
#else
  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", "UnitTests",
    "-b", IasSpringVilleInfo::getBusId(),
    "-d", IasSpringVilleInfo::getDeviceId(),
    "-n", IasSpringVilleInfo::getInterfaceName(),
    "-e", "libias-media_transport-avb_clockdriver.so"
  };
#endif

  int argc = static_cast<int>(sizeof(args) / sizeof(args[0]));

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->init(theConfigPlugin, false, argc, (char**)args));
}

TEST_F(IasTestAvbStreamHandler, init_heap)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  optind = 0;

  IasSpringVilleInfo::fetchData();

#ifdef IAS_HOST_BUILD
  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", "UnitTests",
    "-n", IasSpringVilleInfo::getInterfaceName(),
    "-e", "libias-media_transport-avb_clockdriver.so",
    "-k", "clock.hwcapture.nominal=93750",
    "-k", "ptp.loopcount=1"
  };
#else
  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", "UnitTests",
    "-b", IasSpringVilleInfo::getBusId(),
    "-d", IasSpringVilleInfo::getDeviceId(),
    "-n", IasSpringVilleInfo::getInterfaceName(),
    "-e", "libias-media_transport-avb_clockdriver.so"
  };
#endif

  int argc = static_cast<int>(sizeof(args) / sizeof(args[0]));

  size_t numDltContexts = 19;
  size_t heap = sizeof (IasAvbStreamHandlerEnvironment) + sizeof (device_t) + sizeof (IasDiaLogger)
                  + sizeof (IasAvbDiagnosticPacket) + IasAvbDiagnosticPacket::cPacketLength
                  + (numDltContexts * sizeof (DltContext)) - 1 + sizeof (IasAvbSwClockDomain)
                  + sizeof (IasAvbPtpClockDomain) + sizeof (IasAvbHwCaptureClockDomain) + sizeof (IasLibPtpDaemon);
  heapSpaceLeft = heap;//2385;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->init(theConfigPlugin, false, argc, (char**)args));
}

TEST_F(IasTestAvbStreamHandler, start)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcNotInitialized, mIasAvbStreamHandler->start());

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasThread * pRxEngineRxThread                      = mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread;
  mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread = NULL;

  ASSERT_EQ(eIasAvbProcNullPointerAccess, mIasAvbStreamHandler->start());

  mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread = pRxEngineRxThread;

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());
}

TEST_F(IasTestAvbStreamHandler, start_wd_branch)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcNotInitialized, mIasAvbStreamHandler->start());

  bool wdEnvSet = false;
  if (0 == setenv("WATCHDOG_USEC","1000000", false))
    wdEnvSet = true;
  // dummy packets
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(true,true));

  IasThread * pRxEngineRxThread                      = mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread;
  mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread = NULL;

  ASSERT_EQ(eIasAvbProcNullPointerAccess, mIasAvbStreamHandler->start());

  mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread = pRxEngineRxThread;

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());

  if (wdEnvSet)
    unsetenv("WATCHDOG_USEC");
}

TEST_F(IasTestAvbStreamHandler, start_wd_branch2)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcNotInitialized, mIasAvbStreamHandler->start());

  bool wdEnvSet = false;
  if (0 == setenv("WATCHDOG_USEC","1000000", false))
    wdEnvSet = true;
  // xmit successful
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(true, true, "UnitTests"));

  IasThread * pRxEngineRxThread                      = mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread;
  mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread = NULL;

  ASSERT_EQ(eIasAvbProcNullPointerAccess, mIasAvbStreamHandler->start());

  mIasAvbStreamHandler->mAvbReceiveEngine->mReceiveThread = pRxEngineRxThread;

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());

  if (wdEnvSet)
    unsetenv("WATCHDOG_USEC");
}

TEST_F(IasTestAvbStreamHandler, start_resume)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  bool resume = false;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start(resume));
  usleep(500000);

  bool suspend = true;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop(suspend));
  usleep(500000);

  resume = true;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start(resume));
}

TEST_F(IasTestAvbStreamHandler, start_resume_clockDriver)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool runSetup   = true;
  bool loadDriver = true;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(runSetup, loadDriver));
  bool resume = false;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start(resume));
  usleep(500000);

  bool suspend = true;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop(suspend));
  usleep(500000);

  resume = true;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start(resume));
}

TEST_F(IasTestAvbStreamHandler, start_resume_no_ptp)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  bool suspend = true;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop(suspend));
  usleep(10);

  bool resume = true;
  delete mIasAvbStreamHandler->mEnvironment->mPtpProxy;
  mIasAvbStreamHandler->mEnvironment->mPtpProxy = NULL;
  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandler->start(resume));
}

TEST_F(IasTestAvbStreamHandler, stop)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcNotInitialized, mIasAvbStreamHandler->stop());

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  bool suspend = false;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop(suspend));
}

TEST_F(IasTestAvbStreamHandler, stop_noPtp)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool runSetup   = true;
  bool loadDriver = true;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(runSetup, loadDriver));

  bool resume = false;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start(resume));
  usleep(500000);

  delete mIasAvbStreamHandler->mEnvironment->mPtpProxy;
  mIasAvbStreamHandler->mEnvironment->mPtpProxy = NULL;
  bool suspend = true;
  // NULL != ptp                                                   (F)
  // NULL != driver                                                (T)
  // IasAvbResult::eIasAvbResultOk != driver->init(*mEnvironment)  (F)
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop(suspend));
}

TEST_F(IasTestAvbStreamHandler, createReceiveAudioStream_NoInit)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  AvbStreamId streamId = 0x91E0F000FE000001u;
  MacAddress destMacAddr = 0x91E0F000FE01;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));
}

TEST_F(IasTestAvbStreamHandler, createReceiveAudioStream_NoMem)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(false));

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  AvbStreamId streamId = 0x91E0F000FE000001u;
  MacAddress destMacAddr = 0x91E0F000FE01;
  heapSpaceLeft = sizeof(IasAvbReceiveEngine) - 1;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));

  heapSpaceLeft = sizeof(IasAvbReceiveEngine) + sizeof(IasThread) - 1;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));

  heapSpaceLeft = heapSpaceInitSize;
}

TEST_F(IasTestAvbStreamHandler, createReceiveAudioStream_maxNumChannels)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());

  uint16_t maxNumberChannels = cIasAvbMaxNumChannels + 1u;
  uint32_t sampleFreq = 48000;
  AvbStreamId streamId = 0x91E0F000FE000001u;
  MacAddress destMacAddr = 0x91E0F000FE01;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));
}

TEST_F(IasTestAvbStreamHandler, createReceiveAudioStream_start)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  AvbStreamId streamId = 0x91E0F000FE000001u;
  MacAddress destMacAddr = 0x91E0F000FE01;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));
}

TEST_F(IasTestAvbStreamHandler, createReceiveAudioStream_clockRecovery_noDriver)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup  = false;
  bool noDriver = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup, noDriver));

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  AvbStreamId streamId = 0x91E0F000FE000001u;
  MacAddress destMacAddr = 0x91E0F000FE01;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClkRecoverFrom, streamId));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));
}

TEST_F(IasTestAvbStreamHandler, createReceiveAudioStream_clockRecovery)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool runSetup    = true;
  bool clkRecovery = true;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(runSetup, clkRecovery));
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  AvbStreamId streamId = 0x91E0F000FE000001u;
  MacAddress destMacAddr = 0x91E0F000FE01;
  uint32_t clockId = 0;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClkRecoverFrom, streamId));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClkRecoverUsing, clockId));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq, streamId, destMacAddr));
}

TEST_F(IasTestAvbStreamHandler, createTransmitAudioStream_NoInit)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  uint16_t maxNumberChannels    = 2;
  uint32_t sampleFreq           = 48000;
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t clockId              = 0u;
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  AvbStreamId streamId          = 0x91E0F000FE000001u;
  MacAddress destMacAddr        = 0x91E0F000FE01;
  bool active                   = true;

  IasAvbResult result = mIasAvbStreamHandler->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                                        sampleFreq,
                                                                        format,
                                                                        clockId,
                                                                        assignMode,
                                                                        streamId,
                                                                        destMacAddr,
                                                                        active);

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, result);
}

TEST_F(IasTestAvbStreamHandler, destroyStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbSrClass srClass         = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels    = 2u;
  uint32_t sampleFreq           = 48000u;
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t clockId              = 0u;
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  AvbStreamId streamId          = 0u;
  MacAddress destMacAddr        = 0x91E0F000FE01;
  bool active                   = true;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTransmitAudioStream(srClass,
                                                                                           maxNumberChannels,
                                                                                           sampleFreq,
                                                                                           format,
                                                                                           clockId,
                                                                                           assignMode,
                                                                                           streamId,
                                                                                           destMacAddr,
                                                                                           active));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyStream(streamId));
}

TEST_F(IasTestAvbStreamHandler, destroyStream_noSetup)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  AvbStreamId streamId = 0x91E0F000FE000001u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->destroyStream(streamId));

  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->destroyStream(streamId));

  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq        = 48000u;
  AvbStreamId rxStreamId     = 0x91E0F000FE000001u;
  MacAddress destMacAddr     = 0x91E0F000FE01;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                                                          sampleFreq,
                                                                                          rxStreamId,
                                                                                          destMacAddr));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyStream(rxStreamId));
}

// TODO: Enable test when video for KSL will be implemented
TEST_F(IasTestAvbStreamHandler, DISABLED_connectStreams)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  AvbStreamId networkStreamId = 0x91E0F000FE000001u;
  uint16_t localStreamId = 1u;
  // not initialized
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->connectStreams(networkStreamId, localStreamId));

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  localStreamId = mIasAvbStreamHandler->getNextLocalStreamId();
  // NULL == localAudioStream && NULL == localVideoStream                                 (T && T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->connectStreams(networkStreamId, localStreamId));

  localStreamId                            = 3u; // default config tx stream id
  IasAvbTransmitEngine * pTxEngine         = mIasAvbStreamHandler->mAvbTransmitEngine;
  mIasAvbStreamHandler->mAvbTransmitEngine = NULL;
  // NULL == localAudioStream && NULL == localVideoStream                                 (F && N/A)
  // localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork  (T)
  // NULL != mAvbTransmitEngine                                                           (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->connectStreams(networkStreamId, localStreamId));

  mIasAvbStreamHandler->mAvbTransmitEngine = pTxEngine;
  localStreamId                            = 4u; // default config rx stream id
  IasAvbReceiveEngine * pRxEngine          = mIasAvbStreamHandler->mAvbReceiveEngine;
  mIasAvbStreamHandler->mAvbReceiveEngine  = NULL;
  // NULL == localAudioStream && NULL == localVideoStream                                 (F && N/A)
  // localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork  (F)
  // NULL != mAvbReceiveEngine                                                            (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->connectStreams(networkStreamId, localStreamId));

  mIasAvbStreamHandler->mAvbReceiveEngine  = pRxEngine;

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t maxPacketRate = 42u;
  uint16_t maxPacketSize = 1024u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  uint32_t clockId = 0u;
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  uint64_t txStreamId(0u), rxStreamId(1u);
  localStreamId = 2u;
  uint64_t dmac = 0u;
  bool active = false;
  string ipcName = "test";

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTransmitVideoStream(srClass,
                                                                                           maxPacketRate,
                                                                                           maxPacketSize,
                                                                                           format,
                                                                                           clockId,
                                                                                           assignMode,
                                                                                           txStreamId,
                                                                                           dmac,
                                                                                           active));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                        maxPacketRate,
                                                                                        maxPacketSize,
                                                                                        format,
                                                                                        ipcName,
                                                                                        localStreamId));
  pTxEngine = mIasAvbStreamHandler->mAvbTransmitEngine;
  mIasAvbStreamHandler->mAvbTransmitEngine = NULL;
  // NULL == localAudioStream && NULL == localVideoStream                                 (T && F)
  // localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork  (T)
  // NULL != mAvbTransmitEngine                                                           (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->connectStreams(txStreamId, localStreamId));

  mIasAvbStreamHandler->mAvbTransmitEngine = pTxEngine;
  // NULL == localAudioStream && NULL == localVideoStream                                 (T && F)
  // localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork  (T)
  // NULL != mAvbTransmitEngine                                                           (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->connectStreams(txStreamId, localStreamId));
  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  localStreamId = 1u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                        maxPacketRate,
                                                                                        maxPacketSize,
                                                                                        format,
                                                                                        ipcName,
                                                                                        localStreamId));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveVideoStream(srClass,
                                                                                          maxPacketRate,
                                                                                          maxPacketSize,
                                                                                          format,
                                                                                          rxStreamId,
                                                                                          dmac));
  // NULL == localAudioStream && NULL == localVideoStream                                 (T && F)
  // localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork  (F)
  // NULL != mAvbReceiveEngine                                                            (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->connectStreams(rxStreamId, localStreamId));

  delete mIasAvbStreamHandler;
  mIasAvbStreamHandler = new IasAvbStreamHandler(DLT_LOG_INFO);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  localStreamId = 0u;
  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                        maxPacketRate,
                                                                                        maxPacketSize,
                                                                                        format,
                                                                                        ipcName,
                                                                                        localStreamId));
  // NULL == localAudioStream && NULL == localVideoStream                                 (T && F)
  // localVideoStream->getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork  (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->connectStreams(txStreamId, localStreamId));
}

TEST_F(IasTestAvbStreamHandler, disconnectStreams)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  AvbStreamId networkStreamId = 0x91E0F000FE000001u;

  bool runSetup = true;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(runSetup));
  // one of the default tx streams created at setup
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->disconnectStreams(networkStreamId));

  networkStreamId = 0u;
  // one of the default rx streams created at setup
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->disconnectStreams(networkStreamId));

  networkStreamId = 1u;
  // invalid stream id
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->disconnectStreams(networkStreamId));
}

TEST_F(IasTestAvbStreamHandler, disconnectStreams_noSetup)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  AvbStreamId networkStreamId = 0x91E0F000FE000001u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->disconnectStreams(networkStreamId));

  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->disconnectStreams(networkStreamId));
}

TEST_F(IasTestAvbStreamHandler, setChannelLayout)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  uint16_t localStreamId = 0u;
  uint8_t channelLayout  = 0u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setChannelLayout(localStreamId, channelLayout));

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, mIasAvbStreamHandler->setChannelLayout(localStreamId, channelLayout));

  IasAvbStreamDirection direction    = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numChannels               = 2u;
  bool hasSideChannel                = false;
  string name                        = "test_";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numChannels,
                                                                                  48000u,
                                                                                  mAlsaAudioFormat,
                                                                                  cIasAvbPtpClockDomainId,
                                                                                  256u,
                                                                                  3u,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  name,
                                                                                  localStreamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->setChannelLayout(localStreamId, channelLayout));
}

TEST_F(IasTestAvbStreamHandler, getAvbStreamInfo_resetCount)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  uint16_t localStreamId = 0u;
  uint8_t channelLayout  = 0u;

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numChannels              = 2u;
  bool hasSideChannel             = false;
  string name               = "test_";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numChannels,
                                                                                  48000u,
                                                                                  mAlsaAudioFormat,
                                                                                  cIasAvbPtpClockDomainId,
                                                                                  256u,
                                                                                  3u,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  name,
                                                                                  localStreamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));
  AudioStreamInfoList audioStreamInfoList;
  VideoStreamInfoList videoStreamInfoList;
  ClockReferenceStreamInfoList crStreamInfoList;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->getAvbStreamInfo(audioStreamInfoList, videoStreamInfoList, crStreamInfoList));
  ASSERT_FALSE(audioStreamInfoList.empty());
  ASSERT_EQ(0u, audioStreamInfoList[0].getDiagnostics().getResetCount());

  IasAvbStreamDiagnostics diag;
  diag.setResetCount(1u);
  ASSERT_EQ(1u, diag.getResetCount());
}

TEST_F(IasTestAvbStreamHandler, getLocalStreamInfo)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  uint16_t localStreamId = 0u;
  uint8_t channelLayout  = 0u;

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numChannels              = 2u;
  bool hasSideChannel             = false;
  string name               = "test_";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numChannels,
                                                                                  48000u,
                                                                                  mAlsaAudioFormat,
                                                                                  cIasAvbPtpClockDomainId,
                                                                                  256u,
                                                                                  3u,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  name,
                                                                                  localStreamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  LocalAudioStreamInfoList audioList;
  LocalVideoStreamInfoList videoList;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->getLocalStreamInfo(audioList, videoList));
  ASSERT_FALSE(audioList.empty());
}

TEST_F(IasTestAvbStreamHandler, setClockRecoveryMode)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  uint32_t masterClockId = 0u;
  uint32_t slaveClockId  = 0u;
  uint32_t driverId      = 0u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setClockRecoveryParams(masterClockId,
                                                                                         slaveClockId,
                                                                                         driverId));

  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setClockRecoveryParams(masterClockId,
                                                                                         slaveClockId,
                                                                                         driverId));

  slaveClockId = cIasAvbPtpClockDomainId + 1u;
  // (NULL != master) && (NULL != slave)    (T && F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, mIasAvbStreamHandler->setClockRecoveryParams(masterClockId,
                                                                                                  slaveClockId,
                                                                                                  driverId));
}

TEST_F(IasTestAvbStreamHandler, setStreamActive)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  AvbStreamId streamId = 0u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setStreamActive(streamId, false));

  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  // !isInitialized() || (NULL == mAvbTransmitEngine)  (F || T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setStreamActive(streamId, false));
}

TEST_F(IasTestAvbStreamHandler, registerClient_NoInit)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->registerClient(NULL));
}

TEST_F(IasTestAvbStreamHandler, unregisterClient_NoInit)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->unregisterClient(NULL));
}

TEST_F(IasTestAvbStreamHandler, undateStreamStatus_NoInit)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  uint64_t streamID = 0;
  IasAvbStreamState state = IasAvbStreamState::eIasAvbStreamInactive;
  mIasAvbStreamHandler->updateStreamStatus(streamID, state);
}

// TODO: Investigate why this test stuck while connecting streams
TEST_F(IasTestAvbStreamHandler, DISABLED_BRANCH_LifeCycle)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  // StreamHandler not initialized yet
  ASSERT_EQ(eIasAvbProcNotInitialized, mIasAvbStreamHandler->start());
  ASSERT_EQ(eIasAvbProcNotInitialized, mIasAvbStreamHandler->stop());

  // initialize
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  ASSERT_EQ(eIasAvbProcAlreadyInUse, initAvbStreamHandler());

  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, mIasAvbStreamHandler->setClockRecoveryParams(-1,-1,-1));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setClockRecoveryParams(0u,0u,0u));
  heapSpaceLeft = 0;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setClockRecoveryParams(0u,0u,0u));
  heapSpaceLeft = heapSpaceInitSize;

  IasAvbStreamDirection transmit = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numberOfChannels = 2;
  uint8_t channelLayout = 0x00;
  bool hasSideChannel = false;
  string const name = "test";
  uint16_t localStreamId = 0u;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  IasAvbResult resultAvb = mIasAvbStreamHandler->createAlsaStream(transmit,
                                                      numberOfChannels,
                                                      48000u,
                                                      mAlsaAudioFormat,
                                                      cIasAvbPtpClockDomainId,
                                                      256u,
                                                      3u,
                                                      channelLayout,
                                                      hasSideChannel,
                                                      name,
                                                      localStreamId,
                                                      useAlsaDeviceType,
                                                      sampleFreqASRC);

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, resultAvb);

  uint16_t maxNumberChannels    = 2;
  uint32_t sampleFreq           = 48000;
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t clockId              = 0u;
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  AvbStreamId streamId          = 0x91E0F000FE000001u;
  MacAddress destMacAddr        = 0x91E0F000FE01;
  bool active                   = true;

  resultAvb = mIasAvbStreamHandler->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                              sampleFreq,
                                                              format,
                                                              clockId,
                                                              assignMode,
                                                              streamId,
                                                              destMacAddr,
                                                              active);
  // stream is already created
  ASSERT_EQ(eIasAvbProcErr, resultAvb);

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->connectStreams(streamId, localStreamId));

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());

  // race condition
  sleep(1);

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->setStreamActive(streamId, false));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->setStreamActive(streamId, true));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop());
}

TEST_F(IasTestAvbStreamHandler, BRANCH_Un_Registring)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamHandlerClientInterfaceImpl clientInterfaceImpl;

  // mClient != client
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->unregisterClient(&clientInterfaceImpl));

  // invalid param
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->registerClient(NULL));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->registerClient(&clientInterfaceImpl));

  // number of clients exceeded
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->registerClient(&clientInterfaceImpl));

  // unregister
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->unregisterClient(NULL));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->unregisterClient(&clientInterfaceImpl));
}

TEST_F(IasTestAvbStreamHandler, BRANCH_CallUpdates)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  IasAvbStreamHandlerClientInterfaceImpl clientInterfaceImpl;
  uint64_t streamId = 0u;
  bool linkIsUp = false;
  IasAvbStreamState status = IasAvbStreamState::eIasAvbStreamInactive;

  mIasAvbStreamHandler->updateLinkStatus(linkIsUp);
  mIasAvbStreamHandler->updateStreamStatus(streamId, status);

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, initAvbStreamHandler());
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->registerClient(&clientInterfaceImpl));

  mIasAvbStreamHandler->updateLinkStatus(linkIsUp);
  mIasAvbStreamHandler->updateStreamStatus(streamId, status);
}

TEST_F(IasTestAvbStreamHandler, ImproperTransmitStreamCreation)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  IasAvbResult avbResult = IasAvbResult::eIasAvbResultErr;
  IasAvbProcessingResult result = eIasAvbProcErr;

  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcOK, result);

  uint16_t maxNumberChannels    = 2;
  uint32_t sampleFreq           = 48000;
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t clockId              = int32_t(-1); //invalid id is needed for purpose of the test
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  AvbStreamId streamId          = 0x91E0F000FE000001u;
  MacAddress destMacAddr        = 0x91E0F000FE01;
  bool active                   = true;

  avbResult = mIasAvbStreamHandler->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                              sampleFreq,
                                                              format,
                                                              clockId,
                                                              assignMode,
                                                              streamId,
                                                              destMacAddr,
                                                              active);
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, avbResult);
}

//TEST_F(IasTestAvbStreamHandler, Not_AssignModeStatic)
//{
//  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

//  IasAvbResult avbResult = IasAvbResult::eIasAvbResultErr;
//  IasAvbProcessingResult result = eIasAvbProcErr;

//  result = initAvbStreamHandler();
//  ASSERT_EQ(eIasAvbProcOK, result);

//  uint16_t maxNumberChannels = 2;
//  uint32_t sampleFreq = 48000;
//  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
//  uint32_t clockId = 0u;
//  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll;
//  AvbStreamId streamId = 0x91E0F000FE000001u;
//  MacAddress destMacAddr = 0x91E0F000FE01;

//  avbResult = mIasAvbStreamHandler->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
//                                                              sampleFreq,
//                                                              format,
//                                                              clockId,
//                                                              assignMode,
//                                                              streamId,
//                                                              destMacAddr,
//                                                              true);
//  ASSERT_EQ(IasAvbResult::eIasAvbResultNotImplemented, avbResult);
//}

TEST_F(IasTestAvbStreamHandler, OutOfMemory)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  IasAvbProcessingResult result = eIasAvbProcErr;

  size_t heapSpace = 0;
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  heapSpace += sizeof(IasAvbStreamHandlerEnvironment);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  heapSpace += IasAvbStreamHandlerEnvironment::numDltContexts * sizeof(DltContext);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(device_t);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasLibPtpDaemon);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbReceiveEngine);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbSwClockDomain);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbPtpClockDomain);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbTransmitEngine);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasThread);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbAudioStream);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbTSpec);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbStreamId);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(IasAvbPacketPool);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += (sizeof(IasAvbPacket) * 30) + 8;
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  heapSpace += sizeof(struct igb_dma_alloc);
  heapSpaceLeft = heapSpace;
  result = initAvbStreamHandler();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbStreamHandler, deriveClockDomainFromRxStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  AvbStreamId rxStreamId = 0x91E0F000FE000001u;
  AvbStreamId badStreamId = -1;
  uint32_t clockId = 0u;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  MacAddress destMacAddr = 0x91E0F000FE01;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->deriveClockDomainFromRxStream(badStreamId, clockId));

  heapSpaceLeft = 0;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->deriveClockDomainFromRxStream(rxStreamId, clockId));

  heapSpaceLeft = heapSpaceInitSize;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                             maxNumberChannels, sampleFreq, rxStreamId, destMacAddr));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->deriveClockDomainFromRxStream(rxStreamId, clockId));

  mIasAvbStreamHandler->mNextClockDomainId = cIasAvbPtpClockDomainId;
  clockId                                  = 0u;// reset clockId
  // (mAvbClockDomains.end() != mAvbClockDomains.find(mNextClockDomainId)) && (mNextClockDomainId != 0u) (T && F)
  // 0u == mNextClockDomainId                                                                            (T)
  // eIasAvbProcOK != result                                                                             (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->deriveClockDomainFromRxStream(rxStreamId, clockId));
}

TEST_F(IasTestAvbStreamHandler, deriveClockDomainFromRxStream_NoInit)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  AvbStreamId rxStreamId = 0u;
  uint32_t      clockId    = 0u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->deriveClockDomainFromRxStream(rxStreamId, clockId));
}

TEST_F(IasTestAvbStreamHandler, deriveClockDomainFromRxStream_NoEngine)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  AvbStreamId rxStreamId = 0u;
  uint32_t      clockId    = 0u;
  // NULL != mAvbReceiveEngine                           (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->deriveClockDomainFromRxStream(rxStreamId, clockId));
}

TEST_F(IasTestAvbStreamHandler, DISABLED_EmergencyStop)
{
  //
  // NORMALLY DO NOT ENABLE THIS TEST CASE
  // emergencyStop is intended to clean up as much as possible but might leave some garbage.
  //
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  // called properly
  mIasAvbStreamHandler->emergencyStop();

  // already called - went through else case
  mIasAvbStreamHandler->emergencyStop();
}

TEST_F(IasTestAvbStreamHandler, safe_EmergencyStop)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  // not called on StreamHandlerEnvironment
  mIasAvbStreamHandler->emergencyStop();

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  // called on StreamHandlerEnvironment, but not detaching
  mIasAvbStreamHandler->mEnvironment->mArmed = false;
  mIasAvbStreamHandler->emergencyStop();
}

TEST_F(IasTestAvbStreamHandler, sleep_ns)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  mIasAvbStreamHandler->sleep_ns(100);
}

TEST_F(IasTestAvbStreamHandler, mapResultCode)
{
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, IasAvbStreamHandler::mapResultCode(eIasAvbProcOK));

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcErr));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcInvalidParam));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcOff));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcInitializationFailed));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcNotInitialized));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcNoSpaceLeft));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcNotEnoughMemory));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcAlreadyInUse));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcCallbackError));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcThreadStartFailed));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcThreadStopFailed));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode(eIasAvbProcNullPointerAccess));

  ASSERT_EQ(IasAvbResult::eIasAvbResultNotSupported, IasAvbStreamHandler::mapResultCode(eIasAvbProcUnsupportedFormat));

  ASSERT_EQ(IasAvbResult::eIasAvbResultNotImplemented, IasAvbStreamHandler::mapResultCode(eIasAvbProcNotImplemented));

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, IasAvbStreamHandler::mapResultCode((IasAvbProcessingResult)-1));
}

TEST_F(IasTestAvbStreamHandler, create_alsa_stream_No_Init)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numberOfChannels       = 2u;
  uint32_t sampleFreq             = 48000u;
  IasAvbAudioFormat format        = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t clockId                = 0u;
  uint32_t periodSize             = 256u;
  uint32_t numPeriods             = 3u;
  uint8_t channelLayout           = 0u;
  bool hasSideChannel             = false;
  std::string deviceName;
  uint16_t streamId               = 1u;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                  numberOfChannels,
                                                                  sampleFreq,
                                                                  format,
                                                                  clockId,
                                                                  periodSize,
                                                                  numPeriods,
                                                                  channelLayout,
                                                                  hasSideChannel,
                                                                  deviceName,
                                                                  streamId,
                                                                  useAlsaDeviceType,
                                                                  sampleFreqASRC));
}

// TODO: Enable when problems with mutex will be solved (IasLocalAudioBufferDesc::cleanup)
TEST_F(IasTestAvbStreamHandler, DISABLED_create_alsa_stream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numberOfChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = mAlsaAudioFormatNotSupported;
  uint32_t clockId = 0u;
  uint32_t periodSize = 256u;
  uint32_t numPeriods = 3u;
  uint8_t channelLayout = 0u;
  bool hasSideChannel = false;
  std::string deviceName("AlsaTest");
  uint16_t streamId = 0u;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  heapSpaceLeft = 0;

  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                   numberOfChannels,
                                                                   sampleFreq,
                                                                   format,
                                                                   clockId,
                                                                   periodSize,
                                                                   numPeriods,
                                                                   channelLayout,
                                                                   hasSideChannel,
                                                                   deviceName,
                                                                   streamId,
                                                                   useAlsaDeviceType,
                                                                   sampleFreqASRC));

  heapSpaceLeft = heapSpaceInitSize;

  streamId = 0;

  ASSERT_EQ(IasAvbResult::eIasAvbResultNotSupported, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                            numberOfChannels,
                                                                                            sampleFreq,
                                                                                            format,
                                                                                            clockId,
                                                                                            periodSize,
                                                                                            numPeriods,
                                                                                            channelLayout,
                                                                                            hasSideChannel,
                                                                                            deviceName,
                                                                                            streamId,
                                                                                            useAlsaDeviceType,
                                                                                            sampleFreqASRC));

  streamId = 0;
  numberOfChannels = cIasAvbMaxNumChannels + 1u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                   numberOfChannels,
                                                                                   sampleFreq,
                                                                                   format,
                                                                                   clockId,
                                                                                   periodSize,
                                                                                   numPeriods,
                                                                                   channelLayout,
                                                                                   hasSideChannel,
                                                                                   deviceName,
                                                                                   streamId,
                                                                                   useAlsaDeviceType,
                                                                                   sampleFreqASRC));

  numberOfChannels = 2u;
  format = mAlsaAudioFormat;
  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  // NULL == mAlsaEngine                      (T)
  // (eIasAvbProcOK == result) && isStarted() (T && T)

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numberOfChannels,
                                                                                  sampleFreq,
                                                                                  format,
                                                                                  clockId,
                                                                                  periodSize,
                                                                                  numPeriods,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  deviceName,
                                                                                  streamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  streamId = 0u;
  while (mIasAvbStreamHandler->isLocalStreamIdInUse(streamId++)) {}
  direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numberOfChannels,
                                                                                  sampleFreq,
                                                                                  format,
                                                                                  clockId,
                                                                                  periodSize,
                                                                                  numPeriods,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  deviceName,
                                                                                  streamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  uint16_t duplicateStreamId = streamId;

  // streamId in use
  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                   numberOfChannels,
                                                                   sampleFreq,
                                                                   format,
                                                                   clockId,
                                                                   periodSize,
                                                                   numPeriods,
                                                                   channelLayout,
                                                                   hasSideChannel,
                                                                   deviceName,
                                                                   duplicateStreamId,
                                                                   useAlsaDeviceType,
                                                                   sampleFreqASRC));

  ASSERT_EQ(0u, duplicateStreamId);

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));

  streamId = 0u;
  while (NULL != mIasAvbStreamHandler->getClockDomainById(clockId += 16u)) ;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numberOfChannels,
                                                                                  sampleFreq,
                                                                                  format,
                                                                                  clockId,
                                                                                  periodSize,
                                                                                  numPeriods,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  deviceName,
                                                                                  streamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop());
}

// TODO: Enable when problems with mutex will be solved (IasLocalAudioBufferDesc::cleanup)
TEST_F(IasTestAvbStreamHandler, DISABLED_create_alsa_stream_start)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t numberOfChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = mAlsaAudioFormatNotSupported;
  uint32_t clockId = 0u;
  uint32_t periodSize = 256u;
  uint32_t numPeriods = 3u;
  uint8_t channelLayout = 0u;
  bool hasSideChannel = false;
  std::string deviceName("AlsaTest");
  uint16_t streamId = 0u;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));

  format = mAlsaAudioFormat;
  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  // NULL == mAlsaEngine                      (T)
  // (eIasAvbProcOK == result) && isStarted() (T && T)
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numberOfChannels,
                                                                                  sampleFreq,
                                                                                  format,
                                                                                  clockId,
                                                                                  periodSize,
                                                                                  numPeriods,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  deviceName,
                                                                                  streamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->stop());
}

TEST_F(IasTestAvbStreamHandler, getAvbStreamInfo)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  AudioStreamInfoList audioStreamInfo;
  VideoStreamInfoList videoStreamInfo;
  ClockReferenceStreamInfoList clockRefStreamInfo;
  // not initialized, so cannot get proper results from Receive/TransmitEngine
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->getAvbStreamInfo(audioStreamInfo, videoStreamInfo, clockRefStreamInfo));

  bool runSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(runSetup));
  // initialized, but no stream has been created yet
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->getAvbStreamInfo(audioStreamInfo, videoStreamInfo, clockRefStreamInfo));

  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t maxPacketRate = 42u;
  uint16_t maxPacketSize = 1024u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  uint32_t clockId = 0;
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  uint64_t streamId = 0;
  uint64_t dmac = 0;
  bool active = false;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTransmitVideoStream(srClass,
                                                                                           maxPacketRate,
                                                                                           maxPacketSize,
                                                                                           format,
                                                                                           clockId,
                                                                                           assignMode,
                                                                                           streamId,
                                                                                           dmac,
                                                                                           active));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveVideoStream(srClass,
                                                                                          maxPacketRate,
                                                                                          maxPacketSize,
                                                                                          format,
                                                                                          streamId,
                                                                                          dmac));
  // both engines initialized, get info for all streams
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->getAvbStreamInfo(audioStreamInfo, videoStreamInfo, clockRefStreamInfo));
}

TEST_F(IasTestAvbStreamHandler, createTransmitVideoStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  IasAvbSrClass srClass         = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t maxPacketRate          = 42u;
  uint16_t maxPacketSize          = 1024u;
  IasAvbVideoFormat format      = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  uint32_t clockId                = 0u;
  IasAvbIdAssignMode assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  uint64_t streamId               = 0u;
  uint64_t dmac                   = 0u;
  bool active                   = false;
  // not initialized
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitVideoStream(srClass,
                                                                                            maxPacketRate,
                                                                                            maxPacketSize,
                                                                                            format,
                                                                                            clockId,
                                                                                            assignMode,
                                                                                            streamId,
                                                                                            dmac,
                                                                                            active));

  bool runSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(runSetup));

  assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicSrp;
  // IasAvbIdAssignMode::eIasAvbIdAssignModeStatic == assignMode    (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultNotImplemented, mIasAvbStreamHandler->createTransmitVideoStream(srClass,
                                                                                                       maxPacketRate,
                                                                                                       maxPacketSize,
                                                                                                       format,
                                                                                                       clockId,
                                                                                                       assignMode,
                                                                                                       streamId,
                                                                                                       dmac,
                                                                                                       active));

  assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  active     = true;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTransmitVideoStream(srClass,
                                                                                           maxPacketRate,
                                                                                           maxPacketSize,
                                                                                           format,
                                                                                           clockId,
                                                                                           assignMode,
                                                                                           streamId,
                                                                                           dmac,
                                                                                           active));

  clockId  = 1u;
  streamId = 1u;
  // NULL == clockDomain      (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitVideoStream(srClass,
                                                                                            maxPacketRate,
                                                                                            maxPacketSize,
                                                                                            format,
                                                                                            clockId,
                                                                                            assignMode,
                                                                                            streamId,
                                                                                            dmac,
                                                                                            active));
}

TEST_F(IasTestAvbStreamHandler, createReceiveVideoStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  uint16_t maxPacketRate = 42u;
  uint16_t maxPacketSize = 1024u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  uint64_t streamId = 0;
  uint64_t dmac = 0;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                                           maxPacketRate,
                                                                                           maxPacketSize,
                                                                                           format,
                                                                                           streamId,
                                                                                           dmac));

  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  heapSpaceLeft = sizeof(IasAvbReceiveEngine) - 1;
  // not enough mem for new receive engine
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                                           maxPacketRate,
                                                                                           maxPacketSize,
                                                                                           format,
                                                                                           streamId,
                                                                                           dmac));

  heapSpaceLeft = sizeof(IasAvbReceiveEngine) + sizeof(IasThread) - 1;
  // receive engine init failure
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                                           maxPacketRate,
                                                                                           maxPacketSize,
                                                                                           format,
                                                                                           streamId,
                                                                                           dmac));

  heapSpaceLeft = heapSpaceInitSize;
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());
  // (eIasAvbProcOK == result) && isStarted()   (T && T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                                          maxPacketRate,
                                                                                          maxPacketSize,
                                                                                          format,
                                                                                          streamId,
                                                                                          dmac));
}

TEST_F(IasTestAvbStreamHandler, createTransmitClockReferenceStream_active)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t crfStampsPerPdu          = 184u;
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 24000u;
  IasAvbClockMultiplier pull        = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  uint32_t clockId                  = (uint32_t)cIasAvbPtpClockDomainId;
  IasAvbIdAssignMode assignMode     = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  uint64_t streamId                 = 0u;
  uint64_t dmac                     = 0u;
  bool active                       = true;

  // (eIasAvbProcOK == result) && (active)                         (T && T)
  //  eIasAvbProcOK != result                                      (T)
  // max bandwidth exceeded
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  crfStampsPerPdu                = 18u;
  // (eIasAvbProcOK == result) && (active)                         (T && T)
  //  eIasAvbProcOK != result                                      (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTransmitClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));
}

TEST_F(IasTestAvbStreamHandler, createTransmitClockReferenceStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  // mCrfHeaderSize + (cCrfTimeStampSize * crfStampsPerPdu) = 1500
  uint16_t crfStampsPerPdu          = 185u;// 20 + 8 * 185
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 0x1FFFFFFFu - 1u;
  IasAvbClockMultiplier pull        = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  uint32_t clockId                  = (uint32_t)cIasAvbPtpClockDomainId;
  IasAvbIdAssignMode assignMode     = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  uint64_t streamId                 = 0u;
  uint64_t dmac                     = 0u;
  bool active                       = false;

  // !isInitialized()                                              (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  // packetSize >= ETH_DATA_LEN                                    (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  assignMode = IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicSrp;
  // IasAvbIdAssignMode::eIasAvbIdAssignModeStatic == assignMode   (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultNotImplemented, mIasAvbStreamHandler->createTransmitClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  assignMode   = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  crfStampsPerPdu = 184u;
  pull            = IasAvbClockMultiplier::eIasAvbCrsMultTvToMovie;
  // (IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio != type) (F)
  // || (IasAvbClockMultiplier::eIasAvbCrsMultFlat != pull)        (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultNotSupported, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));
  pull = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeVideoLine;
  // (IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio != type) (T)
  // || (IasAvbClockMultiplier::eIasAvbCrsMultFlat != pull)        (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultNotSupported, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  baseFreq                       = 0u;
  // (0u == crfStampsPerPdu) || (0u == crfStampInterval) (F || F)
  // || (0u == baseFreq) || (0x1FFFFFFFu < baseFreq)     (T || F)
  // || (NULL == clockDomain)                            (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  crfStampsPerPdu                = 1u;
  crfStampInterval               = 0u;
  baseFreq                       = 0x2FFFFFFFu;
  // (0u == crfStampsPerPdu) || (0u == crfStampInterval) (F || T)
  // || (0u == baseFreq) || (0x1FFFFFFFu < baseFreq)     (F || T)
  // || (NULL == clockDomain)                            (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));

  crfStampsPerPdu                = 0u;
  // (0u == crfStampsPerPdu) || (0u == crfStampInterval) (T || T)
  // || (0u == baseFreq) || (0x1FFFFFFFu < baseFreq)     (F || F)
  // || (NULL == clockDomain)                            (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));
  baseFreq         = 0x1FFFFFFFu;
  crfStampsPerPdu  = 1u;
  crfStampInterval = 1u;
  type             = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));
  // already initialized
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(
                                                                                      IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));
}

TEST_F(IasTestAvbStreamHandler, createTransmitClockReferenceStream_noTxEngine)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  // mCrfHeaderSize + (cCrfTimeStampSize * crfStampsPerPdu) = 1500
  uint16_t crfStampsPerPdu          = 185u;// 20 + 8 * 185
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 0x1FFFFFFFu - 1u;
  IasAvbClockMultiplier pull        = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  uint32_t clockId                  = (uint32_t)cIasAvbPtpClockDomainId;
  IasAvbIdAssignMode assignMode     = IasAvbIdAssignMode::eIasAvbIdAssignModeStatic;
  uint64_t streamId                 = 0u;
  uint64_t dmac                     = 0u;
  bool active                       = false;

  // NULL == mAvbTransmitEngine                                    (T)
  // packetSize >= ETH_DATA_LEN                                    (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTransmitClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockId,
                                                                                      assignMode,
                                                                                      streamId,
                                                                                      dmac,
                                                                                      active));
}

//TODO: investigate why there is no ptp response
TEST_F(IasTestAvbStreamHandler, DISABLED_triggerStorePersistenceData)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->triggerStorePersistenceData());

  mIasAvbStreamHandler->mEnvironment->mPtpProxy = NULL;

  ASSERT_EQ(eIasAvbProcNullPointerAccess, mIasAvbStreamHandler->triggerStorePersistenceData());
}


// TODO: Enable test when IasAvbStreamHandler::destroyLocalStream for KSL will be defined
TEST_F(IasTestAvbStreamHandler, DISABLED_destroyLocalStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  uint16_t streamId = 0u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->destroyLocalStream(streamId));

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t maxPacketRate            = 42u;
  uint16_t maxPacketSize            = 1024u;
  IasAvbVideoFormat format        = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  string ipcName("ipcName");
  streamId                        = 0u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                        maxPacketRate,
                                                                                        maxPacketSize,
                                                                                        format,
                                                                                        ipcName,
                                                                                        streamId));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));

  uint16_t numChannels = 2u;
  uint8_t channelLayout = 2u;
  bool hasSideChannel = true;
  string name = "test_";
  streamId = 0;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                      numChannels,
                                                      48000u,
                                                      mAlsaAudioFormat,
                                                      cIasAvbPtpClockDomainId,
                                                      256u,
                                                      3u,
                                                      channelLayout,
                                                      hasSideChannel,
                                                      name,
                                                      streamId,
                                                      useAlsaDeviceType,
                                                      sampleFreqASRC));

  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  streamId = 0;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                        maxPacketRate,
                                                                                        maxPacketSize,
                                                                                        format,
                                                                                        ipcName,
                                                                                        streamId));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));
}

TEST_F(IasTestAvbStreamHandler, destroyLocalStream_noSetup)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  uint16_t streamId               = 0u;
  uint16_t numberOfChannels       = 2u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSafFloat;
  uint8_t channelLayout           = 2u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createTestToneStream(numberOfChannels,
                                                                                      sampleFrequency,
                                                                                      audioFormat,
                                                                                      channelLayout,
                                                                                      streamId));

  // (NULL != mAvbJackInterface) && (eIasJackStream == localStreamType)  (F && N/A)
  // (NULL != mAlsaEngine) && (eIasAlsaStream == localStreamType)        (F && N/A)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->destroyLocalStream(streamId));
}

TEST_F(IasTestAvbStreamHandler, createTestToneStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  uint16_t streamId               = 0u;
  uint16_t numberOfChannels       = 2u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t channelLayout           = 2u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTestToneStream(numberOfChannels,
                                                                                       sampleFrequency,
                                                                                       audioFormat,
                                                                                       channelLayout,
                                                                                       streamId));

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());
  numberOfChannels = 0u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTestToneStream(numberOfChannels,
                                                                                       sampleFrequency,
                                                                                       audioFormat,
                                                                                       channelLayout,
                                                                                       streamId));

  streamId = mIasAvbStreamHandler->getNextLocalStreamId();
  // IasAvbAudioFormat::eIasAvbAudioFormatSafFloat != format   (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultNotSupported, mIasAvbStreamHandler->createTestToneStream(numberOfChannels,
                                                                                                sampleFrequency,
                                                                                                audioFormat,
                                                                                                channelLayout,
                                                                                                streamId));

  numberOfChannels = 2u;
  audioFormat      = IasAvbAudioFormat::eIasAvbAudioFormatSafFloat;
  heapSpaceLeft    = sizeof(IasTestToneStream) - 1;
  // NULL == stream                                            (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createTestToneStream(numberOfChannels,
                                                                                       sampleFrequency,
                                                                                       audioFormat,
                                                                                       channelLayout,
                                                                                       streamId));
}

TEST_F(IasTestAvbStreamHandler, setTestToneParams)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  uint16_t channel = 1u;
  uint32_t signalFrequency = 48000u;
  int32_t level = 0;
  IasAvbTestToneMode mode = IasAvbTestToneMode::eIasAvbTestToneSawtooth;
  int32_t userParam = 0;
  uint16_t streamId = 0u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->setTestToneParams(streamId,
                                                                                    channel,
                                                                                    signalFrequency,
                                                                                    level,
                                                                                    mode,
                                                                                    userParam));

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  streamId = 5u;// first free id in default config
  // (NULL != stream) && (eIasTestToneStream == stream->getType())  (F && N/A)
  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, mIasAvbStreamHandler->setTestToneParams(streamId,
                                                                                             channel,
                                                                                             signalFrequency,
                                                                                             level,
                                                                                             mode,
                                                                                             userParam));
  streamId = 0u;
  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numberOfChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = mAlsaAudioFormat;
  uint32_t clockId = 0u;
  uint32_t periodSize = 256;
  uint32_t numPeriods = 3u;
  uint8_t channelLayout = 2u;
  bool hasSideChannel = false;
  string deviceName("AlsaTest");
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createAlsaStream(direction,
                                                                                  numberOfChannels,
                                                                                  sampleFreq,
                                                                                  format,
                                                                                  clockId,
                                                                                  periodSize,
                                                                                  numPeriods,
                                                                                  channelLayout,
                                                                                  hasSideChannel,
                                                                                  deviceName,
                                                                                  streamId,
                                                                                  useAlsaDeviceType,
                                                                                  sampleFreqASRC));

  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, mIasAvbStreamHandler->setTestToneParams(streamId,
                                                                                             channel,
                                                                                             signalFrequency,
                                                                                             level,
                                                                                             mode,
                                                                                             userParam));
}

TEST_F(IasTestAvbStreamHandler, createReceiveEngine)
{
  bool noSetup = false;
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));

  heapSpaceLeft = sizeof(IasAvbReceiveEngine) - 1;
  // NULL == mAvbReceiveEngine             (T)
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mIasAvbStreamHandler->createReceiveEngine());

  heapSpaceLeft = sizeof(IasAvbReceiveEngine) + sizeof(IasThread) - 1;
  // NULL == mAvbReceiveEngine             (F)
  // mAvbReceiveEngine.init fails
  ASSERT_EQ(eIasAvbProcInitializationFailed, mIasAvbStreamHandler->createReceiveEngine());

  heapSpaceLeft                = heapSpaceInitSize;
  // NULL == mAvbReceiveEngine                  (F)
  // (eIasAvbProcOK == result) && isStarted()   (T && F)
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->createReceiveEngine());
}

TEST_F(IasTestAvbStreamHandler, createTransmitEngine)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  heapSpaceLeft = sizeof(IasAvbTransmitEngine) - 1;
  // NULL == mAvbTransmitEngine             (T)
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mIasAvbStreamHandler->createTransmitEngine());

  heapSpaceLeft = sizeof(IasAvbTransmitEngine) - 1;

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mIasAvbStreamHandler->createTransmitEngine());

  heapSpaceLeft                = heapSpaceInitSize;
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  // NULL == mAvbTransmitEngine                  (F)
  // (eIasAvbProcOK == result) && isStarted()   (T && F)
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->createTransmitEngine());
}

TEST_F(IasTestAvbStreamHandler, createTransmitEngine_start)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());
  // NULL == mAvbTransmitEngine                  (F)
  // (eIasAvbProcOK == result) && isStarted()   (T && T)
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->createTransmitEngine());
}

TEST_F(IasTestAvbStreamHandler, createReceiveEngine_start)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->start());
  // NULL == mAvbReceiveEngine                  (F)
  // (eIasAvbProcOK == result) && isStarted()   (T && T)
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandler->createReceiveEngine());
}

TEST_F(IasTestAvbStreamHandler, createReceiveClockReferenceStream)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu           = 12u;
  uint64_t streamId                     = 0x91E0F000FE000000u;
  uint64_t dmac                         = 0u;
  uint32_t clockId                      = (uint32_t)cIasAvbPtpClockDomainId;

  // not initialized
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));

  bool noSetup = false;
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(noSetup));
  heapSpaceLeft = sizeof(IasAvbReceiveEngine) + sizeof(IasAvbClockReferenceStream)
#if defined(DIRECT_RX_DMA)
                   // receive buffer will not be allocated from heap
#else
                   + (sizeof(uint8_t) * IasAvbReceiveEngine::cReceiveBufferSize)
#endif
                   + sizeof(IasThread) + sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId) - 1;
  // NULL == mAvbReceiveEngine                           (T)
  // force IasAvbClockReferenceStream::initReceive to fail on stream init
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));

  heapSpaceLeft = heapSpaceInitSize;

  type          = IasAvbClockReferenceStreamType::eIasAvbCrsTypeUser;
  ASSERT_EQ(IasAvbResult::eIasAvbResultNotSupported, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));
  maxCrfStampsPerPdu = 0u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));
  maxCrfStampsPerPdu = 1u;
  type               = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));
}

TEST_F(IasTestAvbStreamHandler, createReceiveClockReferenceStream_runSetup)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu           = 12u;
  uint64_t streamId                     = 0x91E0F000FE000000u;
  uint64_t dmac                         = 0u;
  uint32_t clockId                      = 0u; // let StrH generate the ID

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler(true, true));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClkRecoverFrom, 0u));

  // mEnvironment->queryConfigValue(IasRegKeys::cClkRecoverFrom, streamIdMcr) (T)
  // && (streamIdMcr == streamId)                                             (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));

  streamId--;
  clockId++;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClkRecoverFrom, streamId));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClkRecoverUsing, cIasAvbPtpClockDomainId));

  // mEnvironment->queryConfigValue(IasRegKeys::cClkRecoverFrom, streamIdMcr) (T)
  // && (streamIdMcr == streamId)                                             (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createReceiveClockReferenceStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                                  type,
                                                                  maxCrfStampsPerPdu,
                                                                  streamId,
                                                                  dmac,
                                                                  clockId));
}

TEST_F(IasTestAvbStreamHandler, isLocalStreamIdInUse)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);
  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t maxPacketRate       = 42u;
  uint16_t maxPacketSize       = 1024u;
  IasAvbVideoFormat format        = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t streamId                 = 5u;
  const string ipcName("ipcName");

  // isLocalStreamIdInUse(streamId)                (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                         maxPacketRate,
                                                                                         maxPacketSize,
                                                                                         format,
                                                                                         ipcName,
                                                                                         streamId));

  // TODO: KSL missing implementation -> getLocalVideoStreamById always returns 0
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mIasAvbStreamHandler->createLocalVideoStream(direction,
                                                                                        maxPacketRate,
                                                                                        maxPacketSize,
                                                                                        format,
                                                                                        ipcName,
                                                                                        streamId));
}

TEST_F(IasTestAvbStreamHandler, IasAvbStreamHandlerControllerInterfaceTest)
{
  ASSERT_TRUE(mIasAvbStreamHandler != NULL);

  ASSERT_EQ(eIasAvbProcOK, initAvbStreamHandler());

  IasAvbStreamHandlerControllerInterfaceImpl controllerInterfaceImpl;
  ASSERT_EQ(eIasAvbProcOK, controllerInterfaceImpl.cleanup());
}
