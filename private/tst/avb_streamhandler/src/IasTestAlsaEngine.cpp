/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestAlsaEngine.cpp
 * @brief   The implementation of the IasTestAlsaEngine test class.
 * @date    2016
 */

#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"
#include "avb_streamhandler/IasAlsaWorkerThread.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAlsaEngine.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#define protected protected
#define private private
#include "test_common/IasSpringVilleInfo.hpp"


using namespace IasMediaTransportAvb;
using std::nothrow;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb
{

class IasTestAlsaEngine : public ::testing::Test
{
protected:
  IasTestAlsaEngine()
    : mAlsaEngine(NULL),
      mEnvironment(NULL),
      mAlsaAudioFormat(IasAvbAudioFormat::eIasAvbAudioFormatSaf16)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAlsaEngine()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
#if VERBOSE_TEST_PRINTOUT
    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_VERBOSE);
#else
    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_ERROR);
#endif
    ASSERT_TRUE(NULL != mEnvironment);

    // ensure that we do not use the dummy context with default loglevel
    mEnvironment->registerDltContexts();

    mAlsaEngine = new (nothrow) IasAlsaEngine();
    ASSERT_NE(nullptr, mAlsaEngine);

    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAlsaEngine",
              DLT_LOG_VERBOSE,
              DLT_TRACE_STATUS_ON);
    dlt_enable_local_print();

    heapSpaceLeft = heapSpaceInitSize;
  }

  virtual void TearDown()
  {
    if (NULL != mEnvironment)
    {
      delete mEnvironment;
      mEnvironment = NULL;
    }

    if (nullptr != mAlsaEngine)
    {
      delete mAlsaEngine;
      mAlsaEngine = nullptr;
    }

    heapSpaceLeft = heapSpaceInitSize;
  }

  IasAvbProcessingResult createDefaultStream(uint16_t streamId = 0u, IasAvbClockDomain * clockDomain = NULL);

  IasAlsaEngine* mAlsaEngine;
  DltContext mDltContext;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  IasAvbAudioFormat mAlsaAudioFormat;

protected:

  // helper class
  class IasLocalAudioStreamClientInterfaceImpl : public IasLocalAudioStreamClientInterface
  {
  public:
    IasLocalAudioStreamClientInterfaceImpl():
      mReturn(false)
    {}
    virtual ~IasLocalAudioStreamClientInterfaceImpl(){}

    virtual bool signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples) { (void) event; (void) numSamples; return mReturn; }
    virtual void updateRelativeFillLevel(int32_t relFillLevel) { (void) relFillLevel; }
    virtual uint32_t getMaxTransmitTime() { return 0u; }
    virtual uint32_t getMinTransmitBufferSize(uint32_t periodCycle) { (void)periodCycle; return 0u; }

    bool mReturn;
  };

  bool initEnvironment()
  {
    if(NULL == mEnvironment)
    {
      return false;
    }
    mEnvironment->setDefaultConfigValues();

    if (IasSpringVilleInfo::fetchData())
    {
      IasSpringVilleInfo::printDebugInfo();

      if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName()))
      {
        if (eIasAvbProcOK == mEnvironment->createIgbDevice())
        {
          if (eIasAvbProcOK == mEnvironment->createPtpProxy())
          {
          return true;
          }
        }
      }
    }
    return false;
  }
};

IasAvbProcessingResult IasTestAlsaEngine::createDefaultStream(uint16_t streamId, IasAvbClockDomain * clockDomain)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  IasAvbStreamDirection direction   = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numChannels              = 2u;
  uint32_t sampleFrequency          = 24000u;
  IasAvbAudioFormat format          = mAlsaAudioFormat;
  uint32_t periodSize               = 8u;
  uint32_t numPeriods               = 3u;
  uint8_t channelLayout             = 2u;
  bool hasSideChannel               = true;
  std::string deviceName            = "avbtestdev";
  IasAvbPtpClockDomain ptpClockDomain;
  IasAlsaDeviceTypes alsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000u;
  if (NULL == clockDomain)
    clockDomain = &ptpClockDomain;

  result = mAlsaEngine->createAlsaStream(direction, numChannels, sampleFrequency, format,
                                         periodSize, numPeriods, channelLayout, hasSideChannel,
                                         deviceName, streamId, clockDomain, alsaDeviceType, sampleFreqASRC);
  return result;
}

TEST_F(IasTestAlsaEngine, start)
{
  ASSERT_TRUE(NULL != mAlsaEngine);

  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaEngine->start());

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->start());

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->stop());

  ASSERT_EQ(eIasAvbProcOK, createDefaultStream());

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->start());

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->stop());
}

TEST_F(IasTestAlsaEngine, init)
{
  ASSERT_TRUE(NULL != mAlsaEngine);

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());

  ASSERT_EQ(eIasAvbProcInitializationFailed, mAlsaEngine->init());
}

TEST_F(IasTestAlsaEngine, run)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  IasAvbStreamDirection direction   = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numChannels              = 2u;
  uint32_t sampleFrequency          = 24000u;
  IasAvbAudioFormat format          = mAlsaAudioFormat;
  uint32_t periodSize               = 8u;
  uint32_t numPeriods               = 3u;
  uint8_t channelLayout             = 2u;
  bool hasSideChannel               = true;
  std::string deviceName               = "avbtestdev";
  uint16_t streamId                 = 0u;
  IasAvbPtpClockDomain *clockDomain = new IasAvbPtpClockDomain();
  std::string policyStr("other");
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000u;

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         streamId,
                                                         clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));


  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, policyStr));
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAlsaClockGain, 0.0f));
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, "other"));
  clockDomain->mEventRate = 0u;
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->start());
  sleep(1);
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->stop());

  if (nullptr != clockDomain)
  {
    delete clockDomain;
    clockDomain = nullptr;
  }
}

TEST_F(IasTestAlsaEngine, createAlsaStream_timeAwareModeOff)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  ASSERT_TRUE(initEnvironment());
  IasAvbStreamDirection direction   = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numChannels              = 2u;
  uint32_t sampleFrequency          = 24000u;
  IasAvbAudioFormat format          = mAlsaAudioFormat;
  uint32_t periodSize               = 8u;
  uint32_t numPeriods               = 3u;
  uint8_t channelLayout             = 2u;
  bool hasSideChannel               = true;
  std::string deviceName            = "avbtestdev";
  uint16_t streamId                 = 0u;
  IasAvbPtpClockDomain *clockDomain = new IasAvbPtpClockDomain();
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000u;

  IasLocalAudioBufferDesc::AudioBufferDescMode timeAwareMode = IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, timeAwareMode));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         streamId,
                                                         clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));

  uint32_t basePeriod = 16u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cAlsaBasePeriod, basePeriod));

  deviceName = "avbtestdev_c";

  uint32_t totalLocalBufferSize = periodSize * IasAlsaEngine::cMinNumberAlsaBuffer - 1;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cAlsaRingBufferSz, totalLocalBufferSize));

  std::string optName = std::string(IasRegKeys::cAlsaDevicePeriods) + deviceName + "_c";
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(optName, IasAlsaEngine::cMinNumberAlsaBuffer - 1));
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         ++streamId,
                                                         clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));

  IasAlsaEngine::AlsaViDevStreamMap::iterator it = mAlsaEngine->mAlsaViDevStreams.find(streamId);
  if (it != mAlsaEngine->mAlsaViDevStreams.end())
  {
    IasAlsaVirtualDeviceStream* alsaStream = it->second;
    ASSERT_EQ(basePeriod, alsaStream->getDiagnostics()->getBasePeriod());
  }

  if (nullptr != clockDomain)
  {
    delete clockDomain;
    clockDomain = nullptr;
  }
}

TEST_F(IasTestAlsaEngine, destroyAlsaStream)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  uint16_t streamId                 = 0u;

  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaEngine->destroyAlsaStream(streamId, true));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());
  // try to destroy non-existing stream
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->destroyAlsaStream(streamId, true));

  ASSERT_EQ(eIasAvbProcOK, createDefaultStream(streamId));

  // connect and activate stream
  {
    IasLocalAudioStream* localStream = NULL;

    localStream = mAlsaEngine->getLocalAudioStream(streamId);
    ASSERT_TRUE(NULL != localStream);

    IasLocalAudioStreamClientInterfaceImpl client;
    localStream->connect(&client);

    // try to destroy connected stream
    ASSERT_EQ(eIasAvbProcAlreadyInUse, mAlsaEngine->destroyAlsaStream(streamId, false));
    localStream->disconnect();
  }

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->destroyAlsaStream(streamId, false));
}

TEST_F(IasTestAlsaEngine, cleanup)
{
  ASSERT_TRUE(NULL != mAlsaEngine);

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createDefaultStream());
  ASSERT_EQ(1, mAlsaEngine->mWorkerThreads.size());

  mAlsaEngine->cleanup();
  ASSERT_TRUE(mAlsaEngine->mAlsaViDevStreams.empty());
  ASSERT_EQ(0, mAlsaEngine->mWorkerThreads.size());
}

TEST_F(IasTestAlsaEngine, stop)
{
  ASSERT_TRUE(NULL != mAlsaEngine);

  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaEngine->stop());
}

TEST_F(IasTestAlsaEngine, getWorkerThread)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  uint16_t streamId = 0u;

  ASSERT_EQ(NULL, mAlsaEngine->getWorkerThread(streamId));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());

  ASSERT_EQ(eIasAvbProcOK, createDefaultStream(streamId));
  IasAvbPtpClockDomain ptpClockDomain;

  IasAlsaEngine::AlsaViDevStreamMap::iterator it = mAlsaEngine->mAlsaViDevStreams.find(streamId);
  if (it != mAlsaEngine->mAlsaViDevStreams.end())
  {
    IasAlsaVirtualDeviceStream* stream = it->second;
    ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->assignToWorkerThread(stream, &ptpClockDomain));
  }

  ASSERT_EQ(NULL, mAlsaEngine->getWorkerThread(++streamId));
}

TEST_F(IasTestAlsaEngine, removeFromWorkerThread)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  IasAlsaVirtualDeviceStream * nullStream = NULL;

  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaEngine->removeFromWorkerThread(nullStream));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());

  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->removeFromWorkerThread(nullStream));

  uint16_t streamId = 0u;
  IasAlsaVirtualDeviceStream * nonWTStream = new IasAlsaVirtualDeviceStream(mDltContext, IasAvbStreamDirection::eIasAvbTransmitToNetwork, streamId);

  ASSERT_EQ(eIasAvbProcErr, mAlsaEngine->removeFromWorkerThread(nonWTStream));

  streamId += 1u;
  ASSERT_EQ(eIasAvbProcOK, createDefaultStream(streamId));

  IasAlsaEngine::AlsaViDevStreamMap::iterator it = mAlsaEngine->mAlsaViDevStreams.find(streamId);
  if (it != mAlsaEngine->mAlsaViDevStreams.end())
  {
    IasAlsaVirtualDeviceStream * stream = it->second;
    ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->removeFromWorkerThread(stream));
    delete stream;
    mAlsaEngine->mAlsaViDevStreams.clear();
  }

  delete nonWTStream;
  nonWTStream = NULL;
}

TEST_F(IasTestAlsaEngine, assignToWorkerThread)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  IasAlsaVirtualDeviceStream * nullStream     = NULL;
  IasAvbClockDomain * nullDomain = NULL;

  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaEngine->assignToWorkerThread(nullStream, nullDomain));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());

  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->assignToWorkerThread(nullStream, nullDomain));

  uint16_t streamId = 0u;
  IasAlsaVirtualDeviceStream * nonWTStream = new IasAlsaVirtualDeviceStream(mDltContext, IasAvbStreamDirection::eIasAvbTransmitToNetwork, streamId);

  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->assignToWorkerThread(nonWTStream, nullDomain));

  streamId += 1u;
  IasAvbPtpClockDomain *clockDomain = new IasAvbPtpClockDomain();
  ASSERT_EQ(eIasAvbProcOK, createDefaultStream(streamId, clockDomain));

  IasAlsaEngine::AlsaViDevStreamMap::iterator it = mAlsaEngine->mAlsaViDevStreams.find(streamId);
  IasAlsaVirtualDeviceStream * stream;
  if (it != mAlsaEngine->mAlsaViDevStreams.end())
  {
    stream = it->second;

    stream->mSampleFrequency = 0u;
    ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->assignToWorkerThread(stream, nullDomain));

    stream->mSampleFrequency = 24000u;
    heapSpaceLeft = sizeof(IasAlsaWorkerThread) - 1;
    ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAlsaEngine->assignToWorkerThread(stream, nullDomain));
    heapSpaceLeft = heapSpaceInitSize;

    // (*it)->checkParameter(...) (F)
    ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->assignToWorkerThread(stream, nullDomain));

    if (nullptr != nonWTStream)
    {
      delete nonWTStream;
      nonWTStream = NULL;
    }
    // create a stream matching the already created one
    streamId += 1u;
    nonWTStream = new IasAlsaVirtualDeviceStream(mDltContext, IasAvbStreamDirection::eIasAvbReceiveFromNetwork, streamId);
    uint16_t numChannels              = 2u;
    uint32_t totalLocalBufferSize     = 8u * 12u;
    uint32_t optimalFillLevel         = totalLocalBufferSize/2u;
    uint32_t periodSize               = 8u;
    uint32_t numAlsaBuffers           = IasAlsaEngine::cMinNumberAlsaBuffer;
    uint32_t sampleFrequency          = 24000u;
    IasAvbAudioFormat format          = mAlsaAudioFormat;
    uint8_t channelLayout             = 2u;
    bool hasSideChannel               = true;
    std::string deviceName            = "avbtestdev";
    IasAlsaDeviceTypes alsaDeviceType = eIasAlsaVirtualDevice;

    ASSERT_EQ(eIasAvbProcOK, nonWTStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, periodSize,
                                               numAlsaBuffers, sampleFrequency, format, channelLayout,
                                               hasSideChannel, deviceName, alsaDeviceType));

    // Adding stream to existing worker thread fail
    ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->assignToWorkerThread(stream, clockDomain));
  }

  delete clockDomain;
  clockDomain = nullptr;

  delete nonWTStream;
  nonWTStream = nullptr;
}

TEST_F(IasTestAlsaEngine, createAlsaStream)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numChannels              = 2u;
  uint32_t sampleFrequency          = 24000u;
  IasAvbAudioFormat format        = mAlsaAudioFormat;
  uint32_t periodSize               = 8u;
  uint32_t numPeriods               = 3u;
  uint8_t channelLayout             = 2u;
  bool hasSideChannel             = true;
  std::string deviceName               = "avbtestdev";
  uint16_t streamId                 = 0u;
  IasAvbPtpClockDomain clockDomain;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000u;
  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaEngine->createAlsaStream(direction,
                                                                     numChannels,
                                                                     sampleFrequency,
                                                                     format,
                                                                     periodSize,
                                                                     numPeriods,
                                                                     channelLayout,
                                                                     hasSideChannel,
                                                                     deviceName,
                                                                     streamId,
                                                                     &clockDomain,
                                                                     useAlsaDeviceType,
                                                                     sampleFreqASRC));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());
  heapSpaceLeft = sizeof(IasAlsaVirtualDeviceStream) - 1;

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAlsaEngine->createAlsaStream(direction,
                                                                      numChannels,
                                                                      sampleFrequency,
                                                                      format,
                                                                      periodSize,
                                                                      numPeriods,
                                                                      channelLayout,
                                                                      hasSideChannel,
                                                                      deviceName,
                                                                      streamId,
                                                                      &clockDomain,
                                                                      useAlsaDeviceType,
                                                                      sampleFreqASRC));

  heapSpaceLeft += sizeof(mAlsaEngine);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAlsaEngine->createAlsaStream(direction,
                                                                      numChannels,
                                                                      sampleFrequency,
                                                                      format,
                                                                      periodSize,
                                                                      numPeriods,
                                                                      channelLayout,
                                                                      hasSideChannel,
                                                                      deviceName,
                                                                      streamId,
                                                                      &clockDomain,
                                                                      useAlsaDeviceType,
                                                                      sampleFreqASRC));

  heapSpaceLeft += sizeof(IasAlsaWorkerThread) + sizeof(IasLocalAudioBuffer) + periodSize * sizeof(float) * sizeof(char);
  // not enough mem to create new worker thread
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAlsaEngine->createAlsaStream(direction,
                                                                      numChannels,
                                                                      sampleFrequency,
                                                                      format,
                                                                      periodSize,
                                                                      numPeriods,
                                                                      channelLayout,
                                                                      hasSideChannel,
                                                                      deviceName,
                                                                      streamId,
                                                                      &clockDomain,
                                                                      useAlsaDeviceType,
                                                                      sampleFreqASRC));

  heapSpaceLeft = heapSpaceInitSize;
  std::string policyStr     = "rr";
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, policyStr));
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->start());

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         streamId,
                                                         &clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->stop());
  //stream id already in use
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->createAlsaStream(direction,
                                                                   numChannels,
                                                                   sampleFrequency,
                                                                   format,
                                                                   periodSize,
                                                                   numPeriods,
                                                                   channelLayout,
                                                                   hasSideChannel,
                                                                   deviceName,
                                                                   streamId,
                                                                   &clockDomain,
                                                                   useAlsaDeviceType,
                                                                   sampleFreqASRC));

  deviceName = "avbtestdev2";
  policyStr     = "other";
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cSchedPolicy, policyStr));

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         ++streamId,
                                                         &clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));

  // device name is already in use
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         ++streamId,
                                                         &clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));
}

TEST_F(IasTestAlsaEngine, getLocalStreamInfo)
{
  ASSERT_TRUE(NULL != mAlsaEngine);
  ASSERT_TRUE(initEnvironment());
  IasAvbStreamDirection direction   = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numChannels              = 2u;
  uint32_t sampleFrequency          = 24000u;
  IasAvbAudioFormat format          = mAlsaAudioFormat;
  uint32_t periodSize               = 8u;
  uint32_t numPeriods               = 3u;
  uint8_t channelLayout             = 2u;
  bool hasSideChannel               = true;
  std::string deviceName            = "avbtestdev";
  uint16_t streamId                 = 0u;
  IasAvbPtpClockDomain clockDomain;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  uint32_t sampleFreqASRC           = 48000u;

  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->init());
  ASSERT_EQ(eIasAvbProcOK, mAlsaEngine->createAlsaStream(direction,
                                                         numChannels,
                                                         sampleFrequency,
                                                         format,
                                                         periodSize,
                                                         numPeriods,
                                                         channelLayout,
                                                         hasSideChannel,
                                                         deviceName,
                                                         streamId,
                                                         &clockDomain,
                                                         useAlsaDeviceType,
                                                         sampleFreqASRC));
  LocalAudioStreamInfoList audioStreamInfoList;
  ASSERT_FALSE(mAlsaEngine->getLocalStreamInfo(0u, audioStreamInfoList));
  ASSERT_FALSE(audioStreamInfoList.empty());
  ASSERT_EQ(direction, audioStreamInfoList[0].getDirection());
  ASSERT_EQ(numChannels, audioStreamInfoList[0].getNumChannels());
  ASSERT_EQ(sampleFrequency, audioStreamInfoList[0].getSampleFrequency());
  ASSERT_EQ(format, audioStreamInfoList[0].getFormat());
  ASSERT_EQ(periodSize, audioStreamInfoList[0].getPeriodSize());
  ASSERT_EQ(numPeriods, audioStreamInfoList[0].getNumPeriods());
  ASSERT_EQ(channelLayout, audioStreamInfoList[0].getChannelLayout());
  ASSERT_EQ(hasSideChannel, audioStreamInfoList[0].getHasSideChannel());
  deviceName = "avb_" + deviceName + "_c";
  ASSERT_EQ(deviceName, audioStreamInfoList[0].getDeviceName());
  ASSERT_EQ(streamId, audioStreamInfoList[0].getStreamId());
  ASSERT_FALSE(audioStreamInfoList[0].getConnected());

  const IasLocalAudioStreamDiagnostics & diag = audioStreamInfoList[0].getStreamDiagnostics();
  ASSERT_EQ(128u, diag.getBasePeriod());
  ASSERT_EQ(48000u, diag.getBaseFreq());
  ASSERT_EQ(0u, diag.getResetBuffersCount());
  ASSERT_EQ(0u, diag.getDeviationOutOfBounds());
  ASSERT_EQ(periodSize * numPeriods, diag.getTotalBufferSize());
  ASSERT_EQ(15u, diag.getBaseFillMultiplier());
  ASSERT_EQ(0u, diag.getBaseFillMultiplierTx());
  ASSERT_EQ(12u, diag.getBufferReadThreshold());
  ASSERT_EQ(2000000u, diag.getCycleWait());
}

TEST_F(IasTestAlsaEngine, attrs_diags)
{
  uint32_t basePeriod = 0u;
  uint32_t baseFreq = 1u;
  uint32_t baseFillMultiplier = 2u;
  uint32_t baseFillMultiplierTx = 3u;
  uint32_t cycleWait = 4u;
  uint32_t totalBufferSize = 5u;
  uint32_t bufferReadThreshold = 6u;
  uint32_t resetBuffersCount = 7u;
  uint32_t deviationOutOfBounds = 8u;
  IasLocalAudioStreamDiagnostics diag(basePeriod, baseFreq, baseFillMultiplier, baseFillMultiplierTx, cycleWait,
                                      totalBufferSize, bufferReadThreshold, resetBuffersCount, deviationOutOfBounds);

  IasAvbStreamDirection iDirection = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t iNumChannels = 1u;
  uint32_t iSampleFrequency = 2u;
  IasAvbAudioFormat iFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t iPeriodSize = 3u;
  uint32_t iNumPeriods = 4u;
  uint8_t iChannelLayout = 5u;
  bool iHasSideChannel = false;
  std::string iDeviceName("deviceName");
  uint16_t iStreamId = 6u;
  bool connected = true;
  IasLocalAudioStreamAttributes attrs(iDirection, iNumChannels, iSampleFrequency, iFormat, iPeriodSize, iNumPeriods,
                                      iChannelLayout, iHasSideChannel, iDeviceName, iStreamId, connected, diag);
  IasLocalAudioStreamAttributes otherAttrs;

  IasLocalAudioStreamDiagnostics otherDiag;

  ASSERT_FALSE(attrs == otherAttrs);
  ASSERT_TRUE(attrs != otherAttrs);
  ASSERT_FALSE(diag == otherDiag);

  otherAttrs = attrs;

  otherDiag.setDeviationOutOfBounds(1u);
  otherDiag = diag;
  diag = diag;
  IasLocalAudioStreamDiagnostics dDiag(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u);
  ASSERT_TRUE(dDiag != diag);
}

} // namespace IasMediaTransportAvb
