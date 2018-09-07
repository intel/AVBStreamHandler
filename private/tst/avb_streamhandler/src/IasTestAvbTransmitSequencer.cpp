/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestAvbTransmitSequencer.cpp
 * @brief   The implementation of the IasTestAvbTransmitSequencer test class.
 * @date    2017
 */

#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasAvbTransmitSequencer.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "internal/audio/common/alsa_smartx_plugin/IasAlsaPluginIpc.hpp"
#include "avb_streamhandler/IasAvbTransmitEngine.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEventInterface.hpp"
#undef protected
#undef private
#include "test_common/IasSpringVilleInfo.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_helper/IasIRunnable.hpp"
#include "avb_helper/IasResult.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"

//#include "e1000_hw.h"
//#include "igb_internal.h"
//#include <stdbool.h>

using namespace IasMediaTransportAvb;
using std::nothrow;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb
{

class FakeEventInterface: public IasAvbStreamHandlerEventInterface
{
public:
    FakeEventInterface(){}
    ~FakeEventInterface(){}

private:
    void updateLinkStatus(const bool linkIsUp)
    {
        (void) linkIsUp;
    }
    void updateStreamStatus(uint64_t streamId, IasAvbStreamState status)
    {
        (void) streamId;
        (void) status;
    }
};

class IasTestAvbTransmitSequencer : public ::testing::Test
{
protected:
  IasTestAvbTransmitSequencer()
    : mEnvironment(nullptr)
      ,mSequencer(nullptr)
      ,mTransmitEngine(nullptr)
  {
    DLT_REGISTER_APP("IATS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbTransmitSequencer()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);

    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAvbTransmitSequencer",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);

    mSequencer = new (std::nothrow) IasAvbTransmitSequencer(mDltContext);
    ASSERT_TRUE(NULL != mSequencer);

    mTransmitEngine = new IasAvbTransmitEngine();
    ASSERT_NE(nullptr, mTransmitEngine);

    heapSpaceLeft = heapSpaceInitSize;
  }

  virtual void TearDown()
  {
    if (nullptr != mTransmitEngine)
    {
      delete mTransmitEngine;
      mTransmitEngine = nullptr;
    }

    if (NULL != mSequencer)
    {
      delete mSequencer;
      mSequencer = NULL;
    }

    if (NULL != mEnvironment)
    {
      delete mEnvironment;
      mEnvironment = NULL;
    }

    heapSpaceLeft = heapSpaceInitSize;
  }

  // create ptp proxy
  bool LocalSetup()
  {
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();
      mEnvironment->mTxRingSize = 512;

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cNwIfName,  IasSpringVilleInfo::getInterfaceName()))
        {
          if (eIasAvbProcOK == mEnvironment->createIgbDevice())
          {
            if (IasAvbStreamHandlerEnvironment::getIgbDevice())
            {
              if (eIasAvbProcOK == mEnvironment->createPtpProxy())
              {
                return true;
              }
            }
          }
        }
      }
    }

    return false;
  }

  IasAvbProcessingResult createProperAudioStream(IasAvbClockDomain * clockDomain, IasAvbStreamId streamId = IasAvbStreamId(uint64_t(0u)));

  DltContext mDltContext;
  IasAvbStreamHandlerEnvironment * mEnvironment;
  IasAvbTransmitSequencer * mSequencer;
  IasAvbTransmitEngine * mTransmitEngine;
};

IasAvbProcessingResult IasTestAvbTransmitSequencer::createProperAudioStream(IasAvbClockDomain * clockDomain, IasAvbStreamId streamId)
{
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbMacAddress destMacAddr = {0};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  IasAvbProcessingResult result = mTransmitEngine->createTransmitAudioStream(srClass,
                                                                             maxNumberChannels,
                                                                             sampleFreq,
                                                                             format,
                                                                             clockDomain,
                                                                             streamId,
                                                                             destMacAddr,
                                                                             true);
  return result;
}
} // namespace

TEST_F(IasTestAvbTransmitSequencer, initDouble)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mSequencer->init(1u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mSequencer->init(1u, IasAvbSrClass::eIasAvbSrClassLow, true);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbTransmitSequencer, init)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mSequencer->init(2u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbTransmitSequencer, initQav)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mSequencer->init(1u, (IasAvbSrClass)5u, false);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbTransmitSequencer, initMem)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  heapSpaceLeft = sizeof(IasThread) - 1;
  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);
}

TEST_F(IasTestAvbTransmitSequencer, initWindowWidth)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  mEnvironment->setConfigValue(IasRegKeys::cXmitWndWidth, 1u);
  mEnvironment->setConfigValue(IasRegKeys::cXmitWndPitch, 2u);
  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbTransmitSequencer, initWindowWidthPitch)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  mEnvironment->setConfigValue(IasRegKeys::cXmitWndWidth, IasAvbTransmitSequencer::cMinTxWindowWidth - 1u);
  mEnvironment->setConfigValue(IasRegKeys::cXmitWndPitch, IasAvbTransmitSequencer::cMinTxWindowPitch);
  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbTransmitSequencer, initWindowPitch)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  mEnvironment->setConfigValue(IasRegKeys::cXmitWndWidth, IasAvbTransmitSequencer::cMinTxWindowWidth);
  mEnvironment->setConfigValue(IasRegKeys::cXmitWndPitch, IasAvbTransmitSequencer::cMinTxWindowPitch - 1u);
  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbTransmitSequencer, initBwRateNeg)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;


  std::string optName = std::string(IasRegKeys::cDebugXmitShaperBwRate) + IasAvbTSpec::getClassSuffix(srClass);
  mEnvironment->setConfigValue(optName, 101u);
  mEnvironment->setConfigValue(IasRegKeys::cXmitUseWatchdog, 1u);
  result = mSequencer->init(0u, srClass, false);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbTransmitSequencer, initBwRatePos)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;


  std::string optName = std::string(IasRegKeys::cDebugXmitShaperBwRate) + IasAvbTSpec::getClassSuffix(srClass);
  mEnvironment->setConfigValue(optName, 99u);
  result = mSequencer->init(0u, srClass, false);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbTransmitSequencer, initBwRateWdog)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  mEnvironment->setConfigValue(IasRegKeys::cXmitUseWatchdog, 1u);
  mEnvironment->mUseWatchdog = true;
  ASSERT_TRUE(mEnvironment->isWatchdogEnabled());
  result = mSequencer->init(0u, srClass, false);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbTransmitSequencer, registerEventInterfaceThreadNull)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  IasAvbStreamHandlerEventInterface * eventInterface = nullptr;
  result = mSequencer->registerEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);
}

TEST_F(IasTestAvbTransmitSequencer, registerEventInterfaceInterfaceNull)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbStreamHandlerEventInterface * eventInterface = nullptr;

  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = eIasAvbProcErr;
  result = mSequencer->registerEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbTransmitSequencer, registerEventInterfaceAlreadyRegistered)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  std::unique_ptr<FakeEventInterface> eventInterface(new FakeEventInterface());

  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = eIasAvbProcErr;
  result = mSequencer->registerEventInterface(eventInterface.get());
  ASSERT_EQ(eIasAvbProcOK, result);

  result = eIasAvbProcErr;
  result = mSequencer->registerEventInterface(eventInterface.get());
  ASSERT_EQ(eIasAvbProcAlreadyInUse, result);
}

TEST_F(IasTestAvbTransmitSequencer, unregisterEventInterfaceInvalidParam)
{
  ASSERT_TRUE(NULL != mEnvironment);
  ASSERT_TRUE(NULL != mSequencer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  std::unique_ptr<FakeEventInterface> eventInterface(new FakeEventInterface());
  std::unique_ptr<FakeEventInterface> otherEventInterface(new FakeEventInterface());

  result = mSequencer->unregisterEventInterface(eventInterface.get());
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  result = mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = eIasAvbProcErr;
  result = mSequencer->registerEventInterface(eventInterface.get());
  ASSERT_EQ(eIasAvbProcOK, result);

  result = eIasAvbProcErr;
  result = mSequencer->unregisterEventInterface(otherEventInterface.get());
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = eIasAvbProcErr;
  result = mSequencer->unregisterEventInterface(nullptr);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = eIasAvbProcErr;
  result = mSequencer->unregisterEventInterface(eventInterface.get());
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbTransmitSequencer, resetPoolsOfActiveStreams)
{
  mSequencer->resetPoolsOfActiveStreams();
}

TEST_F(IasTestAvbTransmitSequencer, updateShaper)
{
  mSequencer->updateShaper();

  ASSERT_TRUE(LocalSetup());

  mSequencer->updateShaper();
}

TEST_F(IasTestAvbTransmitSequencer, cleanup)
{

  ASSERT_TRUE(LocalSetup());

  ASSERT_EQ(eIasAvbProcOK, mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false));

  ASSERT_EQ(eIasAvbProcOK, mSequencer->start());
  sleep(1);

  mSequencer->cleanup();
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbTransmitSequencer, logOutput)
{
  ASSERT_EQ(eIasAvbProcOK, mSequencer->init(0u, IasAvbSrClass::eIasAvbSrClassHigh, false));

  mSequencer->mDiag.debugOutputCount = 400u;
  mSequencer->mDiag.avgPacketSent = 1.0f;
  mSequencer->mDiag.sent = 0u;
  mSequencer->mDiag.reordered = 1u;

  mSequencer->logOutput(1.0f, 0.0f);
  ASSERT_EQ(0u, mSequencer->mDiag.reordered);
  ASSERT_EQ(0.99f, mSequencer->mDiag.avgPacketSent);

  mSequencer->mDiag.debugOutputCount = 400u;
  mSequencer->mDiag.avgPacketSent = 0.01f;
  mSequencer->mDiag.sent = 1u;

  mSequencer->logOutput(1.0f, 0.0f);
  ASSERT_EQ(0u, mSequencer->mDiag.sent);

  mSequencer->mDiag.debugOutputCount = 400u;
  mSequencer->mDiag.avgPacketSent = 0.0f;
  mSequencer->mDiag.sent = 0u;
  mSequencer->mDiag.reordered = 1u;

  mSequencer->logOutput(1.0f, 0.0f);
  ASSERT_EQ(0u, mSequencer->mDiag.reordered);
  ASSERT_EQ(0.0f, mSequencer->mDiag.avgPacketSent);

  mSequencer->mDiag.sent = 1u;
  mSequencer->logOutput(1.0f, 0.0f);
  ASSERT_EQ(0u, mSequencer->mDiag.sent);
}

TEST_F(IasTestAvbTransmitSequencer, serviceStream)
{
  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  ASSERT_TRUE(NULL != sequencer);
  {
    sequencer->mLock.lock();
    sequencer->mRequestCount++;
    sequencer->mLock.unlock();
    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStreamToService = sequencer->mSequence.end();
    sequencer->updateSequence(nextStreamToService);
    sleep(1);

    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t now = ptp->getLocalTime();

    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStream = sequencer->mSequence.begin();
    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eTxError;
    (*nextStream).packet = NULL;

    IasAvbStream *stream = nextStream->stream;
    (*nextStream).stream = NULL;
    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eTxError, sequencer->serviceStream(now, nextStream));
    nextStream->stream = stream;

    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;
    (*nextStream).packet = new IasAvbPacket();
    (*nextStream).packet->mDummyFlag = true;
    now = ptp->getLocalTime();
    (*nextStream).packet->attime = now + sequencer->mConfig.txWindowWidth + 1u;
    (*nextStream).launchTime = now;
    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eEndOfWindow, sequencer->serviceStream(now, nextStream));

    now = ptp->getLocalTime();
    (*nextStream).packet->attime = now;
    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;

    delete (*nextStream).packet;
  }
}

TEST_F(IasTestAvbTransmitSequencer, serviceStreamDummyPacket)
{
  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  ASSERT_TRUE(NULL != sequencer);
  {
    sequencer->mLock.lock();
    sequencer->mRequestCount++;
    sequencer->mLock.unlock();
    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStreamToService = sequencer->mSequence.end();
    sequencer->updateSequence(nextStreamToService);
    sleep(1);

    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t now = ptp->getLocalTime();

    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStream = sequencer->mSequence.begin();

    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;
    IasAvbPacket *packet = new IasAvbPacket();
    (*nextStream).packet = packet;
    (*nextStream).packet->mDummyFlag = true;
    now = ptp->getLocalTime();
    (*nextStream).packet->attime = now + sequencer->mConfig.txWindowWidth;
    (*nextStream).launchTime = now;
    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eNotDone, sequencer->serviceStream(now, nextStream));
    delete packet;
    (*nextStream).packet = nullptr;
  }
}

TEST_F(IasTestAvbTransmitSequencer, serviceStreamSendPacket)
{
  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  ASSERT_TRUE(NULL != sequencer);
  {
    sequencer->mLock.lock();
    sequencer->mRequestCount++;
    sequencer->mLock.unlock();
    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStreamToService = sequencer->mSequence.end();
    sequencer->updateSequence(nextStreamToService);
    sleep(1);

    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t now = ptp->getLocalTime();

    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStream = sequencer->mSequence.begin();

    now = ptp->getLocalTime();
    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;
    IasAvbPacket *packet = new IasAvbPacket();
    (*nextStream).packet = packet;
    (*nextStream).packet->mDummyFlag = false;
    (*nextStream).packet->attime = now + sequencer->mConfig.txWindowWidth;
    (*nextStream).launchTime = now;
    sequencer->mDiag.debugLastLaunchTime = (*nextStream).packet->attime;
    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eNotDone, sequencer->serviceStream(now, nextStream));
    delete packet;
    (*nextStream).packet = nullptr;
  }
}

TEST_F(IasTestAvbTransmitSequencer, start)
{

  ASSERT_TRUE(LocalSetup());
  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  IasThread * oldTransmitThread = sequencer->mTransmitThread;
  sequencer->mTransmitThread = NULL;

  ASSERT_EQ(eIasAvbProcNotInitialized, sequencer->start());
  ASSERT_EQ(eIasAvbProcNotInitialized, sequencer->stop());

  sequencer->mTransmitThread = oldTransmitThread;
  mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "rr");
  // IasAvbTransmitSequencer::run - policyStr == "rr"
  ASSERT_EQ(eIasAvbProcOK, sequencer->start());
  sleep(1);

  ASSERT_EQ(eIasAvbProcOK, sequencer->stop());
  mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "other");
  sleep(1);
  // IasAvbTransmitSequencer::run - policyStr == "other"
  ASSERT_EQ(eIasAvbProcOK, sequencer->start());

  sleep(1);
  ASSERT_EQ(eIasAvbProcOK, sequencer->stop());

  ASSERT_EQ(eIasAvbProcOK, sequencer->start());
  ASSERT_EQ(eIasAvbProcOK, sequencer->start());
  ASSERT_EQ(eIasAvbProcOK, sequencer->stop());
}

TEST_F(IasTestAvbTransmitSequencer, serviceStreamEINVAL)
{
  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  ASSERT_TRUE(NULL != sequencer);
  {
    sequencer->mLock.lock();
    sequencer->mRequestCount++;
    sequencer->mLock.unlock();
    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStreamToService = sequencer->mSequence.end();
    sequencer->updateSequence(nextStreamToService);
    sleep(1);

    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t now = ptp->getLocalTime();

    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStream = sequencer->mSequence.begin();

    now = ptp->getLocalTime();
    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;
    (*nextStream).packet = new IasAvbPacket();
    (*nextStream).packet->mDummyFlag = false;
    (*nextStream).packet->attime = now + sequencer->mConfig.txWindowWidth;
    (*nextStream).launchTime = now;
    sequencer->mDiag.debugLastLaunchTime = (*nextStream).packet->attime;
    _device_t * tempDev = sequencer->mIgbDevice;
    sequencer->mIgbDevice = nullptr;
    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eTxError, sequencer->serviceStream(now, nextStream));
    sequencer->mIgbDevice = tempDev;

    delete (*nextStream).packet;
  }
}

TEST_F(IasTestAvbTransmitSequencer, serviceStreamENXIO)
{
  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  ASSERT_TRUE(NULL != sequencer);
  {
    sequencer->mLock.lock();
    sequencer->mRequestCount++;
    sequencer->mLock.unlock();
    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStreamToService = sequencer->mSequence.end();
    sequencer->updateSequence(nextStreamToService);
    sleep(1);

    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t now = ptp->getLocalTime();

    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStream = sequencer->mSequence.begin();

    now = ptp->getLocalTime();
    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;
    (*nextStream).packet = new IasAvbPacket();
    (*nextStream).packet->mDummyFlag = false;
    (*nextStream).packet->attime = now + sequencer->mConfig.txWindowWidth;
    (*nextStream).launchTime = now;
    sequencer->mDiag.debugLastLaunchTime = (*nextStream).packet->attime;
    void * tempPrivData = sequencer->mIgbDevice->private_data;
    sequencer->mIgbDevice->private_data = nullptr;
    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eTxError, sequencer->serviceStream(now, nextStream));
    sequencer->mIgbDevice->private_data = tempPrivData;

    delete (*nextStream).packet;
  }
}

TEST_F(IasTestAvbTransmitSequencer, streamDataStructure)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbTransmitSequencer::StreamData first_streamData;
  IasAvbStream *avbStream = NULL;
  IasAvbPacket *avbPacket = NULL;
  uint64_t launchTime = 1u;
  IasAvbTransmitSequencer::DoneState done = IasAvbTransmitSequencer::DoneState::eNotDone;

  first_streamData.stream = avbStream;
  first_streamData.packet = avbPacket;
  first_streamData.launchTime = launchTime;

  launchTime = 2u;
  IasAvbTransmitSequencer::StreamData second_streamData;
  second_streamData.stream = avbStream;
  second_streamData.packet = avbPacket;
  second_streamData.launchTime = launchTime;

  ASSERT_TRUE(first_streamData < second_streamData);

  delete avbStream;
  avbStream = NULL;
  delete avbPacket;
  avbPacket = NULL;
}

TEST_F(IasTestAvbTransmitSequencer, addStreamToTransmitList)
{

  ASSERT_TRUE(LocalSetup());
  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  IasAvbStream * stream = mTransmitEngine->mAvbStreams[streamID];
  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(stream);
  IasAvbStream * nullStream = NULL;
  // NULL == stream                             (T)
  // || stream->getTSpec().getClass() != mClass (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, sequencer->addStreamToTransmitList(nullStream));

  sequencer->mClass = IasAvbSrClass::eIasAvbSrClassLow;
  // NULL == stream                             (F)
  // || stream->getTSpec().getClass() != mClass (T)
  ASSERT_EQ(eIasAvbProcInvalidParam, sequencer->addStreamToTransmitList(stream));

  sequencer->mClass = IasAvbSrClass::eIasAvbSrClassHigh;
  IasThread * seqTransmitThread = sequencer->mTransmitThread;
  sequencer->mTransmitThread = NULL;

  ASSERT_EQ(eIasAvbProcNotInitialized, sequencer->addStreamToTransmitList(stream));

  sequencer->mTransmitThread = seqTransmitThread;
  sequencer->mCurrentBandwidth = sequencer->mConfig.txMaxBandwidth;
  // newTotal > mConfig.txMaxBandwidth (T)
  ASSERT_EQ(eIasAvbProcNoSpaceLeft, sequencer->addStreamToTransmitList(stream));
}

TEST_F(IasTestAvbTransmitSequencer, removeStreamFromTransmitList)
{

  ASSERT_TRUE(LocalSetup());
  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));
  IasAvbStream * stream = mTransmitEngine->mAvbStreams[streamID];
  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(stream);
  IasAvbStream * nullStream = NULL;
  // NULL != stream (F)
  // || stream->getTSpec().getClass() != mClass (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, sequencer->removeStreamFromTransmitList(nullStream));

  sequencer->mClass = IasAvbSrClass::eIasAvbSrClassLow;
  // NULL != stream (T)
  // || stream->getTSpec().getClass() != mClass (T)
  ASSERT_EQ(eIasAvbProcInvalidParam, sequencer->removeStreamFromTransmitList(stream));

  sequencer->mClass = IasAvbSrClass::eIasAvbSrClassLow;
  sequencer->mTransmitThread = NULL;

  ASSERT_EQ(eIasAvbProcNotInitialized, sequencer->removeStreamFromTransmitList(stream));
}

TEST_F(IasTestAvbTransmitSequencer, sortByLaunchTime)
{
  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId firstStreamID((uint64_t)0u), secondStreamID((uint64_t)1u), thirdStreamID((uint64_t)2u);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, firstStreamID));
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, secondStreamID));
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, thirdStreamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(thirdStreamID));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(secondStreamID));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(firstStreamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->start());
  sleep(1);

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[firstStreamID]);

  ASSERT_TRUE(NULL != sequencer);
  // let worker thread block to avoid interference with the test code below
  // use a mutex guard to ensure thread is unblocked even if anything goes wrong
  {
    std::lock_guard<std::mutex> autoLock(sequencer->mLock);
    sequencer->mRequestCount++;
    sleep(1);

    IasAvbTransmitSequencer::AvbStreamDataList::iterator lastStream = std::prev(sequencer->mSequence.end());
    IasAvbTransmitSequencer::AvbStreamDataList::iterator middleStream = std::prev(lastStream);
    IasAvbTransmitSequencer::AvbStreamDataList::iterator firstStream = sequencer->mSequence.begin();
    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t customLaunchTime = ptp->getLocalTime();
    (*firstStream).launchTime = customLaunchTime;
    (*middleStream).launchTime = customLaunchTime + 2u;
    (*lastStream).launchTime = customLaunchTime + 1u;

    sequencer->sortByLaunchTime(lastStream);

    (*firstStream).launchTime = 0u;

    sequencer->sortByLaunchTime(lastStream);

    (*firstStream).launchTime = customLaunchTime;
  }

  mTransmitEngine->stop();
}

TEST_F(IasTestAvbTransmitSequencer, reclaimPackets)
{

  ASSERT_TRUE(LocalSetup());
  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
  sequencer->mDoReclaim = false;

  ASSERT_EQ(0u, sequencer->reclaimPackets());
}


// below test has to be moved out to separate file
//TEST_F(IasTestAvbTransmitSequencer, serviceStreamENOSPC)
//{
//  ASSERT_TRUE(LocalSetup());

//  IasAvbPtpClockDomain clockdomain;
//  IasAvbStreamId streamID((uint64_t)0);

//  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

//  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(&clockdomain, streamID));

//  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamID));

//  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[streamID]);
//  ASSERT_TRUE(NULL != sequencer);
//  {
//    sequencer->mLock.lock();
//    sequencer->mRequestCount++;
//    sequencer->mLock.unlock();
//    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStreamToService = sequencer->mSequence.end();
//    sequencer->updateSequence(nextStreamToService);
//    sleep(1);

//    IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
//    uint64_t now = ptp->getLocalTime();

//    IasAvbTransmitSequencer::AvbStreamDataList::iterator nextStream = sequencer->mSequence.begin();

//    now = ptp->getLocalTime();
//    (*nextStream).done = IasAvbTransmitSequencer::DoneState::eNotDone;
//    (*nextStream).packet = new IasAvbPacket();
//    (*nextStream).packet->mDummyFlag = false;
//    (*nextStream).packet->attime = now + sequencer->mConfig.txWindowWidth;
//    (*nextStream).launchTime = now;
//    sequencer->mDiag.debugLastLaunchTime = (*nextStream).packet->attime;
//    struct adapter *adapter = (struct adapter *)sequencer->mIgbDevice->private_data;
//    struct tx_ring *txr = &adapter->tx_rings[sequencer->mQueueIndex];
//    uint16_t tempTxAvail = txr->tx_avail;
//    txr->tx_avail = 1u;
//    ASSERT_EQ(IasAvbTransmitSequencer::DoneState::eNotDone, sequencer->serviceStream(now, nextStream));
//    txr->tx_avail = tempTxAvail;
//  }
//}

