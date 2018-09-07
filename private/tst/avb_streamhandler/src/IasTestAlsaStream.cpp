/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestAlsaStream.cpp
 * @brief   The implementation of the IasTestAlsaStream test class.
 * @date    2013
 */

#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasAlsaStreamInterface.hpp"
#include "avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"
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
  // helper class
  class IasLocalAudioStreamClientInterfaceImpl : public IasLocalAudioStreamClientInterface
  {
  public:
    IasLocalAudioStreamClientInterfaceImpl(bool returnVal):
      mReturn(returnVal)
    {}
    virtual ~IasLocalAudioStreamClientInterfaceImpl(){}

    virtual bool signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples) { (void) event; (void) numSamples; return mReturn; }
    virtual void updateRelativeFillLevel(int32_t relFillLevel) { (void) relFillLevel; }
    virtual uint32_t getMaxTransmitTime() { return 0u; }
    virtual uint32_t getMinTransmitBufferSize(uint32_t periodCycle) { (void)periodCycle; return 0u; }

    bool mReturn;
  };

class IasTestAlsaStream : public ::testing::Test
{
protected:
  IasTestAlsaStream()
    : mAlsaStream(NULL),
      mEnvironment(NULL),
      mAlsaAudioFormat(IasAvbAudioFormat::eIasAvbAudioFormatSaf16)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAlsaStream()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);

    IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
    uint16_t streamId = 0;

    mAlsaStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, direction, streamId);

    mTestClient = new (nothrow) IasLocalAudioStreamClientInterfaceImpl(false);

    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAlsaStream",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);

    heapSpaceLeft = heapSpaceInitSize;
  }

  virtual void TearDown()
  {
    delete mAlsaStream;
    mAlsaStream = NULL;

    if (NULL != mEnvironment)
    {
      delete mEnvironment;
      mEnvironment = NULL;
    }

    delete mTestClient;

    heapSpaceLeft = heapSpaceInitSize;
  }

  // create ptp proxy for the time-aware audio buffer
  bool LocalSetup()
  {
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();

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

  bool TstampBufSetup()
  {
    bool setupResult = false;

    if (mEnvironment && mAlsaStream)
    {
      // set the tstamp key
      uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe);
      if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode))
      {
        mEnvironment->setConfigValue(IasRegKeys::cAudioBaseFillMultiplier, 15);
        mEnvironment->setConfigValue(IasRegKeys::cAudioBaseFillMultiplierTx, 20);
        mEnvironment->setConfigValue(IasRegKeys::cXmitWndWidth, 17000000);

        IasAvbProcessingResult initResult = eIasAvbProcErr;

        uint16_t numChannels          = 2;
        uint32_t alsaPeriodSize       = 256;
        uint32_t numAlsaBuffers       = 3;
        uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
        uint32_t alsaSampleFrequency  = 48000;
        uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
        IasAvbAudioFormat format      = mAlsaAudioFormat;
        uint8_t  channelLayout        = 0;
        bool   hasSideChannel         = false;
        std:string deviceName         = "AlsaTest";
        IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

        initResult = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                                   alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
        if (eIasAvbProcOK == initResult)
        {
          setupResult = true;
        }
      }
    }

    return setupResult;
  }

 // IasAlsaStreamInterface* mAlsaStream;
  IasAlsaVirtualDeviceStream* mAlsaStream;
  DltContext mDltContext;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  IasAvbAudioFormat mAlsaAudioFormat;
  IasLocalAudioStreamClientInterfaceImpl * mTestClient;
};

TEST_F(IasTestAlsaStream, Init)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t totalLocalBufferSize = 0; // basically a valid value (derived class uses a different mechanism)
  uint32_t numAlsaBuffers       = 2;
  uint32_t alsaSampleFrequency  = 48000;
  uint32_t alsaPeriodSize       = 0;
  uint32_t optimalFillLevel     = 2;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std:string deviceName         = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  alsaPeriodSize = 256;
  totalLocalBufferSize = 2;
  numAlsaBuffers = 0;

  result = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  numAlsaBuffers = 2;
  deviceName     = "";

  result = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  totalLocalBufferSize = 32;
  heapSpaceLeft = 0;
  deviceName    = "AlsaTest";

  result = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  heapSpaceLeft = sizeof(mAlsaStream);

  result = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  heapSpaceLeft = heapSpaceInitSize;
  totalLocalBufferSize = 2;
//  numAlsaBuffers = 0;

  result = mAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAlsaStream, ResetBuffers)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mAlsaStream->resetBuffers();
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAlsaStream, WriteLocalAudioBuffer)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t channelIdx = 0;
  uint32_t bufferSize = 1024;
  IasLocalAudioBuffer::AudioData buffer[bufferSize];
  uint16_t samplesWritten = 0;
  uint32_t timeStamp = 0u;

  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  uint16_t numChannels          = 2;
  uint32_t numAlsaFrames        = 32;
  uint32_t numAlsaBuffers       = 2;
  uint32_t alsaSampleFrequency  = 48000;
  uint32_t alsaPeriodSize       = 256;
  uint32_t optimalFillLevel     = 2;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std:string deviceName         = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;


  result = mAlsaStream->init(numChannels, numAlsaFrames, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  // the time-aware buffer can hold samples of totalSize given by the 2nd arg of init()
  ASSERT_EQ(numAlsaFrames, samplesWritten);

  uint32_t otherBufferSize = 1u;
  samplesWritten = 0u;
  IasLocalAudioBuffer::AudioData otherBuffer[otherBufferSize];
  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, otherBuffer, otherBufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(0u, samplesWritten);

  samplesWritten = 0u;
  mAlsaStream->mClient = mTestClient;
  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, otherBuffer, otherBufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(0u, samplesWritten);

  samplesWritten = 0u;
  mAlsaStream->mClient = mTestClient;
  mAlsaStream->mClientState = IasLocalAudioStream::ClientState::eIasActive;
  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, otherBuffer, otherBufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(0u, samplesWritten);

  samplesWritten = 0u;
  IasLocalAudioStreamClientInterfaceImpl * testClient = new IasLocalAudioStreamClientInterfaceImpl(true);
  mAlsaStream->mClient = testClient;
  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(0u, samplesWritten);

  channelIdx = 3;

  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  channelIdx = 0;
  bufferSize = 0;

  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasLocalAudioBuffer::AudioData* nullBuffer = NULL;
  bufferSize = 1024;

  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, nullBuffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  channelIdx = numChannels;

  result = mAlsaStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  ASSERT_TRUE(0u == mAlsaStream->getCurrentTimestamp());

  delete testClient;
}

TEST_F(IasTestAlsaStream, ReadLocalAudioBuffer)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t channelIdx = 0;
  uint32_t bufferSize = 1024;
  IasLocalAudioBuffer::AudioData buffer[bufferSize];
  uint16_t samplesRead = 0;
  uint64_t timeStamp = 0u;

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  result = mAlsaStream->dumpFromLocalAudioBuffer(samplesRead);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  uint16_t numChannels          = 2;
  uint32_t numAlsaFrames        = 32;
  uint32_t numAlsaBuffers       = 2;
  uint32_t alsaSampleFrequency  = 48000;
  uint32_t alsaPeriodSize       = 256;
  uint32_t optimalFillLevel     = 2;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std:string deviceName           = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mAlsaStream->init(numChannels, numAlsaFrames, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  channelIdx = 3;

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  samplesRead = 0;
  result = mAlsaStream->dumpFromLocalAudioBuffer(samplesRead);
  ASSERT_EQ(eIasAvbProcOK, result);

  channelIdx = 0;
  bufferSize = 0;

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasLocalAudioBuffer::AudioData* nullBuffer = NULL;
  bufferSize = 1024;

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, nullBuffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  channelIdx = numChannels;

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAlsaStream, ReadLocalAudioBuffer_branch)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t channelIdx = 0;
  uint32_t bufferSize = 1024;
  IasLocalAudioBuffer::AudioData buffer[bufferSize];
  uint16_t samplesRead = 0;
  uint64_t timeStamp = 0u;

  uint16_t numChannels          = 2;
  uint32_t numAlsaFrames        = 32;
  uint32_t numAlsaBuffers       = 2;
  uint32_t alsaSampleFrequency  = 48000;
  uint32_t alsaPeriodSize       = 256;
  uint32_t optimalFillLevel     = 2;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std:string deviceName         = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mAlsaStream->init(numChannels, numAlsaFrames, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);

  ASSERT_EQ(eIasAvbProcOK, result);

  mAlsaStream->mClient = mTestClient;
  mAlsaStream->mClientState = IasLocalAudioStream::ClientState::eIasIdle;
  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  samplesRead = 0;
  mAlsaStream->mClientState = IasLocalAudioStream::ClientState::eIasActive;
  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  samplesRead = 0;
  mAlsaStream->mClientState = IasLocalAudioStream::ClientState::eIasActive;
  mTestClient->mReturn = true;
  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAlsaStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAlsaStream, setChannelLayout)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  uint16_t numChannels          = 2u;
  uint32_t totalLocalBufferSize = 2u;
  uint32_t optimalFillLevel     = 2u;
  uint32_t alsaPeriodSize       = 2u;
  uint32_t numAlsaBuffers       = 2u;
  uint32_t alsaSampleFrequency  = 48000u;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0u;
  bool   hasSideChannel         = true;
  std:string deviceName         = "avbtestdev";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  ASSERT_EQ(eIasAvbProcOK, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));

  ASSERT_EQ(eIasAvbProcErr, mAlsaStream->setChannelLayout(channelLayout));
}

TEST_F(IasTestAlsaStream, IasLocalAudioStream_getChannelLayout)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  IasLocalAudioStream * localStream = static_cast<IasLocalAudioStream*>(mAlsaStream);
  ASSERT_EQ(0u, localStream->getChannelLayout());
}

TEST_F(IasTestAlsaStream, nextCycle)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  ASSERT_TRUE(mAlsaStream->nextCycle(1u));
}

TEST_F(IasTestAlsaStream, LocalAudioStream_connect)
{
  ASSERT_TRUE(NULL != mAlsaStream);

  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaStream->connect(NULL));

  mAlsaStream->mClient = mTestClient;
  ASSERT_EQ(eIasAvbProcAlreadyInUse, mAlsaStream->connect(mTestClient));
}

TEST_F(IasTestAlsaStream, LocalAudioStream_setClientActive)
{
  ASSERT_TRUE(NULL != mAlsaStream);
  uint16_t numChannels          = 2u;
  uint32_t totalLocalBufferSize = 2u;
  uint32_t optimalFillLevel     = 2u;
  uint32_t alsaPeriodSize       = 2u;
  uint32_t numAlsaBuffers       = 2u;
  uint32_t alsaSampleFrequency  = 48000u;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0u;
  bool   hasSideChannel         = true;
  std:string deviceName         = "avbtestdev";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  ASSERT_EQ(eIasAvbProcOK, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));

  mAlsaStream->setClientActive(false);
  ASSERT_EQ(IasLocalAudioStream::ClientState::eIasNotConnected, mAlsaStream->mClientState);

  mAlsaStream->mClient = mTestClient;
  mAlsaStream->setClientActive(false);
  ASSERT_EQ(IasLocalAudioStream::ClientState::eIasIdle, mAlsaStream->mClientState);

  mAlsaStream->setClientActive(true);
  ASSERT_EQ(IasLocalAudioStream::ClientState::eIasActive, mAlsaStream->mClientState);

  mAlsaStream->setClientActive(true);
  ASSERT_EQ(IasLocalAudioStream::ClientState::eIasActive, mAlsaStream->mClientState);
}

TEST_F(IasTestAlsaStream, LocalAudioStream_init)
{
  ASSERT_TRUE(NULL != mAlsaStream);
  uint16_t numChannels          = 2u;
  uint32_t totalLocalBufferSize = 2u;
  uint32_t optimalFillLevel     = 2u;
  uint32_t alsaPeriodSize       = 2u;
  uint32_t numAlsaBuffers       = 2u;
  uint32_t alsaSampleFrequency  = 48000u;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t  channelLayout        = 0u;
  bool   hasSideChannel         = true;
  std:string deviceName         = "avbtestdev";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  heapSpaceLeft = sizeof (IasLocalAudioBuffer) + totalLocalBufferSize * sizeof (IasLocalAudioBuffer::AudioData)- 1;

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));

  heapSpaceLeft = heapSpaceInitSize;

  numChannels = 1u;
  hasSideChannel = true;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));

  numChannels = 0u;
  hasSideChannel = false;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));

  numChannels = 2u;
  alsaSampleFrequency = 0u;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));

  alsaSampleFrequency = 24000u;
  numChannels = 1u;
  hasSideChannel = true;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));


  alsaSampleFrequency = 24000u;
  numChannels = 2u;
  hasSideChannel = false;
  ASSERT_EQ(eIasAvbProcOK, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAlsaStream->init(numChannels,
                                             totalLocalBufferSize,
                                             optimalFillLevel,
                                             alsaPeriodSize,
                                             numAlsaBuffers,
                                             alsaSampleFrequency,
                                             format,
                                             channelLayout,
                                             hasSideChannel,
                                             deviceName,
                                             useAlsaDeviceType));
}

TEST_F(IasTestAlsaStream, getDeviceName)
{
  ASSERT_TRUE(NULL != mAlsaStream);
  ASSERT_TRUE(NULL == mAlsaStream->getDeviceName());
}

TEST_F(IasTestAlsaStream, Init_tstamp)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(TstampBufSetup());
}

TEST_F(IasTestAlsaStream, ResetBuffers_tstamp)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(TstampBufSetup());
  ASSERT_EQ(eIasAvbProcOK, mAlsaStream->resetBuffers());
}

TEST_F(IasTestAlsaStream, WriteLocalAudioBuffer_tstamp)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(TstampBufSetup());

  (void) mAlsaStream->setWorkerActive(true);

  IasLocalAudioStreamClientInterfaceImpl * testClient = new IasLocalAudioStreamClientInterfaceImpl(true);
  mAlsaStream->mClient = testClient;
  mAlsaStream->mClientState = IasLocalAudioStream::ClientState::eIasActive;

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint32_t bufferSize = 64;

  const uint32_t alsaPeriodSize       = 256;
  const uint32_t numAlsaBuffers       = 3;
  const uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);

  IasLocalAudioBuffer::AudioData buffer[totalLocalBufferSize];
  uint16_t samplesWritten = 0;
  uint32_t timeStamp = 0u;

  timeStamp = mAlsaStream->getCurrentTimestamp();
  ASSERT_TRUE(0u == timeStamp);

  for (uint32_t channel = 0; channel < mAlsaStream->getNumChannels(); channel++)
  {
    for (uint32_t samples = 0; samples < totalLocalBufferSize; samples = samples + bufferSize)
    {
      result = mAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timeStamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      ASSERT_EQ(bufferSize, samplesWritten);
    }
  }

  // overflow
  result = mAlsaStream->writeLocalAudioBuffer(0u, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(64, samplesWritten);

  ASSERT_EQ(eIasAvbProcOK, mAlsaStream->resetBuffers());

  bufferSize = totalLocalBufferSize;
  result = mAlsaStream->writeLocalAudioBuffer(0u, buffer, bufferSize, samplesWritten, timeStamp);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(bufferSize / 2u, samplesWritten);

  timeStamp = mAlsaStream->getCurrentTimestamp();
  ASSERT_TRUE(0u != timeStamp);

  delete testClient;
}

TEST_F(IasTestAlsaStream, ReadLocalAudioBuffer_tstamp)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(TstampBufSetup());

  (void) mAlsaStream->setWorkerActive(true);

  IasLocalAudioStreamClientInterfaceImpl * testClient = new IasLocalAudioStreamClientInterfaceImpl(true);
  mAlsaStream->mClient = testClient;
  mAlsaStream->mClientState = IasLocalAudioStream::ClientState::eIasActive;

  IasAvbProcessingResult result = eIasAvbProcErr;

  const uint32_t bufferSize = 64;

  const uint32_t alsaPeriodSize       = 256;
  const uint32_t numAlsaBuffers       = 3;
  const uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);

  IasLocalAudioBuffer::AudioData buffer[bufferSize];
  uint16_t samplesRead = 0;
  uint16_t samplesWritten = 0;
  uint32_t timeStampAvb = 0u;
  uint64_t timeStampPtp = 0u;

  for (uint32_t channel = 0; channel < mAlsaStream->getNumChannels(); channel++)
  {
    for (uint32_t samples = 0; samples < totalLocalBufferSize; samples = samples + bufferSize)
    {
      result = mAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timeStampAvb);
      ASSERT_EQ(eIasAvbProcOK, result);
      ASSERT_EQ(bufferSize, samplesWritten);
    }
  }

  for (uint32_t channel = 0; channel < mAlsaStream->getNumChannels(); channel++)
  {
    for (uint32_t samples = 0; samples < totalLocalBufferSize; samples = samples + bufferSize)
    {
      result = mAlsaStream->readLocalAudioBuffer(channel, buffer, bufferSize, samplesRead, timeStampPtp);
      ASSERT_EQ(eIasAvbProcOK, result);
      ASSERT_EQ(bufferSize, samplesRead);
    }
  }

  delete testClient;
}

TEST_F(IasTestAlsaStream, connect_tstamp)
{
  ASSERT_TRUE(LocalSetup());
  ASSERT_TRUE(TstampBufSetup());

  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaStream->connect(NULL));
  ASSERT_EQ(eIasAvbProcOK, mAlsaStream->connect(mTestClient));
  ASSERT_EQ(eIasAvbProcAlreadyInUse, mAlsaStream->connect(mTestClient));
}


} // namespace
