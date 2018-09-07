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
#include "../../../inc/avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "test_common/IasSpringVilleInfo.hpp"
#include "internal/audio/common/alsa_smartx_plugin/IasAlsaPluginIpc.hpp"
#include "internal/audio/common/audiobuffer/IasAudioRingBuffer.hpp"
#include "internal/audio/common/audiobuffer/IasAudioRingBufferReal.hpp"
#define protected protected
#define private private

using namespace IasMediaTransportAvb;
using std::nothrow;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb
{

class IasTestAvbAudioShmProvider : public ::testing::Test
{
protected:
  IasTestAvbAudioShmProvider()
    : mTxAlsaStream(NULL),
      mAlsaShm(NULL),
      mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbAudioShmProvider()
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
    mTxAlsaStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, direction, streamId);
    ASSERT_TRUE(NULL != mTxAlsaStream);

    direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
    mRxAlsaStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, direction, streamId);
    ASSERT_TRUE(NULL != mRxAlsaStream);

    mAlsaShm = new (nothrow) IasAvbAudioShmProvider("test");
    ASSERT_TRUE(NULL != mAlsaShm);

    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAvbAudioShmProvider",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);

    heapSpaceLeft = heapSpaceInitSize;
  }

  virtual void TearDown()
  {
    if (NULL != mTxAlsaStream)
    {
      delete mTxAlsaStream;
      mTxAlsaStream = NULL;
    }

    if (NULL != mRxAlsaStream)
    {
      delete mRxAlsaStream;
      mRxAlsaStream = NULL;
    }

    if (NULL != mAlsaShm)
    {
      delete mAlsaShm;
      mAlsaShm = NULL;
    }

    if (NULL != mEnvironment)
    {
      delete mEnvironment;
      mEnvironment = NULL;
    }

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

//  IasAlsaStreamInterface* mTxAlsaStream;
//  IasAlsaStreamInterface* mRxAlsaStream;
  IasAlsaVirtualDeviceStream* mTxAlsaStream;
  IasAlsaVirtualDeviceStream* mRxAlsaStream;
  IasAvbAudioShmProvider* mAlsaShm;

  DltContext mDltContext;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

// helper class
class IasLocalAudioStreamClientInterfaceImpl : public IasLocalAudioStreamClientInterface
{
public:
  IasLocalAudioStreamClientInterfaceImpl(){}
  virtual ~IasLocalAudioStreamClientInterfaceImpl(){}

  virtual bool signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples) { (void) event; (void) numSamples; return false; }
  virtual void updateRelativeFillLevel(int32_t relFillLevel) { (void) relFillLevel; }
  virtual uint32_t getMaxTransmitTime() { return 0u; }
  virtual uint32_t getMinTransmitBufferSize(uint32_t periodCycle) { (void)periodCycle; return 0u; }
};

TEST_F(IasTestAvbAudioShmProvider, copyJob_tx)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mTxAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mTxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = false; // Tx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalAudioStream::LocalAudioBufferVec buffers = mTxAlsaStream->getChannelBuffers();
  IasLocalAudioBufferDesc * descQ = mTxAlsaStream->getBufferDescQ();

  uint32_t numFrames = 0u;
  bool dummy = true;
  uint64_t timestamp = 0u;

  numFrames = 0u; // invalid numFrames
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcErr, result);

  numFrames = 128u; // invalid numFrames
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcErr, result);

  numFrames = alsaPeriodSize; // valid numFrames
  dummy = true; // dummy
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  dummy = false;
  timestamp = 0u; // zero timestamp
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  timestamp = 1u; // non-zero timestamp
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAlsaShm->cleanup();
  ASSERT_EQ(eIasAvbProcOK, result);

  // copyJob after cleanup
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcErr, result);
}

TEST_F(IasTestAvbAudioShmProvider, copyJob_tx_tstamp)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mTxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe);
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode));

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mTxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = false; // Tx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalAudioStream::LocalAudioBufferVec buffers = mTxAlsaStream->getChannelBuffers();
  IasLocalAudioBufferDesc * descQ = mTxAlsaStream->getBufferDescQ();

  uint32_t numFrames = alsaPeriodSize;
  bool dummy = false;
  uint64_t timestamp = 1u;
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbAudioShmProvider, copyJob_tx_tstamp_invalidmode)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mTxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeLast);
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode));

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mTxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = false; // Tx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalAudioStream::LocalAudioBufferVec buffers = mTxAlsaStream->getChannelBuffers();
  IasLocalAudioBufferDesc * descQ = mTxAlsaStream->getBufferDescQ();

  uint32_t numFrames = alsaPeriodSize;
  bool dummy = false;
  uint64_t timestamp = 1u;
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbAudioShmProvider, copyJob_rx)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mRxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = true; // Rx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalAudioStream::LocalAudioBufferVec buffers = mRxAlsaStream->getChannelBuffers();
  IasLocalAudioBufferDesc * descQ = mRxAlsaStream->getBufferDescQ();

  ASSERT_TRUE(mRxAlsaStream->isReadReady());

  uint32_t numFrames = alsaPeriodSize;
  bool dummy = false;
  uint64_t timestamp = 1u;

  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbAudioShmProvider, copyJob_rx_tstamp)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe);
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode));

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format    = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel       = false;
  std::string deviceName           = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mRxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = true; // Rx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalAudioStream::LocalAudioBufferVec buffers = mRxAlsaStream->getChannelBuffers();
  IasLocalAudioBufferDesc * descQ = mRxAlsaStream->getBufferDescQ();

  uint32_t numFrames = alsaPeriodSize;
  bool dummy = false;
  uint64_t timestamp = 1u;

  ASSERT_FALSE(mRxAlsaStream->isReadReady());

  // empty read
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // fill the local buffer by dummy data
  (void) mRxAlsaStream->setWorkerActive(true);

  IasLocalAudioStreamClientInterfaceImpl * testClient = new IasLocalAudioStreamClientInterfaceImpl();
  ASSERT_TRUE(NULL != testClient);

  result = mRxAlsaStream->connect(testClient);
  ASSERT_EQ(eIasAvbProcOK, result);

  uint32_t bufferSize = 64;
  IasLocalAudioBuffer::AudioData buffer[bufferSize];
  uint16_t samplesWritten = 0;

  IasLibPtpDaemon* ptpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
  ASSERT_TRUE(NULL != ptpProxy);
  timestamp = uint32_t(ptpProxy->getLocalTime());

  for (uint32_t samples = 0; samples < totalLocalBufferSize / 2; samples = samples + bufferSize)
  {
    for (uint32_t channel = 0; channel < mRxAlsaStream->getNumChannels(); channel++)
    {
      result = mRxAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timestamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      ASSERT_EQ(bufferSize, samplesWritten);
    }
    timestamp++;
  }

  ASSERT_TRUE(mRxAlsaStream->isReadReady());

  // normal read
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // underrun on the local buffer side
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // overrun on the shared memory side
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // passed away data
  for (uint32_t samples = 0; samples < (totalLocalBufferSize / 2u); samples = samples + bufferSize)
  {
    for (uint32_t channel = 0; channel < mRxAlsaStream->getNumChannels(); channel++)
    {
      result = mRxAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timestamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      //ASSERT_EQ(bufferSize, samplesWritten); don't care because it might be zero due to the lack of space
    }
    timestamp++;
  }
  ::sleep(1); // sleep in order to make the queued data expired
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // overrun on the local buffer side
  timestamp = uint32_t(ptpProxy->getLocalTime());
  for (uint32_t samples = 0; samples < (totalLocalBufferSize * 2u); samples = samples + bufferSize)
  {
    for (uint32_t channel = 0; channel < mRxAlsaStream->getNumChannels(); channel++)
    {
      result = mRxAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timestamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      // ASSERT_EQ(bufferSize, samplesWritten); don't care because it would be zero due to the lack of space
    }
    timestamp++;
  }

  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  (void) mRxAlsaStream->setWorkerActive(false);

  result = mRxAlsaStream->disconnect();
  ASSERT_EQ(eIasAvbProcOK, result);

  if (NULL != testClient)
  {
    delete testClient;
  }
}

TEST_F(IasTestAvbAudioShmProvider, copyJob_rx_tstamp_no_alsaprefix)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe);
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode));

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  std::string optName           = std::string(IasRegKeys::cAlsaDevicePrefill) + "avb_" + deviceName + "_c";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mRxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(optName, 0));

  bool dirWriteToShm = true; // Rx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalAudioStream::LocalAudioBufferVec buffers = mRxAlsaStream->getChannelBuffers();
  IasLocalAudioBufferDesc * descQ = mRxAlsaStream->getBufferDescQ();

  uint32_t numFrames = alsaPeriodSize;
  bool dummy = false;
  uint64_t timestamp = 1u;

  ASSERT_FALSE(mRxAlsaStream->isReadReady());

  // empty read
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  mAlsaShm->mBufferState = IasAvbAudioShmProvider::bufferState::ePrefilling;
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  mAlsaShm->mBufferState = (IasAvbAudioShmProvider::bufferState)(3);
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  mAlsaShm->mBufferState = IasAvbAudioShmProvider::bufferState::eRunning;
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  mAlsaShm->mAlsaPrefill = 0u;
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // fill the local buffer by dummy data
  (void) mRxAlsaStream->setWorkerActive(true);

  IasLocalAudioStreamClientInterfaceImpl * testClient = new IasLocalAudioStreamClientInterfaceImpl();
  ASSERT_TRUE(NULL != testClient);

  result = mRxAlsaStream->connect(testClient);
  ASSERT_EQ(eIasAvbProcOK, result);

  uint32_t bufferSize = 64;
  IasLocalAudioBuffer::AudioData buffer[bufferSize];
  uint16_t samplesWritten = 0;

  IasLibPtpDaemon* ptpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
  ASSERT_TRUE(NULL != ptpProxy);
  timestamp = uint32_t(ptpProxy->getLocalTime());

  for (uint32_t samples = 0; samples < totalLocalBufferSize / 2; samples = samples + bufferSize)
  {
    for (uint32_t channel = 0; channel < mRxAlsaStream->getNumChannels(); channel++)
    {
      result = mRxAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timestamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      ASSERT_EQ(bufferSize, samplesWritten);
    }
    timestamp++;
  }

  ASSERT_TRUE(mRxAlsaStream->isReadReady());

  // normal read
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // underrun on the local buffer side
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // overrun on the shared memory side
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // passed away data
  for (uint32_t samples = 0; samples < (totalLocalBufferSize / 2u); samples = samples + bufferSize)
  {
    for (uint32_t channel = 0; channel < mRxAlsaStream->getNumChannels(); channel++)
    {
      result = mRxAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timestamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      //ASSERT_EQ(bufferSize, samplesWritten); don't care because it might be zero due to the lack of space
    }
    timestamp++;
  }
  ::sleep(1); // sleep in order to make the queued data expired
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  // overrun on the local buffer side
  timestamp = uint32_t(ptpProxy->getLocalTime());
  for (uint32_t samples = 0; samples < (totalLocalBufferSize * 2u); samples = samples + bufferSize)
  {
    for (uint32_t channel = 0; channel < mRxAlsaStream->getNumChannels(); channel++)
    {
      result = mRxAlsaStream->writeLocalAudioBuffer(channel, buffer, bufferSize, samplesWritten, timestamp);
      ASSERT_EQ(eIasAvbProcOK, result);
      // ASSERT_EQ(bufferSize, samplesWritten); don't care because it would be zero due to the lack of space
    }
    timestamp++;
  }

  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcOK, result);

  (void) mRxAlsaStream->setWorkerActive(false);

  result = mRxAlsaStream->disconnect();
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAudio::IasAudioRingBuffer * pTempBuffer = mAlsaShm->mShmConnection.mRingBuffer;
  mAlsaShm->mShmConnection.mRingBuffer = nullptr;
  result = mAlsaShm->copyJob(buffers, descQ, numFrames, dummy, timestamp);
  ASSERT_EQ(eIasAvbProcErr, result);
  mAlsaShm->mShmConnection.mRingBuffer = pTempBuffer;

  if (NULL != testClient)
  {
    delete testClient;
  }
}


TEST_F(IasTestAvbAudioShmProvider, ipcthread_rx)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe);
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode));

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mRxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = true; // Rx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAudio::IasAudioCommonResult res;
  IasAudio::IasAudioIpcPluginControlResponse response;

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcGetLatency);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcStart);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcStop);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcDrain);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcPause);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcResume);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcInvalid);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  IasAudio::IasAudioCurrentSetParameters setParams = {0u, 0u, 0u, 0u, static_cast<IasAudio::IasAudioCommonDataFormat>(0)};
  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginParamData>(IasAudio::IasAudioIpcPluginParamData(IasAudio::eIasAudioIpcParameters,setParams));
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginParamData>(IasAudio::IasAudioIpcPluginParamData(IasAudio::eIasAudioIpcInvalid,setParams));
  ASSERT_TRUE(IasAudio::IasAudioCommonResult::eIasResultOk == res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);
}


TEST_F(IasTestAvbAudioShmProvider, ipcthread_tx)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mTxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  uint64_t mode = static_cast<uint64_t>(IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe);
  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(IasRegKeys::cAudioTstampBuffer, mode));

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000u;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format      = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel         = false;
  std::string deviceName        = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  result = mTxAlsaStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, alsaPeriodSize, numAlsaBuffers,
                             alsaSampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
  ASSERT_EQ(eIasAvbProcOK, result);

  bool dirWriteToShm = false; // Tx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAudio::IasAudioCommonResult res;
  IasAudio::IasAudioIpcPluginControlResponse response;

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcGetLatency);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcStart);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcStop);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcDrain);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcPause);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcResume);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcInvalid);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  mAlsaShm->mAlsaPrefill = 0u;
  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcStart);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);

  res = mAlsaShm->mInIpc->push<IasAudio::IasAudioIpcPluginControl>(IasAudio::eIasAudioIpcStop);
  ASSERT_EQ(IasAudio::IasAudioCommonResult::eIasResultOk, res);
  mAlsaShm->mOutIpc->pop<IasAudio::IasAudioIpcPluginControlResponse>(&response);
}

TEST_F(IasTestAvbAudioShmProvider, init)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t alsaSampleFrequency  = 48000u;
  std::string optName              = std::string(IasRegKeys::cAlsaDevicePrefill) + mAlsaShm->mDeviceName + "_c";

  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(optName, alsaPeriodSize * numAlsaBuffers));

  bool dirWriteToShm = true; // Rx
  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbAudioShmProvider, init_branch)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t alsaSampleFrequency  = 48000u;
  std::string optName              = std::string(IasRegKeys::cAlsaDevicePrefill) + mAlsaShm->mDeviceName + "_c";

  bool dirWriteToShm = true; // Rx

  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(optName, numAlsaBuffers));

  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbAudioShmProvider, resetShmBuffer)
{
  ASSERT_TRUE(NULL != mAlsaShm);
  ASSERT_TRUE(NULL != mRxAlsaStream);
  ASSERT_TRUE(NULL != mEnvironment);

  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  uint16_t numChannels          = 2u;
  uint32_t alsaPeriodSize       = 192u;
  uint32_t numAlsaBuffers       = 3u;
  uint32_t alsaSampleFrequency  = 48000u;
  std::string optName              = std::string(IasRegKeys::cAlsaDevicePrefill) + mAlsaShm->mDeviceName + "_p";

  bool dirWriteToShm = true; // Tx

  ASSERT_EQ(eIasAvbProcOK, mEnvironment->setConfigValue(optName, numAlsaBuffers));

  result = mAlsaShm->init(numChannels, alsaPeriodSize, numAlsaBuffers, alsaSampleFrequency, dirWriteToShm);
  ASSERT_EQ(eIasAvbProcOK, result);

  mAlsaShm->mShmConnection.mRingBuffer->mRingBufReal->mWriteOffset = 1u;
  result = mAlsaShm->resetShmBuffer(IasAvbAudioShmProvider::bufferState::eRunning);
  ASSERT_EQ(eIasAvbProcOK, result);
  result = eIasAvbProcErr;

  mAlsaShm->mParams->numPeriods = 0;//error in beginAccess
  result = mAlsaShm->resetShmBuffer(IasAvbAudioShmProvider::bufferState::eRunning);
  ASSERT_EQ(eIasAvbProcOK, result);

  auto tempRingBufferPtr = mAlsaShm->mShmConnection.mRingBuffer;
  mAlsaShm->mShmConnection.mRingBuffer = nullptr;
  result = mAlsaShm->resetShmBuffer(IasAvbAudioShmProvider::bufferState::eRunning);
  ASSERT_EQ(eIasAvbProcErr, result);
  mAlsaShm->mShmConnection.mRingBuffer = tempRingBufferPtr;

  mAlsaShm->mAlsaPrefill = 0u;//disable prefill
  result = mAlsaShm->resetShmBuffer(IasAvbAudioShmProvider::bufferState::eRunning);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAlsaShm->resetShmBuffer(static_cast<IasAvbAudioShmProvider::bufferState>(3));
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
  result = eIasAvbProcErr;

  result = mAlsaShm->resetShmBuffer(static_cast<IasAvbAudioShmProvider::bufferState>(-1));
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
  result = eIasAvbProcErr;

  mAlsaShm->mDirWriteToShm = false;
  result = mAlsaShm->resetShmBuffer(IasAvbAudioShmProvider::bufferState::eRunning);//Rx
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}


} // namespace
