/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestAlsaWorkerThread.cpp
 * @brief   The implementation of the IasTestAlsaWorkerThread test class.
 * @date    2015
 */

#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAlsaStreamInterface.hpp"
#include "avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"
#include "avb_streamhandler/IasAlsaWorkerThread.hpp"
#define protected protected
#define private private

using namespace IasMediaTransportAvb;
using std::nothrow;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb
{

class IasTestAlsaWorkerThread : public ::testing::Test
{
protected:
  IasTestAlsaWorkerThread()
    : mAlsaWorkerThread(NULL)
     ,mEnvironment(NULL)
     ,mAlsaAudioFormat(IasAvbAudioFormat::eIasAvbAudioFormatSaf16)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAlsaWorkerThread()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {

    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAlsaWorkerThread",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);

    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);

    mAlsaWorkerThread = new (nothrow) IasAlsaWorkerThread(mDltContext);

    heapSpaceLeft = heapSpaceInitSize;

    mStream = NULL;
  }

  virtual void TearDown()
  {
    mAlsaWorkerThread->cleanup();
    delete mAlsaWorkerThread;
    mAlsaWorkerThread = NULL;

    if (NULL != mEnvironment)
    {
      delete mEnvironment;
      mEnvironment = NULL;
    }

    if (NULL != mStream)
    {
      delete mStream;
      mStream = NULL;
    }

    DLT_UNREGISTER_CONTEXT(mDltContext);
    heapSpaceLeft = heapSpaceInitSize;
  }

  IasAvbProcessingResult initDefaultStream(IasAlsaStreamInterface* stream)
  {
    IasAvbProcessingResult result = eIasAvbProcInvalidParam;

    if (NULL != stream)
    {

      uint16_t numChannels              = 2u;
      uint32_t totalLocalBufferSize     = 256u;
      uint32_t optimalFillLevel         = totalLocalBufferSize / 2u;
      uint32_t periodSize               = 8u;
      uint32_t numAlsaBuffers           = 4u;
      uint32_t sampleFrequency          = 24000u;
      IasAvbAudioFormat format          = mAlsaAudioFormat;
      uint8_t channelLayout             = 2u;
      bool hasSideChannel               = true;
      std:string deviceName             = "avbtestdev";
      IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

      result = stream->init(numChannels, totalLocalBufferSize, optimalFillLevel, periodSize,
          numAlsaBuffers, sampleFrequency, format, channelLayout, hasSideChannel, deviceName, useAlsaDeviceType);
    }

    return result;
  }

  IasAlsaWorkerThread * mAlsaWorkerThread;
  DltContext mDltContext;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  IasAlsaStreamInterface *mStream;
  IasAvbAudioFormat mAlsaAudioFormat;
};

TEST_F(IasTestAlsaWorkerThread, Ctor_Dtor)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);
}

TEST_F(IasTestAlsaWorkerThread, init)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  //IasAlsaStreamInterface * nullStream = NULL;
  IasAlsaVirtualDeviceStream * nullStream = NULL;
  IasAvbPtpClockDomain * clockDomain = NULL;
  uint32_t period = 0u;
  uint32_t frequency = 0u;
  // (NULL != alsaStream) && (NULL != clockDomain) ... (F && F && F && F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->init(nullStream, period, frequency, clockDomain));

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t streamId(0u);
  //mStream = new (nothrow) IasAlsaStreamInterface(mDltContext, direction, streamId);
  mStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, direction, streamId);

  // (NULL != alsaStream) && (NULL != clockDomain) (T && F && F && F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->init(mStream, period, frequency, clockDomain));

  IasAvbPtpClockDomain ptpClockDomain;
  // (NULL != alsaStream) && (NULL != clockDomain) ... (T && T && F && F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->init(mStream, period, frequency, &ptpClockDomain));

  period = 8u;
  // (NULL != alsaStream) && (NULL != clockDomain) ... (T && T && T && F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->init(mStream, period, frequency, &ptpClockDomain));

  frequency = 24000u;
  heapSpaceLeft = sizeof(IasThread) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mAlsaWorkerThread->init(mStream, period, frequency, &ptpClockDomain));

  heapSpaceLeft = heapSpaceInitSize;
  ASSERT_EQ(eIasAvbProcOK, initDefaultStream(mStream));
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->init(mStream, period, frequency, &ptpClockDomain));
  // already initialized
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAlsaWorkerThread->init(mStream, period, frequency, &ptpClockDomain));
}

TEST_F(IasTestAlsaWorkerThread, init_branch)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t streamId(0u);
  mStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, direction, streamId);

  IasAvbPtpClockDomain ptpClockDomain;
  uint32_t period = 8u;
  uint32_t frequency = 24000u;
  mAlsaWorkerThread->mAlsaStreams.insert(mAlsaWorkerThread->mAlsaStreams.begin(), mStream);

  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->init(mStream, period, frequency, &ptpClockDomain));
}

TEST_F(IasTestAlsaWorkerThread, start_stop)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);
  // mThread != NULL (F)
  ASSERT_EQ(eIasAvbProcThreadStartFailed, mAlsaWorkerThread->start());

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t streamId(0u);
  mStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, direction, streamId);
  IasAvbPtpClockDomain ptpClockDomain;
  ASSERT_EQ(eIasAvbProcOK, initDefaultStream(mStream));
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->init(mStream, mStream->getPeriodSize(), mStream->getSampleFrequency(), &ptpClockDomain));
  // mThread != NULL (T)
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->start());
  // !mThread->isRunning() (F)
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->start());
  // mThread->isRunning() (T)
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->stop());

  delete mAlsaWorkerThread->mThread;
  mAlsaWorkerThread->mThread = NULL;
  // mThread != NULL (F)
  ASSERT_EQ(eIasAvbProcThreadStopFailed, mAlsaWorkerThread->stop());
}

TEST_F(IasTestAlsaWorkerThread, run)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  IasAvbStreamDirection recvDirection = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t rxStreamId(0u);
  mStream = new (nothrow) IasAlsaVirtualDeviceStream(mDltContext, recvDirection, rxStreamId);

  uint16_t numChannels          = 2u;
  uint32_t totalLocalBufferSize = 2u;
  uint32_t optimalFillLevel     = 2u;
  uint32_t alsaPeriodSize       = 2u;
  uint32_t numAlsaBuffers       = 2u;
  uint32_t alsaSampleFrequency  = 24000u;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t channelLayout         = 0u;
  bool hasSideChannel           = true;
  std:string deviceName         = "avbtestdev";
  IasAvbPtpClockDomain ptpClockDomain;
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;

  ASSERT_EQ(eIasAvbProcOK, mStream->init(numChannels,
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
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->init(mStream, alsaPeriodSize, alsaSampleFrequency, &ptpClockDomain));
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->start());
}

TEST_F(IasTestAlsaWorkerThread, streamIsHandled)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  ASSERT_FALSE(mAlsaWorkerThread->streamIsHandled(0u));
}

TEST_F(IasTestAlsaWorkerThread, process)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  mAlsaWorkerThread->process();

  IasAvbStreamDirection recvDirection = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  IasAvbStreamDirection txDirection = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  uint16_t rxStreamId(0u),txStreamId(1u), nullIpcStreamId(2u);
  IasAlsaDeviceTypes useAlsaDeviceType = eIasAlsaVirtualDevice;
  IasAlsaVirtualDeviceStream rxAlsaStream(mDltContext, recvDirection, rxStreamId),
                txAlsaStream(mDltContext, txDirection,   txStreamId),
                nullIpcStream(mDltContext, txDirection,  nullIpcStreamId);

  uint16_t numChannels          = 2u;
  uint32_t totalLocalBufferSize = 2u;
  uint32_t optimalFillLevel     = 2u;
  uint32_t alsaPeriodSize       = 2u;
  uint32_t numAlsaBuffers       = 2u;
  uint32_t alsaSampleFrequency  = 24000u;
  IasAvbAudioFormat format      = mAlsaAudioFormat;
  uint8_t channelLayout         = 0u;
  bool hasSideChannel           = true;
  std:string deviceName         = "avbtestdev";
  IasAvbPtpClockDomain ptpClockDomain;

  ASSERT_EQ(eIasAvbProcOK, rxAlsaStream.init(numChannels,
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
  ASSERT_EQ(eIasAvbProcOK, txAlsaStream.init(numChannels,
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
  ASSERT_EQ(eIasAvbProcOK, nullIpcStream.init(numChannels,
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
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->init(&rxAlsaStream, alsaPeriodSize, alsaSampleFrequency, &ptpClockDomain));
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->addAlsaStream(&txAlsaStream));
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->addAlsaStream(&nullIpcStream));
  IasAvbAudioShmProvider *tempProv = nullIpcStream.mShm;
  nullIpcStream.mShm = NULL;
  mAlsaWorkerThread->mServiceCycle = 3u;
  txAlsaStream.mCycle = 2u;

  mAlsaWorkerThread->process();
  nullIpcStream.mShm = tempProv;
}

TEST_F(IasTestAlsaWorkerThread, shutDown)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  IasResult result = mAlsaWorkerThread->shutDown();
  ASSERT_EQ(0, result.getValue());
}

TEST_F(IasTestAlsaWorkerThread, addAlsaStream)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t streamId(0u);
  IasAlsaVirtualDeviceStream alsaStream(mDltContext, direction, streamId);

  ASSERT_EQ(eIasAvbProcOK, initDefaultStream(&alsaStream));

  ASSERT_EQ(eIasAvbProcNotInitialized, mAlsaWorkerThread->addAlsaStream(&alsaStream));

  IasAvbPtpClockDomain ptpClockDomain;
  // init includes addAlsaStream()
  ASSERT_EQ(eIasAvbProcOK, mAlsaWorkerThread->init(&alsaStream, alsaStream.getPeriodSize(), alsaStream.getSampleFrequency(), &ptpClockDomain));

  // stream already added
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->addAlsaStream(&alsaStream));

  IasAlsaStreamInterface * nullStream = NULL;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->addAlsaStream(nullStream));

  alsaStream.mSampleFrequency = 0u;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->addAlsaStream(&alsaStream));

  alsaStream.mSampleFrequency = 24000u;
  alsaStream.mPeriodSize      = 0u;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->addAlsaStream(&alsaStream));

  bool lastStream = false;
  mAlsaWorkerThread->removeAlsaStream(&alsaStream, lastStream);
  alsaStream.mPeriodSize      = 9u;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->addAlsaStream(&alsaStream));
}

TEST_F(IasTestAlsaWorkerThread, removeAlsaStream)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  IasAlsaVirtualDeviceStream * nullStream = NULL;
  bool laststream;
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->removeAlsaStream(nullStream, laststream));

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  uint16_t streamId(0u);
  IasAlsaVirtualDeviceStream alsaStream(mDltContext, direction, streamId);
  // stream not found
  ASSERT_EQ(eIasAvbProcInvalidParam, mAlsaWorkerThread->removeAlsaStream(&alsaStream, laststream));
}

TEST_F(IasTestAlsaWorkerThread, getClockDomain)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);
  ASSERT_TRUE(NULL == mAlsaWorkerThread->getClockDomain());
}

TEST_F(IasTestAlsaWorkerThread, isInitialized)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);
  ASSERT_FALSE(mAlsaWorkerThread->isInitialized());
}

TEST_F(IasTestAlsaWorkerThread, checkParameter)
{
  ASSERT_TRUE(NULL != mAlsaWorkerThread);

  ASSERT_FALSE(mAlsaWorkerThread->checkParameter(0u, 0u, 0u, 0u));
  ASSERT_FALSE(mAlsaWorkerThread->checkParameter(1u, 1u, 2u, 3u));
}
}//namespace IasMediaTransportAvb
