/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file   IasTestTransmitEngine.cpp
 * @date   2018
 * @brief  This file contains the unit tests of the IasTransmitEngine class.
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbStream.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbTransmitSequencer.hpp"
#include "avb_streamhandler/IasAvbTransmitEngine.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEventInterface.hpp"
#define protected protected
#define private private

#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbTSpec.hpp"
#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include <stdlib.h>// for wd setenv

#include "test_common/IasSpringVilleInfo.hpp"

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb {

class IasTestTransmitEngine : public ::testing::Test
{
  public:
    enum TestPolicy {
      ePolicyOther,
      ePolicyRr,
      ePolicyFifo,
      ePolicyNone
    };

  protected:
  IasTestTransmitEngine()
    : mTransmitEngine(NULL)
    , mEnvironment(NULL)
    , mDltCtx()
  {
    //
  }

  virtual ~IasTestTransmitEngine() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
                               "TEST",
                               "IasTestTransmitEngine",
                               DLT_LOG_INFO,
                               DLT_TRACE_STATUS_OFF);
    heapSpaceLeft = heapSpaceInitSize;

//    dlt_enable_local_print();
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mTransmitEngine = new IasAvbTransmitEngine();

    ASSERT_NE(nullptr, mTransmitEngine);
  }

  // Tears down the test fixture.
  virtual void TearDown()
  {
    delete mTransmitEngine;
    delete mEnvironment;
    heapSpaceLeft = heapSpaceInitSize;
    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }

  bool LocalSetup(TestPolicy policy = ePolicyNone)
  {
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();
      mEnvironment->mTxRingSize = 512;

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());

        switch (policy)
        {
          default:
            // Do Nothing
            break;
          case ePolicyOther:
            mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "other");
            break;
          case ePolicyRr:
            mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "rr");
            break;
          case ePolicyFifo:
            mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "fifo");
            break;
        }

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
    return false;
  }

  IasAvbProcessingResult createProperAudioStream(IasAvbClockDomain * clockDomain, IasAvbStreamId streamId = IasAvbStreamId(uint64_t(0u)));
  IasAvbProcessingResult createProperVideoStream(IasAvbClockDomain * clockDomain, IasAvbStreamId streamId = IasAvbStreamId(uint64_t(0u)));
  IasAvbProcessingResult createProperCRStream(IasAvbClockDomain * clockDomain, uint64_t uStreamId = 0u);

  IasAvbTransmitEngine * mTransmitEngine;
  IasAvbStreamHandlerEnvironment * mEnvironment;
  DltContext mDltCtx;

protected:

  class IasAvbStreamHandlerEventImpl : public IasAvbStreamHandlerEventInterface
  {
    virtual void updateLinkStatus(const bool linkIsUp)
    {
      (void) linkIsUp;
    }

    virtual void updateStreamStatus(uint64_t streamId, IasAvbStreamState status)
    {
      (void) streamId;
      (void) status;
    }
  };

  IasAvbStreamHandlerEventImpl streamHandlerEvent;
};

IasAvbProcessingResult IasTestTransmitEngine::createProperAudioStream(IasAvbClockDomain * clockDomain, IasAvbStreamId streamId)
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

IasAvbProcessingResult IasTestTransmitEngine::createProperVideoStream(IasAvbClockDomain * clockDomain, IasAvbStreamId streamId)
{
  uint16_t maxPacketRate = 24u;
  uint16_t maxPacketSize = 24u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbMacAddress destMacAddr = {0};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  IasAvbProcessingResult result = mTransmitEngine->createTransmitVideoStream(srClass,
                                                                             maxPacketRate,
                                                                             maxPacketSize,
                                                                             format,
                                                                             clockDomain,
                                                                             streamId,
                                                                             destMacAddr,
                                                                             true);
  return result;
}

IasAvbProcessingResult IasTestTransmitEngine::createProperCRStream(IasAvbClockDomain * clockDomain, uint64_t uStreamId)
{
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t crfStampsPerPdu          = 18u;
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 24000u;
  IasAvbClockMultiplier pull      = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  IasAvbStreamId streamId(uStreamId);
  IasAvbMacAddress dmac = {};

  IasAvbProcessingResult result = mTransmitEngine->createTransmitClockReferenceStream(srClass,
                                                                                      type,
                                                                                      crfStampsPerPdu,
                                                                                      crfStampInterval,
                                                                                      baseFreq,
                                                                                      pull,
                                                                                      clockDomain,
                                                                                      streamId,
                                                                                      dmac);
  return result;
}

TEST_F(IasTestTransmitEngine, CTor_DTor)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(NULL != mEnvironment);
}

TEST_F(IasTestTransmitEngine, isValidStreamId)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(NULL != mEnvironment);

  IasAvbStreamId streamId((uint64_t)0);

  ASSERT_FALSE(mTransmitEngine->isValidStreamId(streamId));
}


TEST_F(IasTestTransmitEngine, init)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbProcessingResult status = mTransmitEngine->init();
  ASSERT_EQ(eIasAvbProcOK, status);
  // already initialized
  ASSERT_TRUE(mTransmitEngine->isInitialized());
  ASSERT_EQ(eIasAvbProcInitializationFailed, mTransmitEngine->init());
}

TEST_F(IasTestTransmitEngine, StartNok)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbProcessingResult status = mTransmitEngine->start();
  ASSERT_EQ(eIasAvbProcNotInitialized, status);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(clockDomain));

  IasAvbTransmitSequencer *seq = mTransmitEngine->mSequencers[0];
  if (seq->mTransmitThread->isRunning())
  {
    seq->mTransmitThread->stop();
  }
  delete seq->mTransmitThread;
  seq->mTransmitThread = NULL;
  // (i < IasAvbTSpec::cIasAvbNumSupportedClasses) (T)
  // && (eIasAvbProcOK == result)                  (F)
  ASSERT_EQ(eIasAvbProcNotInitialized, mTransmitEngine->start());

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, StopNok)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbProcessingResult status = mTransmitEngine->stop();
  ASSERT_EQ(eIasAvbProcNotInitialized, status);
}

TEST_F(IasTestTransmitEngine, destroyAvbStream)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbStreamId streamId((uint64_t)0);
  IasAvbProcessingResult result = mTransmitEngine->destroyAvbStream(streamId);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestTransmitEngine, activateAvbStream)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbStreamId streamId((uint64_t(0u)));
  IasAvbProcessingResult result = mTransmitEngine->activateAvbStream(streamId);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(clockDomain));
  IasAvbStream *stream = mTransmitEngine->mAvbStreams.find(streamId)->second;
  IasAvbTransmitSequencer *seq = mTransmitEngine->getSequencerByStream(stream);
  if (seq->mTransmitThread->isRunning())
  {
    seq->mTransmitThread->stop();
  }
  delete seq->mTransmitThread;
  seq->mTransmitThread = NULL;
  // eIasAvbProcOK == result( = seq->addStreamToTransmitList(stream)) (F)
  ASSERT_EQ(eIasAvbProcNotInitialized, mTransmitEngine->activateAvbStream(streamId));
  ASSERT_FALSE(stream->isActive());

  stream->mActive = true;
  // !stream->isActive() (F)
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamId));

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, deactivateAvbStream)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbStreamId streamId((uint64_t(0u)));
  IasAvbProcessingResult result = mTransmitEngine->deactivateAvbStream(streamId);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mTransmitEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->deactivateAvbStream(streamId);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(clockDomain));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamId));
  IasAvbStream *stream = mTransmitEngine->mAvbStreams.find(streamId)->second;
  IasAvbTransmitSequencer *seq = mTransmitEngine->getSequencerByStream(stream);
  // (eIasAvbProcOK == result) && (mUseShaper) (T)
  mTransmitEngine->mUseShaper = true;
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->deactivateAvbStream(streamId));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(streamId));
  if (seq->mTransmitThread->isRunning())
  {
    seq->mTransmitThread->stop();
  }
  delete seq->mTransmitThread;
  seq->mTransmitThread = NULL;

  // eIasAvbProcOK == result( = seq->removeStreamFromTransmitList(stream)) (F)
  ASSERT_EQ(eIasAvbProcNotInitialized, mTransmitEngine->deactivateAvbStream(streamId));

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, connectAudioStreams)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbStreamId streamId((uint64_t(0u)));
  IasLocalAudioStream * localStream(NULL);
  IasAvbProcessingResult result = mTransmitEngine->connectAudioStreams(streamId, localStream);
  ASSERT_EQ(eIasAvbProcErr, result);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperVideoStream(clockDomain));
  // eIasAvbAudioStream == it->second->getStreamType() (F)
  ASSERT_EQ(eIasAvbProcErr, mTransmitEngine->connectAudioStreams(streamId, localStream));

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, createSequencerOnDemand_noMem)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  heapSpaceLeft = sizeof(IasAvbTransmitSequencer) - 1;
  // NULL == seq (T)
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mTransmitEngine->createSequencerOnDemand(IasAvbSrClass::eIasAvbSrClassHigh));

  heapSpaceLeft = sizeof(IasAvbTransmitSequencer) + sizeof(IasThread) - 1;
  // (0u == i) && (eIasAvbProcOK == result) (T && F)
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mTransmitEngine->createSequencerOnDemand(IasAvbSrClass::eIasAvbSrClassHigh));
}

TEST_F(IasTestTransmitEngine, createSequencerOnDemand_srClass)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->createSequencerOnDemand(IasAvbSrClass::eIasAvbSrClassHigh));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->createSequencerOnDemand(IasAvbSrClass::eIasAvbSrClassLow));
  ASSERT_EQ(eIasAvbProcNoSpaceLeft, mTransmitEngine->createSequencerOnDemand(IasAvbSrClass(2)));
}

TEST_F(IasTestTransmitEngine, UnregisterEvent)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  mTransmitEngine->unregisterEventInterface(&streamHandlerEvent);
  ASSERT_TRUE(1);
}

TEST_F(IasTestTransmitEngine, createTransmitAudioStreamOutOfMem)
{
  ASSERT_TRUE(LocalSetup());
  uint16_t const maxNumberChannels = 1;
  uint32_t const sampleFreq = 48000;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  IasAvbStreamId streamId((uint64_t)1);
  IasAvbMacAddress destMacAddr = {0};

  heapSpaceLeft = 0;

  IasAvbProcessingResult result = mTransmitEngine->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                                             sampleFreq,
                                                                             format,
                                                                             clockDomain,
                                                                             streamId,
                                                                             destMacAddr,
                                                                             true);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory , result);

  // 2nd branch: stream can be created, but not the sequencer
  heapSpaceLeft = 280;

  result = mTransmitEngine->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                                             sampleFreq,
                                                                             format,
                                                                             clockDomain,
                                                                             streamId,
                                                                             destMacAddr,
                                                                             true);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory , result);

  delete clockDomain;
}


TEST_F(IasTestTransmitEngine, createTransmitAudioStream)
{
  ASSERT_TRUE(LocalSetup());
  uint16_t const maxNumberChannels = 0;
  uint32_t const sampleFreq = 0;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  IasAvbStreamId streamId((uint64_t)0);
  IasAvbMacAddress destMacAddr = {0};

  IasAvbProcessingResult result = mTransmitEngine->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels,
                                                                             sampleFreq,
                                                                             format,
                                                                             clockDomain,
                                                                             streamId,
                                                                             destMacAddr,
                                                                             true);
  delete clockDomain;

  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestTransmitEngine, no_env)
{
  // No local setup of env
  ASSERT_EQ(eIasAvbProcInitializationFailed, mTransmitEngine->init());

}

TEST_F(IasTestTransmitEngine, BRANCH_AudioStream)
{
  ASSERT_TRUE(NULL != mTransmitEngine);

  ASSERT_TRUE(LocalSetup());

  IasAvbPtpClockDomain clockdomain;
  IasAvbStreamId streamID((uint64_t)0);
  IasAvbMacAddress macAddr = {1};

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mTransmitEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);


  result = mTransmitEngine->createTransmitAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, 2, 48000u,
                                   IasAvbAudioFormat::eIasAvbAudioFormatSaf16, &clockdomain, streamID, macAddr, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->activateAvbStream(streamID);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->start();
  ASSERT_EQ(eIasAvbProcOK, result);
  sleep(1);

  result = mTransmitEngine->stop();
  ASSERT_EQ(eIasAvbProcOK, result);
  sleep(1);

  result = mTransmitEngine->deactivateAvbStream(streamID);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->activateAvbStream(streamID);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->deactivateAvbStream(streamID);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->start();
  ASSERT_EQ(eIasAvbProcOK, result);
  sleep(1);

  result = mTransmitEngine->destroyAvbStream(streamID);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->stop();
  ASSERT_EQ(eIasAvbProcOK, result);
  sleep(1);
}

TEST_F(IasTestTransmitEngine, BRANCH_RegisterEvent)
{
  ASSERT_TRUE(NULL != mTransmitEngine);

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mTransmitEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  ASSERT_TRUE(LocalSetup());
  result = mTransmitEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAvbStreamHandlerEventInterface* eventInterface = NULL;
  result = mTransmitEngine->registerEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mTransmitEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcAlreadyInUse, result);
}

TEST_F(IasTestTransmitEngine, BRANCH_UnRegisterEvent)
{
  ASSERT_TRUE(NULL != mTransmitEngine);

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mTransmitEngine->unregisterEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  ASSERT_TRUE(LocalSetup());
  result = mTransmitEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAvbStreamHandlerEventInterface* eventInterface = NULL;
  result = mTransmitEngine->unregisterEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mTransmitEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcOK, result);

  // (NULL == eventInterface)               (F)
  // || (mEventInterface != eventInterface) (T)
  ASSERT_EQ(eIasAvbProcInvalidParam, mTransmitEngine->unregisterEventInterface(mTransmitEngine));

  result = mTransmitEngine->unregisterEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestTransmitEngine, CleanUp_stop_thread)
{
  ASSERT_TRUE(NULL != mTransmitEngine);

  IasAvbProcessingResult result = eIasAvbProcErr;

  ASSERT_TRUE(LocalSetup());
  result = mTransmitEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mTransmitEngine->start();
  ASSERT_EQ(eIasAvbProcOK, result);

  // transmit's thread should be stopped by cleanup
  usleep(10);
  mTransmitEngine->cleanup();
}

TEST_F(IasTestTransmitEngine, run_policy_option_other)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(LocalSetup(ePolicyOther));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->start());

  // transmit's thread should be stopped by cleanup
  usleep(10);
  mTransmitEngine->cleanup();
}

TEST_F(IasTestTransmitEngine, run_policy_option_rr)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(LocalSetup(ePolicyRr));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->start());

  // transmit's thread should be stopped by cleanup
  usleep(10);
  mTransmitEngine->cleanup();
}

TEST_F(IasTestTransmitEngine, run_policy_option_fifo)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(LocalSetup(ePolicyFifo));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->start());

  // transmit's thread should be stopped by cleanup
  usleep(10);
  mTransmitEngine->cleanup();
}

TEST_F(IasTestTransmitEngine, connectVideoStreams)
{
  ASSERT_TRUE(LocalSetup());
  IasAvbStreamId streamId((uint64_t(0u)));
  IasLocalVideoStream * localStream(NULL);
  IasAvbProcessingResult result = mTransmitEngine->connectVideoStreams(streamId, localStream);
  ASSERT_EQ(eIasAvbProcErr, result);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(clockDomain));
  // eIasAvbVideoStream == it->second->getStreamType() (F)
  ASSERT_EQ(eIasAvbProcErr, mTransmitEngine->connectVideoStreams(streamId, localStream));

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, createTransmitVideoStream)
{
  ASSERT_TRUE(LocalSetup());
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 0;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  IasAvbStreamId streamId((uint64_t)0);
  IasAvbMacAddress destMacAddr = {0};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  heapSpaceLeft = 0;

  IasAvbProcessingResult result = mTransmitEngine->createTransmitVideoStream(srClass,
                                                                             maxPacketRate,
                                                                             maxPacketSize,
                                                                             format,
                                                                             clockDomain,
                                                                             streamId,
                                                                             destMacAddr,
                                                                             true);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory , result);

  heapSpaceLeft = heapSpaceInitSize;

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  result = mTransmitEngine->createTransmitVideoStream(srClass,
                                                      maxPacketRate,
                                                      maxPacketSize,
                                                      format,
                                                      clockDomain,
                                                      streamId,
                                                      destMacAddr,
                                                      true);
  ASSERT_EQ(eIasAvbProcInvalidParam , result);

  maxPacketRate = 24u;
  maxPacketSize = 24u;
  format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;

  ASSERT_EQ(eIasAvbProcOK , mTransmitEngine->createTransmitVideoStream(srClass,
                                                                       maxPacketRate,
                                                                       maxPacketSize,
                                                                       format,
                                                                       clockDomain,
                                                                       streamId,
                                                                       destMacAddr,
                                                                       true));
  // mAvbStreams.end() != mAvbStreams.find(streamId) (F)
  ASSERT_EQ(eIasAvbProcInvalidParam , mTransmitEngine->createTransmitVideoStream(srClass,
                                                                                 maxPacketRate,
                                                                                 maxPacketSize,
                                                                                 format,
                                                                                 clockDomain,
                                                                                 streamId,
                                                                                 destMacAddr,
                                                                                 true));

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, getAvbStreamInfo)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  IasAvbClockDomain * clockDomain = new IasAvbPtpClockDomain();
  IasAvbStreamId videoStreamId((uint64_t(0u))), audioStreamId((uint64_t(1u))), otherStreamId((uint64_t(2u)));
  ASSERT_EQ(eIasAvbProcOK, createProperVideoStream(clockDomain, videoStreamId));

  AudioStreamInfoList returnedAudioInfo;
  VideoStreamInfoList returnedVideoInfo;
  ClockReferenceStreamInfoList returnedCRFInfo;
  // 1. out of 1
  ASSERT_TRUE(mTransmitEngine->getAvbStreamInfo(videoStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
  // no stream exists with such id
  ASSERT_FALSE(mTransmitEngine->getAvbStreamInfo(audioStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(clockDomain, audioStreamId));
  // 1. out of 2
  ASSERT_TRUE(mTransmitEngine->getAvbStreamInfo(videoStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
  // 2. out of 2
  ASSERT_TRUE(mTransmitEngine->getAvbStreamInfo(audioStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(clockDomain, otherStreamId));
  // 2. out of 3
  ASSERT_TRUE(mTransmitEngine->getAvbStreamInfo(audioStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, getAvbStreamInfo_ClockRef)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  IasAvbPtpClockDomain * clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperCRStream(clockDomain, 2u));

  AudioStreamInfoList returnedAudioInfo;
  VideoStreamInfoList returnedVideoInfo;
  ClockReferenceStreamInfoList returnedCRFInfo;
  IasAvbStreamId streamId(uint64_t(2u));
  ASSERT_TRUE(mTransmitEngine->getAvbStreamInfo(streamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
  ASSERT_EQ(0u, returnedAudioInfo.size());
  ASSERT_EQ(0u, returnedVideoInfo.size());
  ASSERT_EQ(1u, returnedCRFInfo.size());

  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, updateStreamStatus)
{
  ASSERT_TRUE(NULL != mTransmitEngine);

  IasAvbStreamId streamID((uint64_t)0);
  IasAvbStreamState stateNoData = IasAvbStreamState::eIasAvbStreamNoData;

  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  mTransmitEngine->updateStreamStatus(streamID, stateNoData);

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->registerEventInterface(&streamHandlerEvent));

  mTransmitEngine->updateStreamStatus(streamID, stateNoData);
}

TEST_F(IasTestTransmitEngine, get_setMaxFrameSizeHigh)
{
  ASSERT_TRUE(NULL != mTransmitEngine);

  ASSERT_TRUE(LocalSetup());

  uint64_t enableShapers = 1u;
  mEnvironment->setConfigValue(IasRegKeys::cXmitUseShaper, enableShapers);

  IasAvbClockDomain * clockdomain = new IasAvbPtpClockDomain();
  IasAvbStreamId lowStreamID((uint64_t)0);
  IasAvbStreamId highStreamID((uint64_t)1);
  IasAvbMacAddress macAddr = {1};
  IasAvbSrClass srClassLow = IasAvbSrClass::eIasAvbSrClassLow;
  IasAvbSrClass srClassHigh = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->createTransmitAudioStream(srClassLow,
                                                                      2u,
                                                                      48000u,
                                                                      audioFormat,
                                                                      clockdomain,
                                                                      lowStreamID,
                                                                      macAddr,
                                                                      true));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(lowStreamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->createTransmitAudioStream(srClassHigh,
                                                                      2u,
                                                                      48000u,
                                                                      audioFormat,
                                                                      clockdomain,
                                                                      highStreamID,
                                                                      macAddr,
                                                                      true));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(highStreamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->destroyAvbStream(lowStreamID));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->destroyAvbStream(highStreamID));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->createTransmitAudioStream(srClassHigh,
                                                                      2u,
                                                                      48000u,
                                                                      audioFormat,
                                                                      clockdomain,
                                                                      highStreamID,
                                                                      macAddr,
                                                                      true));

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->activateAvbStream(highStreamID));

  IasAvbTransmitSequencer * sequencer = mTransmitEngine->getSequencerByStream(mTransmitEngine->mAvbStreams[highStreamID]);
  ASSERT_TRUE(NULL != sequencer);
  sequencer->mMaxFrameSizeHigh = 3000u;

  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->destroyAvbStream(highStreamID));

  delete clockdomain;
}

TEST_F(IasTestTransmitEngine, updateLinkStatus)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->init());
  bool linkNotUp = false;

  mTransmitEngine->updateLinkStatus(linkNotUp);

  bool linkUp = true;
  mTransmitEngine->mUseShaper = true;
  mTransmitEngine->updateLinkStatus(linkUp);
}

TEST_F(IasTestTransmitEngine, updateShapers)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(LocalSetup());
  mEnvironment->setConfigValue(IasRegKeys::cXmitUseShaper, 1u);
  mTransmitEngine->init();
  mTransmitEngine->createSequencerOnDemand(IasAvbSrClass::eIasAvbSrClassLow);
  mTransmitEngine->createSequencerOnDemand(IasAvbSrClass::eIasAvbSrClassHigh);

  mTransmitEngine->updateShapers();

  IasAvbTransmitSequencer *seq = mTransmitEngine->getSequencerByClass(IasAvbSrClass::eIasAvbSrClassLow);
  ASSERT_TRUE(NULL != seq);
  seq->updateShaper();

  seq->mCurrentBandwidth = (uint32_t)(-1);
  seq->updateShaper();

  mTransmitEngine->cleanup();
}

TEST_F(IasTestTransmitEngine, createTransmitClockReferenceStream_noMem)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  mTransmitEngine->init();

  IasAvbPtpClockDomain *clockDomain = new IasAvbPtpClockDomain();
  heapSpaceLeft = sizeof(IasAvbClockReferenceStream) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, createProperCRStream(clockDomain));
  delete clockDomain;
}

TEST_F(IasTestTransmitEngine, disconnectStreams)
{
  ASSERT_TRUE(NULL != mTransmitEngine);
  ASSERT_TRUE(LocalSetup());
  mTransmitEngine->init();

  IasAvbStreamId streamId(uint64_t(1u));
  ASSERT_EQ(eIasAvbProcInvalidParam, mTransmitEngine->disconnectStreams(streamId));

  IasAvbPtpClockDomain *clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createProperCRStream(clockDomain, 1u));
  ASSERT_EQ(eIasAvbProcInvalidParam, mTransmitEngine->disconnectStreams(streamId));

  IasAvbStreamId audioStreamId(uint64_t(2u));
  ASSERT_EQ(eIasAvbProcOK, createProperVideoStream(clockDomain, audioStreamId));
  ASSERT_EQ(eIasAvbProcOK, mTransmitEngine->disconnectStreams(audioStreamId));

  delete clockDomain;
}

} // namespace IasMediaTransportAvb

