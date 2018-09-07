/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestAvbAudioStream.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbAudioStream.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamId.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"

#include "avb_streamhandler/IasAvbPacketPool.hpp" // For heap fail size calculations
extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

#include <arpa/inet.h>
#include <sstream>

namespace IasMediaTransportAvb {

// Helper class. Inherits from IasLocalAudioStream to have something to connect to
class LocalAudioDummyStream : public IasLocalAudioStream
{
  public:
    LocalAudioDummyStream(DltContext &dltContext, IasAvbStreamDirection direction, uint16_t streamId) : IasLocalAudioStream(dltContext, direction, eIasTestToneStream, streamId) {}

    virtual ~LocalAudioDummyStream() { cleanup(); }

    IasAvbProcessingResult init(uint16_t numChannels,
                                uint32_t totalBufferSize,
                                uint32_t sampleFrequency,
                                uint8_t  channelLayout,
                                bool   hasSideChannel);

    // mandatory for derived classes
    IasAvbProcessingResult resetBuffers();
};

IasAvbProcessingResult LocalAudioDummyStream::init(uint16_t numChannels,
                            uint32_t totalBufferSize,
                            uint32_t sampleFrequency,
                            uint8_t  channelLayout,
                            bool   hasSideChannel)
{
  return IasLocalAudioStream::init(channelLayout, numChannels, hasSideChannel, totalBufferSize, sampleFrequency);
}

IasAvbProcessingResult LocalAudioDummyStream::resetBuffers()
{
  return eIasAvbProcOK;
}

// Helper class. Inherits IasAvbAudioStream to make tests for protected content
class MyAvbAudioStream : public IasAvbAudioStream
{
public:
  MyAvbAudioStream()
    : IasAvbAudioStream()
  {
  }

  virtual ~MyAvbAudioStream() {}

  bool writeToAvbPacket(IasAvbPacket* packet) { return this->IasAvbAudioStream::writeToAvbPacket(packet, 0u); }
  void readFromAvbPacket(const void* const packet, const size_t length) { this->IasAvbAudioStream::readFromAvbPacket(packet, length); }
  void activationChanged() { this->IasAvbAudioStream::activationChanged(); }
  bool signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples) { return this->IasAvbAudioStream::signalDiscontinuity( event, numSamples); }
  void updateRelativeFillLevel(float relFillLevel) { this->IasAvbAudioStream::updateRelativeFillLevel(relFillLevel); }
};

class IasTestAvbAudioStream : public ::testing::Test
{
public:
  IasTestAvbAudioStream():
    mAudioStream(NULL),
    streamHandler(DLT_LOG_INFO),
    mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbAudioStream()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;
    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
                  "TEST",
                  "IasTestAvbAudioStream",
                  DLT_LOG_INFO,
                  DLT_TRACE_STATUS_OFF);
    dlt_enable_local_print();

    mAudioStream = new (std::nothrow) MyAvbAudioStream();
  }

  virtual void TearDown()
  {
    if (NULL != mAudioStream)
    {
      delete mAudioStream;
      mAudioStream = NULL;
    }

    if (mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    streamHandler.cleanup();

    heapSpaceLeft = heapSpaceInitSize;
    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }


protected:

  IasAvbProcessingResult initStreamHandler()
  {
    // IT HAS TO BE SET TO ZERO - getopt_long - DefaultConfig_passArguments - needs to be reinitialized before use
    optind = 0;

    IasSpringVilleInfo::fetchData();

    const char *args[] = {
      "setup",
      "-t", "Fedora",
      "-p", "UnitTests",
      "-n", IasSpringVilleInfo::getInterfaceName()
    };

    int argc = sizeof(args) / sizeof(args[0]);

    if (NULL != mEnvironment)
    {
      // init of streamhandler will create it's own environment
      return eIasAvbProcErr;
    }

    return streamHandler.init(theConfigPlugin, false, argc, (char**)args);
  }

  bool createEnvironment()
  {
    if(NULL != mEnvironment)
    {
      return false;
    }

    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    if (mEnvironment)
    {
      mEnvironment->registerDltContexts();
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
    }
    return false;
  }

  IasAvbResult setConfigValue(const std::string& key, uint64_t value)
  {
    IasAvbResult result;
    IAS_ASSERT(IasAvbStreamHandlerEnvironment::mInstance);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
    result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
    return result;
  }

  IasAvbResult setConfigValue(const std::string& key, const std::string& value)
  {
    IasAvbResult result;
    IAS_ASSERT(IasAvbStreamHandlerEnvironment::mInstance);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
    result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
    return result;
  }

  DltContext mDltCtx;
  MyAvbAudioStream *mAudioStream;
  IasAvbStreamHandler streamHandler;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

TEST_F(IasTestAvbAudioStream, IsConnected)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_TRUE(createEnvironment());
  ASSERT_TRUE(!mAudioStream->isConnected());
}

TEST_F(IasTestAvbAudioStream, InitTransmit)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbStreamId avbStreamIdObj;
  IasAvbPtpClockDomain * avbClockDomainObj = NULL;
  IasAvbMacAddress avbMacAddr = {};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels = 0u;
  uint32_t sampleFreq = 0u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  uint32_t poolSize = 2u;

  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->initTransmit(srClass,
                                                                maxNumberChannels,
                                                                sampleFreq,
                                                                format,
                                                                avbStreamIdObj,
                                                                poolSize,
                                                                avbClockDomainObj,
                                                                avbMacAddr,
                                                                true));

  maxNumberChannels = 2u;
  // maxNumberChannels != 0 || sampleFreq == 0
  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->initTransmit(srClass,
                                                                maxNumberChannels,
                                                                sampleFreq,
                                                                format,
                                                                avbStreamIdObj,
                                                                poolSize,
                                                                avbClockDomainObj,
                                                                avbMacAddr,
                                                                true));

  sampleFreq = 48000u;
  // maxNumberChannels != 0 || sampleFreq != 0 || avbClockDomain == NULL
  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->initTransmit(srClass,
                                                                maxNumberChannels,
                                                                sampleFreq,
                                                                format,
                                                                avbStreamIdObj,
                                                                poolSize,
                                                                avbClockDomainObj,
                                                                avbMacAddr,
                                                                true));

  avbClockDomainObj = new (nothrow) IasAvbPtpClockDomain();
  sampleFreq = 24000u;
  // sampleFreq != 48000u
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAudioStream->initTransmit(srClass,
                                                                     maxNumberChannels,
                                                                     sampleFreq,
                                                                     format,
                                                                     avbStreamIdObj,
                                                                     poolSize,
                                                                     avbClockDomainObj,
                                                                     avbMacAddr,
                                                                     true));

  sampleFreq = 48000u;
  format = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  // sampleFreq == 48000u || format != IasAvbAudioFormat::eIasAvbAudioFormatSaf16
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAudioStream->initTransmit(srClass,
                                                                     maxNumberChannels,
                                                                     sampleFreq,
                                                                     format,
                                                                     avbStreamIdObj,
                                                                     poolSize,
                                                                     avbClockDomainObj,
                                                                     avbMacAddr,
                                                                     true));

  sampleFreq = 12000u;
  format = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  // !(48000u == sampleFreq || 24000 == sampleFreq)            (!(F || F ||T))
  // || (IasAvbAudioFormat::eIasAvbAudioFormatSaf16 != format)
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAudioStream->initTransmit(srClass,
                                                                     maxNumberChannels,
                                                                     sampleFreq,
                                                                     format,
                                                                     avbStreamIdObj,
                                                                     poolSize,
                                                                     avbClockDomainObj,
                                                                     avbMacAddr,
                                                                     true));

  sampleFreq = 48000u;
  format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  // streamId == bend (T)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cBendCtrlStream, uint64_t(avbStreamIdObj)));
  // create and open the debug file
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cDebugBufFName, "initTransmit.log"));
  // use saturation
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioSaturate, 1u));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamIdObj,
                                                      poolSize,
                                                      avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));

  ASSERT_EQ(eIasAvbProcInitializationFailed, mAudioStream->initTransmit(srClass,
                                                                        maxNumberChannels,
                                                                        sampleFreq,
                                                                        format,
                                                                        avbStreamIdObj,
                                                                        poolSize,
                                                                        avbClockDomainObj,
                                                                        avbMacAddr,
                                                                        true));
  delete mAudioStream;
  mAudioStream = new (std::nothrow) MyAvbAudioStream();

  IasAvbStreamId streamId(uint64_t(32u));
  int maxBend = 300; // do not exceed 999
  // streamId == bend (F)
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cBendCtrlStream, uint64_t(streamId) + 1u));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioMaxBend, maxBend));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      streamId,
                                                      poolSize,
                                                      avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));
  ASSERT_TRUE(0 == mAudioStream->mRatioBendLimit);

  delete avbClockDomainObj;
  avbClockDomainObj = NULL;
}

TEST_F(IasTestAvbAudioStream, InitTransmit_OutOfMemory)
{
  ASSERT_TRUE(mAudioStream != NULL);


  IasAvbStreamId avbStreamIdObj;
  IasAvbClockDomain *avbClockDomainObj = new IasAvbPtpClockDomain();
  ASSERT_TRUE(NULL != avbClockDomainObj);
  IasAvbMacAddress avbMacAddr = {};

  uint16_t maxNumberChannels = 2;
  uint32_t sampleFreq = 48000;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t poolSize = 1;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  heapSpaceLeft = 0;

  // 4 Memory tests here as initTransmit calls initCommon. initCommon has 3 allocations before returning.
  // Memory Fails from IasAvbStream::initCommon() - mTSpec = new (nothrow) IasAvbTSpec( tSpec );
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  heapSpaceLeft = sizeof(IasAvbTSpec);
  // Memory Fails from IasAvbStream::initCommon() - mAvbStreamId = new (nothrow) IasAvbStreamId( streamId );
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  heapSpaceLeft = sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId);
  // Memory Fails from IasAvbStream::initCommon() -     mPacketPool = new (nothrow) IasAvbPacketPool();
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  heapSpaceLeft = sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId) + sizeof(IasAvbPacketPool);
  // Memory Fails from initTransmit - mTempBuffer = new (nothrow) IasAvbPacket[poolSize];
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  heapSpaceLeft = sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId) + sizeof(IasAvbPacketPool) + sizeof(IasAvbPacket) * poolSize + sizeof(size_t); // for new[]
  // From here we hit a path blocker with Page failing to be created.
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  heapSpaceLeft = sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId) + sizeof(IasAvbPacketPool) + sizeof(IasAvbPacket) * poolSize + sizeof(size_t) +
      sizeof(igb_dma_alloc);
  // From here we hit a path blocker with AudioData failing to be created.
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cBendCtrlStream, uint64_t(avbStreamIdObj)));
  uint32_t pps = IasAvbTSpec::getPacketsPerSecondByClass(srClass); // packets per second
  heapSpaceLeft = sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId) + sizeof(IasAvbPacketPool) + sizeof(IasAvbPacket)
                    * poolSize + sizeof(size_t) + sizeof(igb_dma_alloc) + sizeof(IasLocalAudioBuffer::AudioData)
                    * ((sampleFreq + pps - 1u) / pps) + sizeof(int) * (IasAvbAudioStream::cFillLevelFifoSize - 1u);
  // not enough mem to create fillLevelFifo
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, avbStreamIdObj, poolSize, avbClockDomainObj, avbMacAddr, true));

  // Destroy object created dynamically
  delete avbClockDomainObj;
  avbClockDomainObj = NULL;
}

TEST_F(IasTestAvbAudioStream, InitReceive)
{
  ASSERT_TRUE(mAudioStream != NULL);

  uint16_t maxNumberChannels = 0u;
  uint32_t sampleFreq = 0u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  IasAvbStreamId streamId;
  IasAvbMacAddress dmac = {};
  uint16_t vid = 0u;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  ASSERT_TRUE(createEnvironment());

  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->initReceive(srClass,
                                                                    maxNumberChannels,
                                                                    sampleFreq,
                                                                    format,
                                                                    streamId,
                                                                    dmac,
                                                                    vid,
                                                                    true));

  maxNumberChannels = 2u;

  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->initReceive(srClass,
                                                               maxNumberChannels,
                                                               sampleFreq,
                                                               format,
                                                               streamId,
                                                               dmac,
                                                               vid,
                                                               true));

  sampleFreq = 24000u;

  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAudioStream->initReceive(srClass,
                                                                    maxNumberChannels,
                                                                    sampleFreq,
                                                                    format,
                                                                    streamId,
                                                                    dmac,
                                                                    vid,
                                                                    true));

  sampleFreq = 48000u;

  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAudioStream->initReceive(srClass,
                                                                    maxNumberChannels,
                                                                    sampleFreq,
                                                                    format,
                                                                    streamId,
                                                                    dmac,
                                                                    vid,
                                                                    true));

  format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  vid = 2u;

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(srClass,
                                                     maxNumberChannels,
                                                     sampleFreq,
                                                     format,
                                                     streamId,
                                                     dmac,
                                                     vid,
                                                     true));

  uint32_t ptOffset = (2000000u - 125000u);
  ASSERT_EQ(ptOffset, mAudioStream->getMaxTransmitTime()); // see mPresentationTimeOffsetTable[]

  // double init testing
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAudioStream->initReceive(srClass,
                                                                       maxNumberChannels,
                                                                       sampleFreq,
                                                                       format,
                                                                       streamId,
                                                                       dmac,
                                                                       vid,
                                                                       true));
}

TEST_F(IasTestAvbAudioStream, initTransmit_config_options)
{
  ASSERT_TRUE(mAudioStream != NULL);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbStreamId avbStreamIdObj;
  IasAvbPtpClockDomain avbClockDomainObj;
  IasAvbMacAddress avbMacAddr = {};

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioMaxBend, 1));
  uint64_t ppm = 0u;
  IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioMaxBend, ppm);
  ASSERT_EQ(1, ppm);

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioFloatGain, 1));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(IasAvbSrClass::eIasAvbSrClassHigh, 2, 48000u,
               IasAvbAudioFormat::eIasAvbAudioFormatSaf16, avbStreamIdObj, 2, &avbClockDomainObj, avbMacAddr, true));
}

#if 1 // TODO: replace JackStream!
TEST_F(IasTestAvbAudioStream, ConnectTo)
{
  ASSERT_TRUE(mAudioStream != NULL);
  IasAvbStreamDirection directionTransmit = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  IasAvbStreamDirection directionReceive = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;

  LocalAudioDummyStream * localStream = new LocalAudioDummyStream(mDltCtx, directionTransmit, 1u);

  ASSERT_EQ(eIasAvbProcNotInitialized, mAudioStream->connectTo(localStream));

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbStreamId avbStreamId(uint64_t(1u));
  uint32_t poolSize = 2u;
  IasAvbPtpClockDomain avbClockDomain;
  IasAvbMacAddress avbMacAddr = {};
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cBendCtrlStream, uint64_t(avbStreamId)));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamId,
                                                      poolSize,
                                                      &avbClockDomain,
                                                      avbMacAddr,
                                                      true));

  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->connectTo(localStream));

  uint16_t numChannels = 2u;
  uint32_t totalBufferSize = 256u;
  uint32_t jackSampleFrequency = 48000u;
  uint8_t  channelLayout = 2u;
  bool   hasSideChannel = true;

  ASSERT_EQ(eIasAvbProcOK, localStream->init(numChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  localStream->mNumChannels = 0u;
  // hasSideChannel == true (T)
  // && numChannels > 0u    (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->connectTo(localStream));
  localStream->mNumChannels = numChannels;
  // hasSideChannel == true (T)
  // && numChannels > 0u    (T)
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));

  numChannels = maxNumberChannels + 1u;
  hasSideChannel = false;
  uint16_t localStreamId = 0u;
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
  localStream = new LocalAudioDummyStream(mDltCtx, directionTransmit, localStreamId);
  ASSERT_EQ(eIasAvbProcOK, localStream->init(numChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  // localStream->numChannels > mMaxNumChannels
  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->connectTo(localStream));

  numChannels = maxNumberChannels;
  jackSampleFrequency = sampleFreq - 1u;
  localStreamId = 0u;
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
  localStream = new LocalAudioDummyStream(mDltCtx, directionTransmit, localStreamId);
  ASSERT_EQ(eIasAvbProcOK, localStream->init(numChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  // mSampleFrequency != localStream->getSampleFrequency()
  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->connectTo(localStream));

  jackSampleFrequency = sampleFreq;
  localStreamId = 0u;
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
  localStream = new LocalAudioDummyStream(mDltCtx, directionReceive, localStreamId);
  ASSERT_EQ(eIasAvbProcOK, localStream->init(numChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  // getDirection() != localStream->getDirection()
  ASSERT_EQ(eIasAvbProcInvalidParam, mAudioStream->connectTo(localStream));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
  localStream = NULL;
}
#endif

TEST_F(IasTestAvbAudioStream, GetPacketSize)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_EQ(32, mAudioStream->getPacketSize(IasAvbAudioFormat::eIasAvbAudioFormatIec61883, 0));
}

TEST_F(IasTestAvbAudioStream, GetSampleSize)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_EQ(4, mAudioStream->getSampleSize(IasAvbAudioFormat::eIasAvbAudioFormatIec61883));
}

#if 1 // TODO: replace JackStream!
TEST_F(IasTestAvbAudioStream, WriteToAvbPacket)
{
  ASSERT_TRUE(mAudioStream != NULL);
  IasAvbPacket packet;
  // !isInitialized() || !isActive() || !isTransmitStream() (T || T || F)
  ASSERT_FALSE(mAudioStream->writeToAvbPacket(&packet));

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  uint16_t maxNumberChannels      = 6u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbStreamId avbStreamId(uint64_t(1u));
  IasAvbRxStreamClockDomain avbClockDomainObj;
  IasAvbMacAddress avbMacAddr   = {0};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint16_t vid                    = 2u;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(srClass,
                                                     maxNumberChannels,
                                                     sampleFrequency,
                                                     audioFormat,
                                                     avbStreamId,
                                                     avbMacAddr,
                                                     vid,
                                                     true));
  // !isInitialized() || !isActive() || !isTransmitStream() (F || T || T)
  ASSERT_FALSE(mAudioStream->writeToAvbPacket(&packet));

  mAudioStream->activate();
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || T)
  ASSERT_FALSE(mAudioStream->writeToAvbPacket(&packet));

  delete mAudioStream;
  mAudioStream = new (std::nothrow) MyAvbAudioStream();
  ASSERT_TRUE(mAudioStream != NULL);
  //
  // set up a transmit stream
  //
  LocalAudioDummyStream * localStream = new LocalAudioDummyStream(mDltCtx, IasAvbStreamDirection::eIasAvbTransmitToNetwork, 1u);

  uint32_t poolSize = 2u;
  IasAvbPtpClockDomain avbClockDomain;

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFrequency,
                                                      audioFormat,
                                                      avbStreamId,
                                                      poolSize,
                                                      &avbClockDomain,
                                                      avbMacAddr,
                                                      true));

  uint32_t totalBufferSize = 256u;
  uint32_t jackSampleFrequency = 48000u;
  uint8_t  channelLayout = 2u;
  bool   hasSideChannel = true;

  ASSERT_EQ(eIasAvbProcOK, localStream->init(maxNumberChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));
  mAudioStream->activate();
  ////

  uint8_t _vaddr[1024];
  memset(_vaddr, 0, sizeof _vaddr);
  packet.vaddr = _vaddr;
  // avtpBase8[22] & 0x10                          (F)
  // isConnected()                                 (T)
  // mLocalStream->hasSideChannel()                (T)
  // mDummySamplesSent > 0u                        (F)
  // (eIasAvbCompSaf == mCompatibilityModeAudio)  (T)
  // || (eIasAvbCompD6 == mCompatibilityModeAudio) (|| F)
  ASSERT_TRUE(mAudioStream->writeToAvbPacket(&packet));

  memset(_vaddr, 0, sizeof _vaddr);
  uint8_t* const avtpBase8 = static_cast<uint8_t*>(packet.vaddr) + 18;
  avtpBase8[22]                         = 0x10u;
  mAudioStream->mSeqNum                 = 8u;
  mAudioStream->mDummySamplesSent       = 1u;
  mAudioStream->mWaitForData            = true;
  mAudioStream->mUseSaturation          = false;
  mAudioStream->mCompatibilityModeAudio = eIasAvbCompD6;
  mAudioStream->mDebugIn                = false;
  // avtpBase8[22] & 0x10                          (T)
  // !(mSeqNum % 8)                                (T)
  // (eIasAvbCompSaf == mCompatibilityModeAudio)  (F)
  // || (eIasAvbCompD6 == mCompatibilityModeAudio) (|| T)
  ASSERT_TRUE(mAudioStream->writeToAvbPacket(&packet));

  memset(_vaddr, 0, sizeof _vaddr);
  avtpBase8[22]                         = 0x10u;
  mAudioStream->mCompatibilityModeAudio = eIasAvbCompLatest;
  mAudioStream->mRefPlaneSampleCount     = 0u;
  mAudioStream->mRefPlaneSampleTime      = 1u;
  mAudioStream->mSeqNum                 = 7u;
  mAudioStream->mDummySamplesSent       = 1u;
  mAudioStream->mDumpCount              = 11u;
  mAudioStream->mSampleIntervalNs       = -0.08f;
  // avtpBase8[22] & 0x10                          (T)
  // !(mSeqNum % 8)                                (F)
  // (eIasAvbCompSaf == mCompatibilityModeAudio)  (F)
  // || (eIasAvbCompD6 == mCompatibilityModeAudio) (|| F)
  ASSERT_TRUE(mAudioStream->writeToAvbPacket(&packet));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
  localStream = NULL;
}
#endif

TEST_F(IasTestAvbAudioStream, WriteToAvbPacket_dump)
{
  ASSERT_TRUE(mAudioStream != NULL);
  IasAvbPacket packet;

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  uint16_t maxNumberChannels      = 6u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbStreamId avbStreamId(uint64_t(1u));
  IasAvbRxStreamClockDomain avbClockDomainObj;
  IasAvbMacAddress avbMacAddr   = {0};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  //
  // set up a transmit stream
  //
  std::unique_ptr<IasAlsaVirtualDeviceStream> localStream(new IasAlsaVirtualDeviceStream(mDltCtx, IasAvbStreamDirection::eIasAvbTransmitToNetwork, 1u));

  uint32_t poolSize = 2u;
  IasAvbPtpClockDomain avbClockDomain;

  ASSERT_EQ(eIasAvbProcOK, setConfigValue(IasRegKeys::cAudioTstampBuffer, 1));// fail-safe
  localStream->mDescMode = IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe;

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFrequency,
                                                      audioFormat,
                                                      avbStreamId,
                                                      poolSize,
                                                      &avbClockDomain,
                                                      avbMacAddr,
                                                      true));

  uint16_t numChannels          = 2;
  uint32_t alsaPeriodSize       = 256;
  uint32_t numAlsaBuffers       = 3;
  uint32_t totalLocalBufferSize = (alsaPeriodSize * numAlsaBuffers);
  uint32_t alsaSampleFrequency  = 48000;
  uint32_t optimalFillLevel     = (totalLocalBufferSize / 2u);
  IasAvbAudioFormat format    = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint8_t  channelLayout        = 0;
  bool   hasSideChannel       = false;
  std::string deviceName           = "AlsaTest";
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  ASSERT_EQ(eIasAvbProcOK, localStream->init(numChannels,
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

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream.get()));
  mAudioStream->activate();
  ////

  uint8_t _vaddr[1024];
  memset(_vaddr, 0, sizeof _vaddr);
  packet.vaddr = _vaddr;
  mAudioStream->mDummySamplesSent = 1001u;
  mAudioStream->mWaitForData = true;

  ASSERT_TRUE(mAudioStream->writeToAvbPacket(&packet));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
}

TEST_F(IasTestAvbAudioStream, ReadFromAvbPacket_noInit)
{
  ASSERT_TRUE(mAudioStream != NULL);
  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);
  // isInitialized() && isReceiveStream() (F && N/A)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);
}

TEST_F(IasTestAvbAudioStream, ReadFromAvbPacket_notReceive)
{
  ASSERT_TRUE(mAudioStream != NULL);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbStreamId avbStreamIdObj;
  IasAvbPtpClockDomain avbClockDomainObj;
  IasAvbMacAddress avbMacAddr = {0};
  uint16_t maxNumberChannels    = 2u;
  uint32_t sampleFreq           = 48000u;
  IasAvbAudioFormat format    = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t poolSize             = 2u;
  IasAvbSrClass srClass       = IasAvbSrClass::eIasAvbSrClassHigh;
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamIdObj,
                                                      poolSize,
                                                      &avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));

  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);
  // isInitialized() && isReceiveStream() (T && F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);
}

TEST_F(IasTestAvbAudioStream, ReadFromAvbPacket_NULL)
{
  ASSERT_TRUE(mAudioStream != NULL);

  ASSERT_TRUE(createEnvironment());

  uint16_t maxNumberChannels      = 6u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbMacAddress avbMacAddr   = {0};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint16_t vid                    = 2u;
  IasAvbStreamId avbStreamIdObj;
  IasAvbRxStreamClockDomain avbRxClockDomainObj;
  ASSERT_EQ(eIasAvbProcOK, setConfigValue(IasRegKeys::cCompatibilityAudio, "latest"));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh,
                                                     maxNumberChannels,
                                                     sampleFrequency,
                                                     audioFormat,
                                                     avbStreamIdObj,
                                                     avbMacAddr,
                                                     vid,
                                                     true));

  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  IasAvbPtpClockDomain ptpClockDomain;
  mAudioStream->mAvbClockDomain         = &ptpClockDomain;
  // NULL == packet                                                            (T)
  // newState != oldState                                                      (T)
  // IasAvbStreamState::eIasAvbStreamValid == newState                         (F)
  // IasAvbStreamState::eIasAvbStreamValid == oldState                         (T)
  // IasAvbStreamState::eIasAvbStreamNoData == newState                        (T)
  // (NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx) (T && F)
  mAudioStream->readFromAvbPacket(NULL, 0);

  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  mAudioStream->mAvbClockDomain         = NULL;
  // NULL == packet                                                            (T)
  // newState != oldState                                                      (T)
  // IasAvbStreamState::eIasAvbStreamValid == newState                         (F)
  // IasAvbStreamState::eIasAvbStreamValid == oldState                         (T)
  // IasAvbStreamState::eIasAvbStreamNoData == newState                        (T)
  // isConnected()                                                             (F)
  // (NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx) (F && N/A)
  mAudioStream->readFromAvbPacket(NULL, 0);
}

#if 1 // TODO: replace JackStream!
TEST_F(IasTestAvbAudioStream, ReadFromAvbPacket_connected_no_side_channel)
{
  ASSERT_TRUE(mAudioStream != NULL);

  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);

  ASSERT_TRUE(createEnvironment());

  uint16_t maxNumberChannels      = 6u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbMacAddress avbMacAddr   = {0};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint16_t vid                    = 2u;
  IasAvbStreamId avbStreamIdObj;
  IasAvbRxStreamClockDomain avbRxClockDomainObj;
  ASSERT_EQ(eIasAvbProcOK, setConfigValue(IasRegKeys::cCompatibilityAudio, "latest"));

  ASSERT_EQ(eIasAvbProcOK, setConfigValue(IasRegKeys::cAudioTstampBuffer,
                           IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeFailSafe));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh,
                                                     maxNumberChannels,
                                                     sampleFrequency,
                                                     audioFormat,
                                                     avbStreamIdObj,
                                                     avbMacAddr,
                                                     vid,
                                                     true));
  //
  // local stream setup
  uint16_t localStreamId        = 1u;
  LocalAudioDummyStream * localStream = new LocalAudioDummyStream(mDltCtx,
                                                  IasAvbStreamDirection::eIasAvbReceiveFromNetwork,
                                                  localStreamId);
  uint32_t totalBufferSize      = 256u;
  uint8_t  channelLayout        = 0u;
  bool   hasSideChannel       = false;

  ASSERT_EQ(eIasAvbProcOK, localStream->init(maxNumberChannels,
                                             totalBufferSize,
                                             sampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));
  mAudioStream->activate();
  //

  uint8_t*  const avtpBase8  = packet;
  uint16_t* const avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateAlways;
  avtpBase8[0]                          = 0x02u;
  avtpBase8[16]                         = mAudioStream->mAudioFormatCode;
  avtpBase16[10]                        = htons(uint8_t(sizeof packet - IasAvbAudioStream::cAvtpHeaderSize));// payload
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamInactive; // oldState != newState
  mAudioStream->mValidationCount        = 0u;
  mAudioStream->mCompatibilityModeAudio = IasAvbCompatibility::eIasAvbCompLatest;
  avtpBase8[1]                          = 0x08u; // increment MEDIA_RESET
  avtpBase8[1]                         |= 0x01u; // increment TIMESTAMP_VALID
  avtpBase8[3]                          = 0x01u; // increment TIMESTAMP_UNCERTAIN
  mAudioStream->mAvbClockDomain         = &avbRxClockDomainObj;
  // sampleFrequencyCode == mSampleFrequencyCode
  avtpBase8[17]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency) << 4;
  avtpBase8[17]                        += maxNumberChannels; //numChannels
  mAudioStream->mNumSkippedPackets      = mAudioStream->mNumPacketsToSkip;
  // sideChannel (F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));

  delete localStream;
  localStream = NULL;
}
#endif

#if 1 // TODO: replace JackStream!
TEST_F(IasTestAvbAudioStream, ReadFromAvbPacket)
{
  ASSERT_TRUE(mAudioStream != NULL);

  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);

  ASSERT_TRUE(createEnvironment());

  uint16_t maxNumberChannels      = 6u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbMacAddress avbMacAddr   = {0};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint16_t vid                    = 2u;
  IasAvbStreamId avbStreamIdObj;
  IasAvbRxStreamClockDomain avbRxClockDomainObj;
  ASSERT_EQ(eIasAvbProcOK, setConfigValue(IasRegKeys::cCompatibilityAudio, "latest"));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh,
                                                     maxNumberChannels,
                                                     sampleFrequency,
                                                     audioFormat,
                                                     avbStreamIdObj,
                                                     avbMacAddr,
                                                     vid,
                                                     true));
  //
  // local stream setup
  uint16_t localStreamId        = 1u;
  LocalAudioDummyStream * localStream = new LocalAudioDummyStream(mDltCtx,
                                                  IasAvbStreamDirection::eIasAvbReceiveFromNetwork,
                                                  localStreamId);
  uint32_t totalBufferSize      = 256u;
  uint8_t  channelLayout        = 0u;
  bool   hasSideChannel       = true;

  ASSERT_EQ(eIasAvbProcOK, localStream->init(maxNumberChannels,
                                             totalBufferSize,
                                             sampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));
  mAudioStream->activate();
  //

  uint8_t*  const avtpBase8  = packet;
  uint16_t* const avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateAlways;
  avtpBase8[0]                          = 0x02u;
  avtpBase8[16]                         = mAudioStream->mAudioFormatCode;
  avtpBase16[10]                        = htons(uint8_t(sizeof packet - IasAvbAudioStream::cAvtpHeaderSize));// payload
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamInactive; // oldState != newState
  mAudioStream->mValidationCount        = 0u;
  mAudioStream->mCompatibilityModeAudio = IasAvbCompatibility::eIasAvbCompLatest;
  avtpBase8[1]                          = 0x08u; // increment MEDIA_RESET
  avtpBase8[1]                         |= 0x01u; // increment TIMESTAMP_VALID
  avtpBase8[3]                          = 0x01u; // increment TIMESTAMP_UNCERTAIN
  mAudioStream->mAvbClockDomain         = &avbRxClockDomainObj;
  // sampleFrequencyCode == mSampleFrequencyCode
  avtpBase8[17]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency) << 4;
  avtpBase8[17]                        += maxNumberChannels; //numChannels
  mAudioStream->mNumSkippedPackets      = mAudioStream->mNumPacketsToSkip;
  // longest positive path possible
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[1]                          = 1u; // don't increment MEDIA_RESET
  avtpBase8[1]                         |= 0u; // don't increment TIMESTAMP_VALID
  avtpBase8[3]                          = 0u; // don't increment TIMESTAMP_UNCERTAIN
  mAudioStream->mCurrentAvbLockState    = IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateUnlocked;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  mAudioStream->mNumSkippedPackets      = mAudioStream->mNumPacketsToSkip;
  mAudioStream->mCompatibilityModeAudio = IasAvbCompatibility::eIasAvbCompD6;
  avtpBase16[11]                        = htons(maxNumberChannels); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // eIasAvbCompLatest == mCompatibilityModeAudio                     (F)
  // newState != oldState                                             (F)
  // numChannels > numLocalChannels                                   (T)
  // (eIasAvbCompSaf == mCompatibilityModeAudio)                     (F)
  //   || (eIasAvbCompD6 == mCompatibilityModeAudio)                  (|| T)
  // (avtpBase8[1] & 0x01) && mNumSkippedPackets >= mNumPacketsToSkip (T && T)
  // mCurrentAvbLockState != lockState                                (T)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[1]                          = 1u; // don't increment MEDIA_RESET
  avtpBase8[1]                         |= 0u; // don't increment TIMESTAMP_VALID
  avtpBase8[3]                          = 0u; // don't increment TIMESTAMP_UNCERTAIN
  mAudioStream->mCurrentAvbLockState    = avbRxClockDomainObj.getLockState();
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  mAudioStream->mNumSkippedPackets      = mAudioStream->mNumPacketsToSkip - 1u;
  mAudioStream->mCompatibilityModeAudio = IasAvbCompatibility::eIasAvbCompD6;
  avtpBase16[11]                        = htons(maxNumberChannels); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // eIasAvbCompLatest == mCompatibilityModeAudio                     (F)
  // newState != oldState                                             (F)
  // numChannels > numLocalChannels                                   (T)
  // (eIasAvbCompSaf == mCompatibilityModeAudio)                     (F)
  //   || (eIasAvbCompD6 == mCompatibilityModeAudio)                  (|| T)
  // (avtpBase8[1] & 0x01) && mNumSkippedPackets >= mNumPacketsToSkip (T && F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[1]                          = 0u; // don't increment MEDIA_RESET
  avtpBase8[1]                         |= 0u; // don't increment TIMESTAMP_VALID
  avtpBase8[3]                          = 0u; // don't increment TIMESTAMP_UNCERTAIN
  mAudioStream->mCurrentAvbLockState    = avbRxClockDomainObj.getLockState();
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  mAudioStream->mNumSkippedPackets      = mAudioStream->mNumPacketsToSkip - 1u;
  mAudioStream->mCompatibilityModeAudio = IasAvbCompatibility::eIasAvbCompSaf;
  avtpBase16[11]                        = htons(maxNumberChannels); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // eIasAvbCompLatest == mCompatibilityModeAudio                     (F)
  // newState != oldState                                             (F)
  // numChannels > numLocalChannels                                   (T)
  // (eIasAvbCompSaf == mCompatibilityModeAudio)                     (T)
  //   || (eIasAvbCompD6 == mCompatibilityModeAudio)                  (|| F)
  // 0u == numChannels                                                (T)
  // (avtpBase8[1] & 0x01) && mNumSkippedPackets >= mNumPacketsToSkip (F && F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  mAudioStream->mNumSkippedPackets      = mAudioStream->mNumPacketsToSkip - 1u;
  mAudioStream->mCompatibilityModeAudio = IasAvbCompatibility::eIasAvbCompSaf;
  avtpBase16[11]                        = htons(1u); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // eIasAvbCompLatest == mCompatibilityModeAudio                     (F)
  // newState != oldState                                             (F)
  // numChannels > numLocalChannels                                   (F)
  // eIasAvbCompNgi == mCompatibilityModeAudio                        (T)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  localStream->mNumChannels             = 0u; // numLocalChannels
  avtpBase16[11]                        = htons(1u); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // sideChannel && (numLocalChannels > 0u)                           (T && F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateOnce;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamInactive;
  localStream->mNumChannels             = 0u; // numLocalChannels
  avtpBase16[11]                        = htons(1u); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  // newState != oldState                                             (T)
  // 0u == mValidationCount                                           (T)
  // isConnected()                                                    (F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateOnce;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamInactive;
  localStream->mNumChannels             = 0u; // numLocalChannels
  avtpBase16[11]                        = htons(1u); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  mAudioStream->mValidationCount        = 1u;
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  // newState != oldState                                             (T)
  // 0u == mValidationCount                                           (F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  // delete local stream and setup a "no side channel" one
  delete localStream;
  localStream = new LocalAudioDummyStream(mDltCtx,
                                  IasAvbStreamDirection::eIasAvbReceiveFromNetwork,
                                  localStreamId);
  bool noSideChannel = false;
  ASSERT_EQ(eIasAvbProcOK, localStream->init(maxNumberChannels - 1u,
                                             totalBufferSize,
                                             sampleFrequency,
                                             channelLayout,
                                             noSideChannel));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));
  ////

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateAlways;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  localStream->mNumChannels             = 0u; // numLocalChannels
  avtpBase16[11]                        = htons(maxNumberChannels); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // sideChannel && (numLocalChannels > 0u)                           (F && F)
  // sideChannel                                                      (F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateAlways;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState valid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;
  localStream->mNumChannels             = maxNumberChannels - 1u; // numLocalChannels
  avtpBase16[10]                        = htons(uint8_t(sizeof packet - IasAvbAudioStream::cAvtpHeaderSize));// payload
  avtpBase16[11]                        = htons(maxNumberChannels); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // sideChannel && (numLocalChannels > 0u)                           (F && T)
  // isConnected() && (numChannels > 0u)                              (T && T)
  // sideChannel                                                      (F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  localStream->mNumChannels             = maxNumberChannels; // numLocalChannels
  avtpBase8[1]                          = 0u;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase8[17]                         = 1u;
  avtpBase8[18]                         = 1 << 4;
  avtpBase16[11]                        = htons(maxNumberChannels); // numChannels
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);
  // newState != oldState                                             (T)
  // IasAvbStreamState::eIasAvbStreamValid == newState                (F)
  // IasAvbStreamState::eIasAvbStreamValid == oldState                (T)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[1]                          = 0u;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum);
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  // legacy SAF comp, for Saf16 the code should be 2u
  avtpBase8[16]                         = mAudioStream->getFormatCode(audioFormat) > 0u ? 0u : 1u;
  // avtpBase8[16] == mAudioFormatCode                                (F)
  mAudioStream->readFromAvbPacket(packet, 10u + IasAvbAudioStream::cAvtpHeaderSize - 1u);

  avtpBase8[1]                          = 0u;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency) > 0u ? 0u : 1u;// legacy SAF comp
  avtpBase16[10]                        = htons(10u);
  avtpBase8[16]                         = mAudioStream->getFormatCode(audioFormat);
  avtpBase16[11]                        = 0u; // numChannels
  // sampleFrequencyCode == mSampleFrequencyCode                      (F)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[1]                          = 0u;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase8[18]                         = mAudioStream->getSampleFrequencyCode(sampleFrequency);// legacy SAF comp
  avtpBase16[10]                        = htons(10u);
  avtpBase8[16]                         = mAudioStream->getFormatCode(audioFormat);
  avtpBase16[11]                        = 0u; // numChannels
  // (length - cAvtpHeaderSize) >= payloadLength                      (F)
  mAudioStream->readFromAvbPacket(packet, 10u + IasAvbAudioStream::cAvtpHeaderSize - 1u);

  avtpBase8[0]                          = 0u;
  avtpBase8[1]                          = 0u;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase16[11]                        = 0u; // numChannels
  // 0x02 == avtpBase8[0]                                             (F)
  mAudioStream->readFromAvbPacket(packet, 10u + IasAvbAudioStream::cAvtpHeaderSize - 1u);

  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase16[11]                        = 0u; // numChannels
  mAudioStream->mAudioFormat            = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  // IasAvbAudioFormat::eIasAvbAudioFormatIec61883 == mAudioFormat    (T)
  mAudioStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase16[11]                        = 0u; // numChannels
  // length >= cAvtpHeaderSize                                        (F)
  mAudioStream->readFromAvbPacket(packet, IasAvbAudioStream::cAvtpHeaderSize - 1u);

  mAudioStream->mValidationMode         = IasAvbAudioStream::cValidateNever;
  avtpBase8[2]                          = uint8_t(mAudioStream->mSeqNum + 1u); // newState invalid
  mAudioStream->mStreamState            = IasAvbStreamState::eIasAvbStreamValid;// newState != oldState
  avtpBase16[11]                        = 0u; // numChannels
  // cValidateNever == mValidationMode                                (F)
  mAudioStream->readFromAvbPacket(packet, IasAvbAudioStream::cAvtpHeaderSize - 1u);

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));

  delete localStream;
  localStream = NULL;
}
#endif

TEST_F(IasTestAvbAudioStream, ActivationChanged)
{
  ASSERT_TRUE(mAudioStream != NULL);

  mAudioStream->activationChanged();
  ASSERT_TRUE(1);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbStreamId avbStreamId(uint64_t(1u));
  uint32_t poolSize = 2u;
  IasAvbPtpClockDomain avbClockDomain;
  IasAvbMacAddress avbMacAddr = {};
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cBendCtrlStream, uint64_t(avbStreamId)));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamId,
                                                      poolSize,
                                                      &avbClockDomain,
                                                      avbMacAddr,
                                                      true));
  mAudioStream->activate();
}

#if 1 // TODO: replace JackStream!
TEST_F(IasTestAvbAudioStream, SignalDiscontinuity)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_TRUE(!mAudioStream->signalDiscontinuity(IasLocalAudioStreamClientInterface::eIasOverrun, 0));

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t numChannels = 2u;
  uint32_t totalBufferSize = 1024u;
  uint32_t jackSampleFrequency = 48000u;
  uint8_t  channelLayout = 2u;
  bool   hasSideChannel = false;
  uint16_t localStreamId = 0u;

  LocalAudioDummyStream* localStream = new LocalAudioDummyStream(mDltCtx, direction, localStreamId);
  ASSERT_EQ(eIasAvbProcOK, localStream->init(numChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));

  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbStreamId streamId;
  IasAvbMacAddress dmac = {};
  uint16_t vid = 0u;

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh,
                                                     numChannels,
                                                     jackSampleFrequency,
                                                     format,
                                                     streamId,
                                                     dmac,
                                                     vid,
                                                     true));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));

  ASSERT_TRUE(!mAudioStream->signalDiscontinuity(IasLocalAudioStreamClientInterface::eIasUnderrun, 0));

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
  localStream = NULL;
}
#endif

TEST_F(IasTestAvbAudioStream, UpdateRelativeFillLevel)
{
  ASSERT_TRUE(mAudioStream != NULL);

  float currFillLevel = mAudioStream->mFillLevelIndex;
  int testFillLevel = -1;
  mAudioStream->updateRelativeFillLevel(testFillLevel);
  ASSERT_EQ(currFillLevel, mAudioStream->mFillLevelIndex);

  mAudioStream->mRatioBendRate = 1.0f;
  mAudioStream->mAvbClockDomain = NULL;
  // (0.0 != mRatioBendRate) && (NULL != clock) (T && F)
  mAudioStream->updateRelativeFillLevel(testFillLevel);
  ASSERT_EQ(currFillLevel, mAudioStream->mFillLevelIndex);

  mAudioStream->mRatioBendRate = 0.0f;
  IasAvbPtpClockDomain avbClockDomainObj;
  mAudioStream->mAvbClockDomain = &avbClockDomainObj;
  // (0.0 != mRatioBendRate) && (NULL != clock) (F && T)
  mAudioStream->updateRelativeFillLevel(testFillLevel);
  ASSERT_EQ(currFillLevel, mAudioStream->mFillLevelIndex);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  IasAvbStreamId avbStreamIdObj(uint64_t(2u));
  IasAvbMacAddress avbMacAddr = {};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 24000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t poolSize = 2u;
  int maxBend = 300; // do not exceed 999
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cBendCtrlStream, uint64_t(avbStreamIdObj)));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioMaxBend, maxBend));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cDebugBufFName, "update.log"));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamIdObj,
                                                      poolSize,
                                                      &avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));
  mAudioStream->mFillLevelIndex = IasAvbAudioStream::cFillLevelFifoSize - 1u;
  testFillLevel = 1;
  int accumFill = -65;
  mAudioStream->mAccumulatedFillLevel = accumFill;
  mAudioStream->mRatioBendRate        = 10.0f;
  if (NULL != mAudioStream->mFillLevelFifo)
    mAudioStream->mFillLevelFifo[mAudioStream->mFillLevelIndex] = 0;
  // (0.0 != mRatioBendRate) && (NULL != clock) (T && T)
  // cFillLevelFifoSize == mFillLevelIndex      (T)
  // bend < -mRatioBendLimit                    (T)
  // mDebugLogCount == 94u                      (T)
  mAudioStream->updateRelativeFillLevel(testFillLevel);
  ASSERT_EQ(accumFill + testFillLevel, mAudioStream->mAccumulatedFillLevel);

  accumFill = 10;
  mAudioStream->mFillLevelIndex       = 1u;
  if (NULL != mAudioStream->mFillLevelFifo)
    mAudioStream->mFillLevelFifo[mAudioStream->mFillLevelIndex] = 0;
  mAudioStream->mAccumulatedFillLevel = accumFill;
  mAudioStream->mRatioBendRate        = 0.2f;
  // mDebugFile.is_open()                       (T)
  // 0 != bend                                  (F)
  mAudioStream->updateRelativeFillLevel(testFillLevel);
  ASSERT_EQ(accumFill + testFillLevel, mAudioStream->mAccumulatedFillLevel);

  accumFill                           = 10 * IasAvbAudioStream::cFillLevelFifoSize;
  mAudioStream->mRatioBendLimit       = 7;
  mAudioStream->mFillLevelIndex       = 2u;
  mAudioStream->mAccumulatedFillLevel = accumFill;
  mAudioStream->mDebugLogCount        = 94u;
  mAudioStream->mRatioBendRate        = 0.2f;
  mAudioStream->mDebugFile.close();
  if (NULL != mAudioStream->mFillLevelFifo)
    mAudioStream->mFillLevelFifo[mAudioStream->mFillLevelIndex] = 0;
  // cFillLevelFifoSize == mFillLevelIndex      (F)
  // bend > mRatioBendLimit                     (T)
  mAudioStream->updateRelativeFillLevel(testFillLevel);
  ASSERT_EQ(accumFill + testFillLevel, mAudioStream->mAccumulatedFillLevel);
}
//

TEST_F(IasTestAvbAudioStream, BRANCH_Gets_Sets)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_TRUE(createEnvironment());

  // ------------------- getPacketSize ----------------------------
  ASSERT_EQ(32u, mAudioStream->getPacketSize(IasAvbAudioFormat::eIasAvbAudioFormatIec61883, 0));
  ASSERT_EQ(24u, mAudioStream->getPacketSize(IasAvbAudioFormat::eIasAvbAudioFormatSaf16, 0));
  ASSERT_EQ(24u, mAudioStream->getPacketSize(IasAvbAudioFormat::eIasAvbAudioFormatSaf24, 0));
  ASSERT_EQ(24u, mAudioStream->getPacketSize(IasAvbAudioFormat::eIasAvbAudioFormatSaf32, 0));
  ASSERT_EQ(24u, mAudioStream->getPacketSize(IasAvbAudioFormat::eIasAvbAudioFormatSafFloat, 0));

  // default case
  ASSERT_EQ(0u, mAudioStream->getPacketSize((IasAvbAudioFormat)-1, 0));

  // ------------------- getSampleSize ----------------------------
  ASSERT_EQ(4u, mAudioStream->getSampleSize(IasAvbAudioFormat::eIasAvbAudioFormatIec61883));
  ASSERT_EQ(2u, mAudioStream->getSampleSize(IasAvbAudioFormat::eIasAvbAudioFormatSaf16));
  ASSERT_EQ(3u, mAudioStream->getSampleSize(IasAvbAudioFormat::eIasAvbAudioFormatSaf24));
  ASSERT_EQ(4u, mAudioStream->getSampleSize(IasAvbAudioFormat::eIasAvbAudioFormatSaf32));
  ASSERT_EQ(4u, mAudioStream->getSampleSize(IasAvbAudioFormat::eIasAvbAudioFormatSafFloat));

  // default case
  ASSERT_EQ(0u, mAudioStream->getSampleSize((IasAvbAudioFormat)-1));

  // ------------------- signalDiscontinuity ----------------------------
  ASSERT_FALSE(mAudioStream->signalDiscontinuity(IasAvbAudioStream::eIasUnspecific, 0));

  uint32_t sampleFrequency = 48000u;
  IasAvbStreamId avbStreamIdObj;
  IasAvbPtpClockDomain avbClockDomainObj;
  IasAvbMacAddress avbMacAddr = {};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;


  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh, 2, sampleFrequency, audioFormat,
                                                     avbStreamIdObj, avbMacAddr, 2, true));
  ASSERT_FALSE(mAudioStream->signalDiscontinuity(IasAvbAudioStream::eIasUnspecific, 0));
  ASSERT_TRUE(mAudioStream->signalDiscontinuity(IasAvbAudioStream::eIasOverrun, 0));
  ASSERT_FALSE(mAudioStream->signalDiscontinuity(IasAvbAudioStream::eIasUnderrun, 0));

  // default case
  ASSERT_FALSE(mAudioStream->signalDiscontinuity((IasAvbAudioStream::DiscontinuityEvent)-1, 0));

  // ------------------- getFormatCode ----------------------------
  ASSERT_EQ(4u, mAudioStream->getFormatCode(IasAvbAudioFormat::eIasAvbAudioFormatSaf16));
  ASSERT_EQ(3u, mAudioStream->getFormatCode(IasAvbAudioFormat::eIasAvbAudioFormatSaf24));
  ASSERT_EQ(2u, mAudioStream->getFormatCode(IasAvbAudioFormat::eIasAvbAudioFormatSaf32));
  ASSERT_EQ(1u, mAudioStream->getFormatCode(IasAvbAudioFormat::eIasAvbAudioFormatSafFloat));
  ASSERT_EQ(0u, mAudioStream->getFormatCode(IasAvbAudioFormat::eIasAvbAudioFormatIec61883));

  ASSERT_EQ(0u, mAudioStream->getFormatCode((IasAvbAudioFormat)-1));

  setConfigValue(IasRegKeys::cCompatibilityAudio, "SAF");
  ASSERT_EQ(0u, mAudioStream->getFormatCode(IasAvbAudioFormat::eIasAvbAudioFormatSaf24));
}
//

TEST_F(IasTestAvbAudioStream, BRANCH_getLocalNumChannels_StreamId)
{
  ASSERT_TRUE(mAudioStream != NULL);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  uint16_t maxNumberChannels      = 6u;
  uint32_t sampleFrequency        = 48000u;
  IasAvbStreamId avbStreamId(uint64_t(1u));
  IasAvbMacAddress avbMacAddr   = {0};
  IasAvbAudioFormat audioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;

  LocalAudioDummyStream * localStream = new LocalAudioDummyStream(mDltCtx, IasAvbStreamDirection::eIasAvbTransmitToNetwork, 1u);

  uint32_t poolSize = 2u;
  IasAvbPtpClockDomain avbClockDomain;

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFrequency,
                                                      audioFormat,
                                                      avbStreamId,
                                                      poolSize,
                                                      &avbClockDomain,
                                                      avbMacAddr,
                                                      true));

  uint32_t totalBufferSize = 256u;
  uint32_t jackSampleFrequency = 48000u;
  uint8_t  channelLayout = 2u;
  bool   hasSideChannel = true;

  ASSERT_EQ(eIasAvbProcOK, localStream->init(maxNumberChannels,
                                             totalBufferSize,
                                             jackSampleFrequency,
                                             channelLayout,
                                             hasSideChannel));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(localStream));

  ASSERT_EQ(maxNumberChannels, mAudioStream->getLocalNumChannels());
  ASSERT_EQ(uint64_t(avbStreamId), mAudioStream->getLocalStreamId());

  ASSERT_EQ(eIasAvbProcOK, mAudioStream->connectTo(NULL));
  delete localStream;
}

TEST_F(IasTestAvbAudioStream, BRANCH_UpdateRelativeFillLevel)
{
  ASSERT_TRUE(mAudioStream != NULL);

  mAudioStream->updateRelativeFillLevel(0.0);
  mAudioStream->updateRelativeFillLevel(-1.0);

  for (uint8_t index = 94 - 1; index > 0; --index)
    mAudioStream->updateRelativeFillLevel(0.0);
}

TEST_F(IasTestAvbAudioStream, HEAP_fail)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  // Tests passes only to IasAvbStream (PacketPool already tested in its test class)

  heapSpaceLeft = 0;

  // Transmiter
  IasAvbStreamId streamID((uint64_t)1);
  IasAvbMacAddress macAddr = {1}; // Specifically Non-zero
  IasAvbPtpClockDomain ptpClock;

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(IasAvbSrClass::eIasAvbSrClassHigh, 1, 48000u,
                                  IasAvbAudioFormat::eIasAvbAudioFormatSaf16, streamID, 1, &ptpClock, macAddr, true));

  heapSpaceLeft = sizeof(IasAvbTSpec);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initTransmit(IasAvbSrClass::eIasAvbSrClassHigh, 1, 48000u,
                                  IasAvbAudioFormat::eIasAvbAudioFormatSaf16, streamID, 1, &ptpClock, macAddr, true));

  // Receiver
  IasAvbStreamId avbStreamIdObj;
  IasAvbMacAddress avbMacAddr = {};

  heapSpaceLeft = 0;

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh, 2, 48000u,
                                      IasAvbAudioFormat::eIasAvbAudioFormatSaf16, avbStreamIdObj, avbMacAddr, 2, true));

  heapSpaceLeft = sizeof(IasAvbTSpec) + sizeof(IasAvbStreamId);

  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAudioStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh, 2, 48000u,
                                      IasAvbAudioFormat::eIasAvbAudioFormatSaf16, avbStreamIdObj, avbMacAddr, 2, true));
}

TEST_F(IasTestAvbAudioStream, getFormatCode)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_TRUE(createEnvironment());

  setConfigValue(IasRegKeys::cCompatibilityAudio, "bad");
  ASSERT_EQ(0u, mAudioStream->getSampleFrequencyCode(0u));
  ASSERT_EQ(0u, mAudioStream->getSampleFrequencyCode(1234u));

  setConfigValue(IasRegKeys::cCompatibilityAudio, "SAF");
  ASSERT_EQ(0u, mAudioStream->getSampleFrequencyCode(8000u));

  setConfigValue(IasRegKeys::cCompatibilityAudio, "d6_1722a");
  ASSERT_EQ(5u, mAudioStream->getSampleFrequencyCode(48000u));
  ASSERT_EQ(0u, mAudioStream->getSampleFrequencyCode(24000u));
}

TEST_F(IasTestAvbAudioStream, getCompatibilityModeAudio)
{
  ASSERT_TRUE(mAudioStream != NULL);
  ASSERT_TRUE(createEnvironment());

  setConfigValue(IasRegKeys::cCompatibilityAudio, "d6_1722a");
  ASSERT_EQ(eIasAvbCompD6, mAudioStream->getCompatibilityModeAudio());

  setConfigValue(IasRegKeys::cCompatibilityAudio, "latest");
  ASSERT_EQ(eIasAvbCompLatest, mAudioStream->getCompatibilityModeAudio());
}

TEST_F(IasTestAvbAudioStream, resetTime)
{
  ASSERT_TRUE(mAudioStream != NULL);

  ASSERT_TRUE(createEnvironment());
  uint64_t next = 1u;
  IasAvbRxStreamClockDomain clockDomain;
  mAudioStream->mAvbClockDomain = &clockDomain;

  // eventRate = 0
  mAudioStream->mPacketLaunchTime = 0u;
  ASSERT_FALSE(mAudioStream->resetTime(next));
  ASSERT_TRUE(next == mAudioStream->mPacketLaunchTime);

  // eventRate != 0
  // masterTime = 0
  clockDomain.reset(IasAvbSrClass::eIasAvbSrClassHigh, uint64_t(0u), 48000u);
  mAudioStream->mPacketLaunchTime = 0u;
  ASSERT_FALSE(mAudioStream->resetTime(next));
  ASSERT_TRUE(next == mAudioStream->mPacketLaunchTime);

  // eventRate != 0
  // masterTime != 0
  // masterTime < next
  clockDomain.update(6u, 125000u, 125000u, 125000u);
  next = 7u;
  mAudioStream->mPacketLaunchTime = 0u;
  ASSERT_TRUE(mAudioStream->resetTime(next));
  ASSERT_TRUE(0u != mAudioStream->mPacketLaunchTime);

  // eventRate != 0
  // masterTime != 0
  // masterTime >= next
  clockDomain.update(6u, 125000u, 125000u, 125000u);
  mAudioStream->mPacketLaunchTime = 0u;
  ASSERT_TRUE(mAudioStream->resetTime(next));
  ASSERT_TRUE(0u != mAudioStream->mPacketLaunchTime);

  uint64_t masterTime;
  clockDomain.getEventCount(masterTime);
  ASSERT_TRUE(mAudioStream->resetTime(masterTime + 1u));

  mAudioStream->mLastRefPlaneSampleTime = masterTime + 2u;
  ASSERT_TRUE(mAudioStream->resetTime(masterTime + 1u));
}

TEST_F(IasTestAvbAudioStream, prepareAllPackets)
{
  ASSERT_TRUE(mAudioStream != NULL);
  IasAvbPtpClockDomain avbClockDomainObj;
  mAudioStream->mAvbClockDomain = &avbClockDomainObj;
  IasAvbStreamId avbStreamIdObj(uint64_t(2u));
  IasAvbMacAddress avbMacAddr = {};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 24000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t poolSize = 2u;
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamIdObj,
                                                      poolSize,
                                                      &avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));
  // 1722 header - subtype set to 0
  mAudioStream->mAudioFormat = IasAvbAudioFormat::eIasAvbAudioFormatIec61883;
  // not sparse
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioSparseTS, 0));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->prepareAllPackets());

  // sparse
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cAudioSparseTS, 1));
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->prepareAllPackets());

  IasAvbPacketPool * mPool = &mAudioStream->getPacketPool();
  mPool->mLock.lock();
  mPool->mFreeBufferStack.clear();
  mPool->mLock.unlock();
  // NULL == referencePacket
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAudioStream->prepareAllPackets());
}

TEST_F(IasTestAvbAudioStream, getMaxTransmitTime)
{
  ASSERT_TRUE(mAudioStream != NULL);
  IasAvbPtpClockDomain avbClockDomainObj;
  mAudioStream->mAvbClockDomain = &avbClockDomainObj;
  IasAvbStreamId avbStreamIdObj(uint64_t(2u));
  IasAvbMacAddress avbMacAddr = {};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t poolSize = 2u;
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamIdObj,
                                                      poolSize,
                                                      &avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));

  uint32_t sampleIntervalNs = uint32_t(1.0e9 / float(sampleFreq));
  uint32_t ptOffset = (2000000u - 125000u);
  ptOffset = ((ptOffset + (sampleIntervalNs - 1u)) / sampleIntervalNs) * sampleIntervalNs;

  ASSERT_EQ(ptOffset, mAudioStream->getMaxTransmitTime()); // see mPresentationTimeOffsetTable[]
}

TEST_F(IasTestAvbAudioStream, getMinTransmitBufferSize)
{
  ASSERT_TRUE(mAudioStream != NULL);
  IasAvbPtpClockDomain avbClockDomainObj;
  mAudioStream->mAvbClockDomain = &avbClockDomainObj;
  IasAvbStreamId avbStreamIdObj(uint64_t(2u));
  IasAvbMacAddress avbMacAddr = {};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t poolSize = 2u;
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  ASSERT_EQ(eIasAvbProcOK, mAudioStream->initTransmit(srClass,
                                                      maxNumberChannels,
                                                      sampleFreq,
                                                      format,
                                                      avbStreamIdObj,
                                                      poolSize,
                                                      &avbClockDomainObj,
                                                      avbMacAddr,
                                                      true));

  uint32_t periodCycle = 4000000u; // 4ms
  ASSERT_EQ(384u, mAudioStream->getMinTransmitBufferSize(periodCycle));
}

TEST_F(IasTestAvbAudioStream, compareAttributes)
{
  ASSERT_TRUE(mAudioStream != NULL);

  uint64_t streamId = 0u;
  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t maxNumberChannels = 2u;
  uint32_t sampleFreq = 48000u;
  IasAvbAudioFormat format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  uint32_t clockId = 0u;
  uint64_t dmac = {};
  uint64_t avbMacAddr = {};
  IasAvbStreamState txStatus = IasAvbStreamState::eIasAvbStreamInactive;
  IasAvbStreamState rxStatus = IasAvbStreamState::eIasAvbStreamInactive;
  bool preconfigured = false;
  IasAvbStreamDiagnostics diagnostics = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  IasAvbStreamDiagnostics t_diagnostics = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  ASSERT_TRUE(diagnostics != t_diagnostics);

  IasAvbAudioStreamAttributes first_att;
  first_att.setStreamId(streamId);
  first_att.setDirection(direction);
  first_att.setNumChannels(maxNumberChannels);
  first_att.setMaxNumChannels(maxNumberChannels);
  first_att.setSampleFreq(sampleFreq);
  first_att.setFormat(format);
  first_att.setClockId(clockId);
  first_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
  first_att.setStreamId(streamId);
  first_att.setDmac(dmac);
  first_att.setSourceMac(avbMacAddr);
  first_att.setTxActive(txStatus);
  first_att.setRxStatus(rxStatus);
  first_att.setLocalStreamId(streamId);
  first_att.setPreconfigured(preconfigured);
  first_att.setDiagnostics(diagnostics);

  first_att = first_att;
  ASSERT_EQ(first_att, first_att);

  IasAvbAudioStreamAttributes second_att;
  second_att.setStreamId(streamId);
  second_att.setDirection(direction);
  second_att.setNumChannels(maxNumberChannels);
  second_att.setMaxNumChannels(maxNumberChannels);
  second_att.setSampleFreq(sampleFreq);
  second_att.setFormat(format);
  second_att.setClockId(clockId);
  second_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
  second_att.setStreamId(streamId);
  second_att.setDmac(dmac);
  second_att.setSourceMac(avbMacAddr);
  second_att.setTxActive(txStatus);
  second_att.setRxStatus(rxStatus);
  second_att.setLocalStreamId(streamId);
  second_att.setPreconfigured(preconfigured);
  second_att.setDiagnostics(diagnostics);

  bool result = false;
  if(first_att == second_att)
  {
      result = true;
  }
  ASSERT_TRUE(result);


  streamId = 1u;
  second_att.setStreamId(streamId);
  ASSERT_EQ(streamId, second_att.getStreamId());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  streamId = 0u;
  second_att.setStreamId(streamId);


  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  second_att.setDirection(direction);
  ASSERT_EQ(direction, second_att.getDirection());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  second_att.setDirection(direction);


  maxNumberChannels = 4u;
  second_att.setNumChannels(maxNumberChannels);
  ASSERT_EQ(maxNumberChannels, second_att.getNumChannels());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  maxNumberChannels = 2u;
  second_att.setNumChannels(maxNumberChannels);


  maxNumberChannels = 4u;
  second_att.setMaxNumChannels(maxNumberChannels);
  ASSERT_EQ(maxNumberChannels, second_att.getMaxNumChannels());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  maxNumberChannels = 2u;
  second_att.setMaxNumChannels(maxNumberChannels);


  sampleFreq = 24000u;
  second_att.setSampleFreq(sampleFreq);
  ASSERT_EQ(sampleFreq, second_att.getSampleFreq());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  sampleFreq = 48000u;
  second_att.setSampleFreq(sampleFreq);


  format = IasAvbAudioFormat::eIasAvbAudioFormatSaf32;
  second_att.setFormat(format);
  ASSERT_EQ(format, second_att.getFormat());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  format = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  second_att.setFormat(format);


  clockId = 1u;
  second_att.setClockId(clockId);
  ASSERT_EQ(clockId, second_att.getClockId());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  clockId = 0u;
  second_att.setClockId(clockId);

  second_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll);
  result = true;
  ASSERT_EQ(IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll, second_att.getAssignMode());
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  second_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);


  dmac = {1};
  second_att.setDmac(dmac);
  ASSERT_EQ(dmac, second_att.getDmac());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  dmac = {};
  second_att.setDmac(dmac);


  avbMacAddr = {1};
  second_att.setSourceMac(avbMacAddr);
  ASSERT_EQ(avbMacAddr, second_att.getSourceMac());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  avbMacAddr = {};
  second_att.setSourceMac(avbMacAddr);


  txStatus = IasAvbStreamState::eIasAvbStreamValid;
  second_att.setTxActive(txStatus);
  ASSERT_EQ(true, second_att.getTxActive());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  txStatus = IasAvbStreamState::eIasAvbStreamInactive;
  second_att.setTxActive(txStatus);


  rxStatus = IasAvbStreamState::eIasAvbStreamValid;
  second_att.setRxStatus(rxStatus);
  ASSERT_EQ(rxStatus, second_att.getRxStatus());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  txStatus = IasAvbStreamState::eIasAvbStreamInactive;
  second_att.setRxStatus(rxStatus);


  streamId = 1u;
  second_att.setLocalStreamId(streamId);
  ASSERT_EQ(streamId, second_att.getLocalStreamId());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  streamId = 0u;
  second_att.setLocalStreamId(streamId);


  preconfigured = true;
  second_att.setPreconfigured(preconfigured);
  ASSERT_EQ(preconfigured, second_att.getPreconfigured());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  preconfigured = false;
  second_att.setPreconfigured(preconfigured);


  diagnostics = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  second_att.setDiagnostics(diagnostics);
  ASSERT_EQ(diagnostics, second_att.getDiagnostics());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  diagnostics = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  second_att.setDiagnostics(diagnostics);

  result = false;
  if(first_att != second_att)
  {
      result = true;
  }
  ASSERT_TRUE(result);

  IasAvbAudioStreamAttributes third_att;
  third_att = first_att;
  ASSERT_EQ(first_att, third_att);

  IasAvbAudioStreamAttributes fourth_att(direction, maxNumberChannels, maxNumberChannels, sampleFreq, format, clockId,
                                         IasAvbIdAssignMode::eIasAvbIdAssignModeStatic, streamId, dmac, avbMacAddr,
                                         txStatus, rxStatus, streamId, preconfigured, diagnostics);
  diagnostics = t_diagnostics;
  ASSERT_EQ(diagnostics, t_diagnostics);

  t_diagnostics = t_diagnostics;
  ASSERT_EQ(diagnostics, t_diagnostics);

  diagnostics.setMediaLocked(0u);
  ASSERT_EQ(0u, diagnostics.getMediaLocked());

  diagnostics.setMediaUnlocked(0u);
  ASSERT_EQ(0u, diagnostics.getMediaUnlocked());
}

} //IasMediaTransportAvb
