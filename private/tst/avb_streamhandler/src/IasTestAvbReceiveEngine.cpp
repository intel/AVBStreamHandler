/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
* @file IasTestAvbReceiveEngine.cpp
* @brief Test file for IasAvbReceiveEngine
* @date 2018
*/

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbReceiveEngine.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEventInterface.hpp"
#include "test_common/IasSpringVilleInfo.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbVideoStream.hpp"
#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include "avb_streamhandler/IasAvbAudioStream.hpp"
#undef protected
#undef private

#include "avb_helper/IasThread.hpp"

#include <sys/types.h>
#include <sys/socket.h>

#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <errno.h>

#include <iostream>
#include <string.h>

#include <boost/iostreams/device/file_descriptor.hpp>

using namespace std;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

#ifndef ETH_P_IEEE1722
#define ETH_P_IEEE1722 0x22F0
#endif

namespace IasMediaTransportAvb
{

const string cFnBase("tmpFile");

class IasTestAvbReceiveEngine : public ::testing::Test
{
  public:
    enum Configs {
        ePolicyOther,
        ePolicyRr,
        ePolicyFifo,
        ePolicyNone,
        eIdleWait_10K,
        eRxSocketRxbufSize
    };

protected:
  IasTestAvbReceiveEngine()
  : mAvbReceiveEngine(NULL)
  , mEnvironment(NULL)
  , mSocketFdList()
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAvbStreamHandlerEnvironment",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);
  }

  virtual ~IasTestAvbReceiveEngine()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->registerDltContexts();

    mAvbReceiveEngine = new IasAvbReceiveEngine();
  }

  virtual void TearDown()
  {
    delete mAvbReceiveEngine;
    mAvbReceiveEngine = NULL;

    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    if(0 < mSocketFdList.size())
    {
      for(auto &vecFd : mSocketFdList)
      {
        if(0 > close(vecFd))
          DLT_LOG(mDltContext,  DLT_LOG_ERROR, DLT_STRING("Error closing fd: "), DLT_INT(vecFd),
                    DLT_STRING(strerror(errno)));
      }

      mSocketFdList.clear();
    }

    heapSpaceLeft = heapSpaceInitSize;
  }

  bool LocalSetup(Configs config = ePolicyNone)
  {
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cNwIfName,  IasSpringVilleInfo::getInterfaceName()))
        {
          switch (config)
          {
            default:
              // Do Nothing
              break;
            case ePolicyOther:
            {
              if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "other"))
              {
                return false;
              }
            }
            break;
            case ePolicyRr:
            {
              if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "rr"))
              {
                return false;
              }
            }
            break;
            case ePolicyFifo:
            {
              if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "fifo"))
              {
                return false;
              }
            }
            break;
            case eIdleWait_10K:
            {
              if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cRxIdleWait, 10000))
              {
                return false;
              }
            }
            break;
            case eRxSocketRxbufSize:
            if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cRxSocketRxBufSize, 512))
            {
              return false;
            }
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
    }

    return false;
  }

  bool LocalHostSetup()
  {
    string interface("lo");

    if (false ==  LocalSetup())
    {
      return false;
    }

    if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cNwIfName, interface))
    {
      return false;
    }

    // The environment caches it, so we need to use our friend privileges to override the cache.
    mEnvironment->mInterfaceName = interface;

    return true;
  }

  bool LocalHostSetup2()
  {
    if(IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cRxDiscardAfter, 1))
    {
      return false;
    }

    if (IasAvbResult::eIasAvbResultOk != mEnvironment->setConfigValue(IasRegKeys::cRxIgnoreStreamId, 1u))
    {
      return false;
    }

    return LocalHostSetup();
  }

  IasAvbProcessingResult createProperAudioStream(IasAvbStreamId streamId = IasAvbStreamId(uint64_t(0u)), IasAvbMacAddress * dmac = NULL);
  IasAvbProcessingResult createProperVideoStream(IasAvbStreamId streamId = IasAvbStreamId(uint64_t(0u)));
  IasAvbProcessingResult createProperCRStream(uint64_t uStreamId = 0u);

  void createMaxFds();

  IasAvbReceiveEngine* mAvbReceiveEngine;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  std::vector<int32_t> mSocketFdList;
  DltContext mDltContext;

protected:

  class IasAvbStreamHandlerEventInterfaceImpl : public IasAvbStreamHandlerEventInterface
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
};

IasAvbProcessingResult IasTestAvbReceiveEngine::createProperAudioStream(IasAvbStreamId streamId, IasAvbMacAddress * dmac)
{
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;

  IasAvbMacAddress destMac = {0x00,0x00,0x00,0x00,0x00,0x00};
  if (NULL != dmac)
  {
    memcpy(destMac, dmac, sizeof(IasAvbMacAddress));
  }

  return mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh,
                                                     maxNumberChannels,
                                                     sampleFreq,
                                                     format,
                                                     streamId,
                                                     destMac,
                                                     true);
}
IasAvbProcessingResult IasTestAvbReceiveEngine::createProperVideoStream(IasAvbStreamId streamId)
{
  uint16_t maxPacketRate = 24u;
  uint16_t maxPacketSize = 24u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbMacAddress destMacAddr = {0};

  return mAvbReceiveEngine->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                     maxPacketRate,
                                                     maxPacketSize,
                                                     format,
                                                     streamId,
                                                     destMacAddr,
                                                     true);
}

IasAvbProcessingResult IasTestAvbReceiveEngine::createProperCRStream(uint64_t uStreamId)
{
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu           = 18u;
  IasAvbStreamId streamId(uStreamId);
  IasAvbMacAddress dmac = {};

  IasAvbProcessingResult result = mAvbReceiveEngine->createReceiveClockReferenceStream(srClass,
                                                                                       type,
                                                                                       maxCrfStampsPerPdu,
                                                                                       streamId,
                                                                                       dmac);
  return result;
}

void IasTestAvbReceiveEngine::createMaxFds()
{
  int32_t sFd = -1;
  while(0 <= (sFd = socket(PF_LOCAL, SOCK_DGRAM, 0)))
    mSocketFdList.push_back(sFd);

  if (0 > sFd)
    DLT_LOG(mDltContext,  DLT_LOG_INFO, DLT_STRING("Created max number of fd's: ["), DLT_INT(errno), DLT_STRING("]: "),
              DLT_STRING(strerror(errno)));
}

TEST_F(IasTestAvbReceiveEngine, Init)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;
  mEnvironment->setConfigValue(IasRegKeys::cRxIgnoreStreamId, 0u);
  // HEAP testing
  heapSpaceLeft = 0;
  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  mEnvironment->setConfigValue(IasRegKeys::cRxIgnoreStreamId, 1u);
  // HEAP testing
  heapSpaceLeft = sizeof(IasThread);
  result = mAvbReceiveEngine->init();
#if defined(DIRECT_RX_DMA)
  ASSERT_EQ(eIasAvbProcOK, result); // init() will return OK since receive buffer will not be allocated from heap
#else
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  // HEAP testing
  heapSpaceLeft = sizeof(IasThread) + sizeof(uint8_t) * ETH_FRAME_LEN + 4u;//IasAvbReceiveEngine::cReceiveBufferSize;
  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);
#endif
  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbReceiveEngine, RegisterEventInterface)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbStreamHandlerEventInterface* eventInterface = NULL;
  IasAvbStreamHandlerEventInterfaceImpl streamHandlerEvent;

  result = mAvbReceiveEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbReceiveEngine->registerEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mAvbReceiveEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbReceiveEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcAlreadyInUse, result);
}

TEST_F(IasTestAvbReceiveEngine, UnregisterEventInterface)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbStreamHandlerEventInterface* eventInterface = NULL;
  IasAvbStreamHandlerEventInterfaceImpl streamHandlerEvent;

  result = mAvbReceiveEngine->unregisterEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbReceiveEngine->unregisterEventInterface(eventInterface);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  // set a valid EventLister to test a proper unregistration case
  result = mAvbReceiveEngine->registerEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAvbStreamHandlerEventInterfaceImpl otherEventInterface;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbReceiveEngine->unregisterEventInterface(&otherEventInterface));

  result = mAvbReceiveEngine->unregisterEventInterface(&streamHandlerEvent);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbReceiveEngine, OpenReceiveSocket)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  mEnvironment->setDefaultConfigValues();

  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbReceiveEngine->openReceiveSocket());

  //
  // open max possible number of fd's for process to get ENFILE or EMFILE error
  //
  createMaxFds();
  // then try to open the receive socket
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbReceiveEngine->openReceiveSocket());

  // TODO:
  // openReceiveSocket segmentation fault occurs when StreamHandlerEnvironment is not initialized
  // no way to test fail cases until this problem is not resolved
}

TEST_F(IasTestAvbReceiveEngine, StartStop_Thread)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;

  ASSERT_TRUE(LocalSetup());

  // no thread object
  result = mAvbReceiveEngine->start();

  ASSERT_EQ(eIasAvbProcNullPointerAccess, result);

  result = mAvbReceiveEngine->stop();
  ASSERT_EQ(eIasAvbProcNullPointerAccess, result);

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  // try to stop not running thread
  result = mAvbReceiveEngine->stop();
  ASSERT_EQ(eIasAvbProcOK, result);

  // try with proper init
  result = mAvbReceiveEngine->start();
  ASSERT_EQ(eIasAvbProcOK, result);

  // receive socket already initialized
  result = mAvbReceiveEngine->start();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  usleep(100);
  result = mAvbReceiveEngine->stop();
  ASSERT_EQ(eIasAvbProcOK, result);

  // start thread properly once again
  result = mAvbReceiveEngine->start();
  ASSERT_EQ(eIasAvbProcOK, result);

  usleep(100);
  result = mAvbReceiveEngine->stop();
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbReceiveEngine, setSchedulingParameters_rr)
{
  ASSERT_TRUE(LocalSetup(ePolicyOther));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());

  IasThread::IasThreadSchedulingPolicy policy = IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyRR;
  int32_t priority = 1;
  IasThreadId const threadId = mAvbReceiveEngine->mReceiveThread->mThreadId;

  ASSERT_EQ(IasThreadResult::cOk, mAvbReceiveEngine->mReceiveThread->IasThread::setSchedulingParameters(threadId, policy, priority));

  mAvbReceiveEngine->mReceiveThread->getSchedulingParameters(threadId, policy, priority);

  ASSERT_EQ(IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyRR, policy);

  usleep(100);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
}

TEST_F(IasTestAvbReceiveEngine, setSchedulingParameters_fifo)
{
  ASSERT_TRUE(LocalSetup(ePolicyOther));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());

  ASSERT_EQ(true, mAvbReceiveEngine->mReceiveThread->IasThread::wasStarted());

  IasThread::IasThreadSchedulingPolicy policy = IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyFifo;
  int32_t priority = 1;
  IasThreadId const threadId = mAvbReceiveEngine->mReceiveThread->IasThread::getThreadId();
  const std::string longThreadName = "to_long_test_name_for_truncate_purpose";
  const std::string shortThreadName = "";
  mAvbReceiveEngine->mReceiveThread->IasThread::setThreadName(threadId, longThreadName);
  mAvbReceiveEngine->mReceiveThread->IasThread::setThreadName(threadId, shortThreadName);

  ASSERT_EQ(IasThreadResult::cOk, mAvbReceiveEngine->mReceiveThread->IasThread::setSchedulingParameters(policy, priority));

  mAvbReceiveEngine->mReceiveThread->getSchedulingParameters(policy, priority);

  ASSERT_EQ(IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyFifo, policy);

  usleep(100);

  mAvbReceiveEngine->mReceiveThread->IasThread::signal(0u);

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());

  IasThreadResult result = mAvbReceiveEngine->mReceiveThread->IasThread::getRunThreadResult();
  ASSERT_EQ(IasThreadResult::cOk, result);
}

TEST_F(IasTestAvbReceiveEngine, testRunPolicyOptions_other)
{
  ASSERT_TRUE(LocalSetup(ePolicyOther));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());

  IasThread::IasThreadSchedulingPolicy policy;
  int32_t priority;
  IasThreadId const threadId = mAvbReceiveEngine->mReceiveThread->mThreadId;
  mAvbReceiveEngine->mReceiveThread->getSchedulingParameters(threadId, policy, priority);
  ASSERT_EQ(ePolicyOther, policy);

  usleep(100);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
}

TEST_F(IasTestAvbReceiveEngine, testRunPolicyOptions_rr)
{
  ASSERT_TRUE(LocalSetup(ePolicyRr));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());
  usleep(100);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
}

TEST_F(IasTestAvbReceiveEngine, testRunPolicyOptions_fifo)
{
  ASSERT_TRUE(LocalSetup(ePolicyFifo));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());
  usleep(100);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
}

TEST_F(IasTestAvbReceiveEngine, testRunIdleWait_10K)
{
  ASSERT_TRUE(LocalSetup(eIdleWait_10K));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());
  usleep(100);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
}

TEST_F(IasTestAvbReceiveEngine, testRun_RxSocketRxbufSize)
{
  ASSERT_TRUE(LocalSetup(eRxSocketRxbufSize));
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());
  usleep(100);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
}

TEST_F(IasTestAvbReceiveEngine, CreateReceiveAudioStream)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  uint16_t maxNumberChannels = 0;
  uint32_t sampleFreq = 0;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSafFloat;
  IasAvbStreamId streamId_1(uint64_t(1)), streamId_2(uint64_t(2)), streamId_3(uint64_t(3));
  IasAvbMacAddress destMacAddr = {0};

  result = mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq,
                                                       format, streamId_1, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  maxNumberChannels = 2;
  sampleFreq = 48000;
  format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;

  result = mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq,
                                                       format, streamId_2, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  ASSERT_TRUE(mAvbReceiveEngine->isValidStreamId(streamId_2));

  result = mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq,
                                                       format, streamId_2, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcAlreadyInUse, result);

  // HEAP testing
  heapSpaceLeft = 0;

  result = mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq,
                                                       format, streamId_3, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);
}

TEST_F(IasTestAvbReceiveEngine, getAvbStreamInfo)
{

  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  IasAvbStreamId videoStreamId((uint64_t(0u))), audioStreamId((uint64_t(1u))), otherStreamId((uint64_t(2u)));
  ASSERT_EQ(eIasAvbProcOK, createProperVideoStream(videoStreamId));

  AudioStreamInfoList returnedAudioInfo;
  VideoStreamInfoList returnedVideoInfo;
  ClockReferenceStreamInfoList returnedCRFInfo;
  // 1. out of 1
  ASSERT_FALSE(mAvbReceiveEngine->getAvbStreamInfo(videoStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
  ASSERT_TRUE(0 < returnedVideoInfo.size());
  // no stream exists with such id
  ASSERT_FALSE(mAvbReceiveEngine->getAvbStreamInfo(audioStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));

  returnedVideoInfo.clear();
  returnedAudioInfo.clear();
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(audioStreamId));
  // 1. out of 2
  ASSERT_FALSE(mAvbReceiveEngine->getAvbStreamInfo(videoStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
  ASSERT_TRUE(0 < returnedVideoInfo.size());
  ASSERT_TRUE(0 < returnedAudioInfo.size());
  // 2. out of 2
  ASSERT_TRUE(mAvbReceiveEngine->getAvbStreamInfo(audioStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(otherStreamId));
  // 2. out of 3
  ASSERT_TRUE(mAvbReceiveEngine->getAvbStreamInfo(audioStreamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
}

TEST_F(IasTestAvbReceiveEngine, getAvbStreamInfo_ClockRef)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, createProperCRStream(2u));

  AudioStreamInfoList returnedAudioInfo;
  VideoStreamInfoList returnedVideoInfo;
  ClockReferenceStreamInfoList returnedCRFInfo;
  IasAvbStreamId streamId(uint64_t(2u));
  ASSERT_TRUE(mAvbReceiveEngine->getAvbStreamInfo(streamId, returnedAudioInfo, returnedVideoInfo, returnedCRFInfo));
  ASSERT_EQ(0u, returnedAudioInfo.size());
  ASSERT_EQ(0u, returnedVideoInfo.size());
  ASSERT_EQ(1u, returnedCRFInfo.size());
}

TEST_F(IasTestAvbReceiveEngine, DestroyAvbStream)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbStreamId streamId(uint64_t(1));
  IasAvbMacAddress destMacAddr = {0};

  result = mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq,
                                                       format, streamId, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAvbStreamId streamIdWrong(uint64_t(0));

  result = mAvbReceiveEngine->destroyAvbStream(streamIdWrong);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mAvbReceiveEngine->destroyAvbStream(streamId);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbReceiveEngine, ConnectAudioStreams)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbStreamId streamId(uint64_t(1));
  IasAvbMacAddress destMacAddr = {0};

  result = mAvbReceiveEngine->createReceiveAudioStream(IasAvbSrClass::eIasAvbSrClassHigh, maxNumberChannels, sampleFreq,
                                                       format, streamId, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAvbStreamId streamIdWrong(uint64_t(0));
  IasLocalAudioStream* localStream = NULL;

  result = mAvbReceiveEngine->connectAudioStreams(streamIdWrong, localStream);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasAvbStreamId videoStreamId(uint64_t(2u));
  ASSERT_EQ(eIasAvbProcOK, createProperVideoStream(videoStreamId));
  // eIasAvbAudioStream == it->second.stream->getStreamType() (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbReceiveEngine->connectAudioStreams(videoStreamId, localStream));

  result = mAvbReceiveEngine->connectAudioStreams(streamId, localStream);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbReceiveEngine, localhost_run)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  ASSERT_TRUE(LocalHostSetup());

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());

  int sendSocket;
  struct ifreq ifr;
  int32_t ifindex;

  // By using SOCK_DGRAM we get the physical layer populated automatically.
  if ((sendSocket = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IEEE1722))) < 0)
  {
    printf("Error creating socket [%s]\n", strerror(errno));
    ASSERT_TRUE(false);
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, "lo", (sizeof ifr.ifr_name) - 1u);
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

  if (ioctl(sendSocket, SIOCGIFINDEX, &ifr) == -1)
  {
    printf("Error getting socket if index [%s]\n", strerror(errno));
    close(sendSocket);
    ASSERT_TRUE(false);
  }
  else
  {
    printf("ifr.ifr_name %s\n", ifr.ifr_name);
    ifindex = ifr.ifr_ifindex;
  }

  sockaddr_ll addr;
  uint8_t buffer[1024];
  memset(&buffer, 'A', sizeof buffer);

  memset(&addr, 0, sizeof addr);
  addr.sll_family = AF_PACKET;
  addr.sll_ifindex = ifindex;
  addr.sll_protocol = htons(ETH_P_IEEE1722);

  for (int count = 0; count < 3; count++)
  {
    sleep(1);
    if (sendto(sendSocket, &buffer, sizeof buffer, 0, (struct sockaddr*)&addr, (socklen_t)sizeof addr) < 0)
    {
      printf("Error sending packet %d errno (%d) [%s]\n", count, errno, strerror(errno));
    }
    else
    {
      printf("Sending packet %d ...\n", count);
    }
  }

  buffer[1] = 0x80;
  for (int count = 0; count < 3; count++)
  {
    sleep(1);
    if (sendto(sendSocket, &buffer, sizeof buffer, 0, (struct sockaddr*)&addr, (socklen_t)sizeof addr) < 0)
    {
      printf("Error sending packet %d errno (%d) [%s]\n", count, errno, strerror(errno));
    }
    else
    {
      printf("Sending packet %d ...\n", count);
    }
  }

  close(sendSocket);

  sleep(3);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
  sleep(3);
}

TEST_F(IasTestAvbReceiveEngine, localhost_run2)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  ASSERT_TRUE(LocalHostSetup2());

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());

  int sendSocket;
  struct ifreq ifr;
  int32_t ifindex;

  // By using SOCK_DGRAM we get the physical layer populated automatically.
  if ((sendSocket = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IEEE1722))) < 0)
  {
    printf("Error creating socket [%s]\n", strerror(errno));
    ASSERT_TRUE(false);
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, "lo", (sizeof ifr.ifr_name) - 1u);
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

  if (ioctl(sendSocket, SIOCGIFINDEX, &ifr) == -1)
  {
    printf("Error getting socket if index [%s]\n", strerror(errno));
    close(sendSocket);
    ASSERT_TRUE(false);
  }
  else
  {
    printf("ifr.ifr_name %s\n", ifr.ifr_name);
    ifindex = ifr.ifr_ifindex;
  }

  sockaddr_ll addr;
  uint8_t buffer[1024];
  memset(&buffer, 'A', sizeof buffer);

  memset(&addr, 0, sizeof addr);
  addr.sll_family = AF_PACKET;
  addr.sll_ifindex = ifindex;
  addr.sll_protocol = htons(ETH_P_IEEE1722);

  buffer[1] = (0x81);
  for (int count = 0; count < 3; count++)
  {
    sleep(1);
    if (sendto(sendSocket, &buffer, sizeof buffer, 0, (struct sockaddr*)&addr, (socklen_t)sizeof addr) < 0)
    {
      printf("Error sending packet %d errno (%d) [%s]\n", count, errno, strerror(errno));
    }
    else
    {
      printf("Sending packet %d ...\n", count);
    }
  }

  close(sendSocket);

  sleep(3);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
  sleep(3);
}

TEST_F(IasTestAvbReceiveEngine, localhost_run_wildcardMac_sent)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  ASSERT_TRUE(LocalHostSetup());

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream());

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->start());

  int sendSocket;
  struct ifreq ifr;
  int32_t ifindex;

  // By using SOCK_DGRAM we get the physical layer populated automatically.
  if ((sendSocket = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IEEE1722))) < 0)
  {
    printf("Error creating socket [%s]\n", strerror(errno));
    ASSERT_TRUE(false);
  }

  boost::iostreams::file_descriptor socket_descriptor(sendSocket, boost::iostreams::close_handle);

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, "lo", (sizeof ifr.ifr_name) - 1u);
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

  if (ioctl(sendSocket, SIOCGIFINDEX, &ifr) == -1)
  {
    printf("Error getting socket if index [%s]\n", strerror(errno));
    close(sendSocket);
    ASSERT_TRUE(false);
  }
  else
  {
    printf("ifr.ifr_name %s\n", ifr.ifr_name);
    ifindex = ifr.ifr_ifindex;
  }

  sockaddr_ll addr;
  uint8_t buffer[1024];
  memset(&buffer, 'A', sizeof buffer);

  memset(&addr, 0, sizeof addr);
  addr.sll_family = AF_PACKET;
  addr.sll_ifindex = ifindex;
  addr.sll_protocol = htons(ETH_P_IEEE1722);
  IasAvbMacAddress dmac = {0x91, 0x80, 0xF0, 0x00, 0xFE, 0x01};
  memcpy(buffer, dmac, sizeof(IasAvbMacAddress));

  for (int count = 0; count < 3; count++)
  {
    usleep(200);
    if (1 == count)
    {
      mAvbReceiveEngine->mAvbStreams.clear();

      IasAvbStreamId streamId(uint64_t(0u));
      IasAvbMacAddress mac = {'A', 'A', 'A', 'A', 'A', 'A'};
      ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(streamId, &mac));
    }
    else if (2 == count)
    {
      memset(&buffer, 0, sizeof buffer);
      buffer[1] = 0x80;

      mAvbReceiveEngine->mAvbStreams.clear();

      IasAvbStreamId streamId(uint64_t(0u));
      IasAvbMacAddress mac = {'A', 'A', 'A', 'A', 'A', 'A'};
      ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(streamId, &mac));
    }
    if (sendto(sendSocket, &buffer, sizeof buffer, 0, (struct sockaddr*)&addr, (socklen_t)sizeof addr) < 0)
    {
      printf("Error sending packet %d errno (%d) [%s]\n", count, errno, strerror(errno));
    }
    else
    {
      printf("\nSending packet %d ...\n\n", count);
    }
  }

  sleep(3);
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->stop());
  sleep(3);
}

TEST_F(IasTestAvbReceiveEngine, ConnectVideoStreams)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mAvbReceiveEngine->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  uint16_t const maxPacketRate = 0;
  uint16_t const maxPacketSize = 0;
  IasAvbVideoFormat const format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  IasAvbStreamId streamId(uint64_t(1));
  IasAvbMacAddress destMacAddr = {0};

  result = mAvbReceiveEngine->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow, maxPacketRate, maxPacketSize,
                                                       format, streamId, destMacAddr, true);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasAvbStreamId streamIdWrong(uint64_t(0));
  IasLocalVideoStream* localStream = NULL;

  result = mAvbReceiveEngine->connectVideoStreams(streamIdWrong, localStream);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasAvbStreamId audioStreamId(uint64_t(2u));
  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream(audioStreamId));
  // eIasAvbVideoStream == it->second.stream->getStreamType() (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbReceiveEngine->connectVideoStreams(audioStreamId, localStream));

}

TEST_F(IasTestAvbReceiveEngine, createReceiveVideoStream)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());

  uint16_t maxPacketRate = 24u;
  uint16_t maxPacketSize = 24u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbMacAddress destMacAddr = {0};
  IasAvbStreamId streamId(uint64_t(0));

  heapSpaceLeft = sizeof(IasAvbVideoStream) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAvbReceiveEngine->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                                    maxPacketRate,
                                                                                    maxPacketSize,
                                                                                    format,
                                                                                    streamId,
                                                                                    destMacAddr,
                                                                                    true));

  heapSpaceLeft = heapSpaceInitSize;

  //streamId already in use

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                       maxPacketRate,
                                                                       maxPacketSize,
                                                                       format,
                                                                       streamId,
                                                                       destMacAddr,
                                                                       true));

  ASSERT_EQ(eIasAvbProcAlreadyInUse, mAvbReceiveEngine->createReceiveVideoStream(IasAvbSrClass::eIasAvbSrClassLow,
                                                                                 maxPacketRate,
                                                                                 maxPacketSize,
                                                                                 format,
                                                                                 streamId,
                                                                                 destMacAddr,
                                                                                 true));
}

TEST_F(IasTestAvbReceiveEngine, closeSocket)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  mAvbReceiveEngine->mReceiveSocket = 0;
  mAvbReceiveEngine->closeSocket();
  ASSERT_EQ(-1, mAvbReceiveEngine->mReceiveSocket);
}

TEST_F(IasTestAvbReceiveEngine, cleanup)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  mAvbReceiveEngine->mReceiveSocket = 0;
  mAvbReceiveEngine->cleanup();
}

TEST_F(IasTestAvbReceiveEngine, checkStreamState)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createProperAudioStream());
  IasAvbReceiveEngine::StreamData streamData;
  streamData.stream = mAvbReceiveEngine->mAvbStreams.begin()->second.stream;
  streamData.lastState = IasAvbStreamState::eIasAvbStreamNoData;
  // streamData.lastState != newState
  ASSERT_FALSE(mAvbReceiveEngine->checkStreamState(streamData));

  IasAvbStreamHandlerEventInterfaceImpl eventInterface;
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->registerEventInterface(&eventInterface));
  streamData.lastState = IasAvbStreamState::eIasAvbStreamNoData;
  // streamData.lastState != newState
  ASSERT_FALSE(mAvbReceiveEngine->checkStreamState(streamData));
}

TEST_F(IasTestAvbReceiveEngine, createReceiveClockReferenceStream)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  IasAvbSrClass srClass               = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu           = 12u;
  IasAvbStreamId streamId(0x91E0F000FE000000u);
  IasAvbMacAddress dmac               = {0};

  heapSpaceLeft = sizeof(IasAvbClockReferenceStream) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAvbReceiveEngine->createReceiveClockReferenceStream(srClass,
                                                                                             type,
                                                                                             maxCrfStampsPerPdu,
                                                                                             streamId,
                                                                                             dmac));
}

TEST_F(IasTestAvbReceiveEngine, createReceiveAudioStream)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);

  IasAvbSrClass srClass               = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels            = 2u;
  uint32_t sampleFreq                   = 24000u;
  IasAvbAudioFormat            format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbStreamId streamId(0x91E0F000FE000000u);
  IasAvbMacAddress dmac               = {0};
  bool preconfigured                  = true;

  heapSpaceLeft = sizeof(IasAvbAudioStream) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAvbReceiveEngine->createReceiveAudioStream(srClass,
                                                                                    maxNumberChannels,
                                                                                    sampleFreq,
                                                                                    format,
                                                                                    streamId,
                                                                                    dmac,
                                                                                    preconfigured));
}

TEST_F(IasTestAvbReceiveEngine, disconnectStreams)
{
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());

  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbReceiveEngine->disconnectStreams(IasAvbStreamId(uint64_t(0u))));

  IasAvbSrClass srClass               = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu           = 12u;
  IasAvbStreamId streamId(0x91E0F000FE000000u);
  IasAvbMacAddress dmac               = {0};

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->createReceiveClockReferenceStream(srClass,
                                                                                type,
                                                                                maxCrfStampsPerPdu,
                                                                                streamId,
                                                                                dmac));

  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbReceiveEngine->disconnectStreams(streamId));
}

#if defined(DIRECT_RX_DMA)
TEST_F(IasTestAvbReceiveEngine, startIgbReceiveEngine)
{dlt_enable_local_print();
  ASSERT_TRUE(mAvbReceiveEngine != NULL);
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbReceiveEngine->startIgbReceiveEngine());

  ASSERT_TRUE(LocalHostSetup());

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->init());

  heapSpaceLeft = sizeof(IasAvbPacketPool) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAvbReceiveEngine->startIgbReceiveEngine());
  heapSpaceLeft = sizeof(IasAvbPacketPool) + IasAvbReceiveEngine::cReceivePoolSize * sizeof(IasAvbPacket) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAvbReceiveEngine->startIgbReceiveEngine());
  heapSpaceLeft = heapSpaceInitSize;

  ASSERT_EQ(eIasAvbProcOK, mAvbReceiveEngine->startIgbReceiveEngine());

  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbReceiveEngine->startIgbReceiveEngine());
  usleep(100);
  mAvbReceiveEngine->stopIgbReceiveEngine();
}
#endif

} // IasMediaTransportAvb
