/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaVirtualDeviceStream.cpp
 * @brief   Implementation of a Alsa stream audio data container
 * @details
 * @date    2018
 */

#include "avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"

#include <pthread.h>
#include <sstream>
#include <iomanip>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAlsaVirtualDeviceStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"


/**
 *  Constructor.
 */
IasAlsaVirtualDeviceStream::IasAlsaVirtualDeviceStream(DltContext& dltContext, IasAvbStreamDirection direction, uint16_t streamId)
  : IasLocalAudioStream(dltContext, direction, eIasAlsaStream, streamId)
  , mShm(nullptr)
  , mOptimalFillLevel(0u)
  , mPeriodSize(0u)
  , mNumAlsaPeriods(0u)
  , mCycle(1u)
  , mCycleCount(1u)
  , mAlsaDeviceType(IasAlsaDeviceTypes::eIasAlsaVirtualDevice)
{
}


/**
 *  Destructor.
 */
IasAlsaVirtualDeviceStream::~IasAlsaVirtualDeviceStream()
{
  cleanup();
}


/**
 *  Initialization method.
 */
IasAvbProcessingResult IasAlsaVirtualDeviceStream::init(uint16_t numChannels, uint32_t totalLocalBufferSize, uint32_t optimalFillLevel,
                                           uint32_t alsaPeriodSize, uint32_t numAlsaPeriods, uint32_t alsaSampleFrequency,
                                           IasAvbAudioFormat format, uint8_t channelLayout, bool hasSideChannel,
                                           std::string deviceName, IasAlsaDeviceTypes alsaDeviceType)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if ( (eIasAlsaVirtualDevice != alsaDeviceType) || (0u == alsaPeriodSize) || (0u == numAlsaPeriods) || (deviceName.empty()))
  {
    // other parameter range checks are done by base class
    ret = eIasAvbProcInvalidParam;
  }
  else if (IasAvbAudioFormat::eIasAvbAudioFormatSaf16 != format)
  {
    ret = eIasAvbProcUnsupportedFormat;
  }
  else
  {
    ret = IasLocalAudioStream::init(channelLayout, numChannels, hasSideChannel, totalLocalBufferSize,
                                    alsaSampleFrequency, alsaPeriodSize);
    if (eIasAvbProcOK == ret)
    {
      mOptimalFillLevel = optimalFillLevel;
      mAlsaDeviceType   = alsaDeviceType;

      mShm = new (nothrow) IasAvbAudioShmProvider(deviceName);
      if (NULL == mShm)
      {
        /*
         * @log Not enough memory: Not enough memory available to create AudioShmProvider.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not create IPC!");
        ret = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        ret = mShm->init(numChannels, alsaPeriodSize, numAlsaPeriods, alsaSampleFrequency,
                         getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork ? false : true);
        mNumAlsaPeriods = numAlsaPeriods;
        if (eIasAvbProcOK != ret)
        {
          /*
           * @log Init failed: The initialization for AudioShmProvider failed.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "IPC init failed! ret=", int32_t(ret));
        }
      }
    }
  }

  if (eIasAvbProcOK == ret)
  {
    mPeriodSize = alsaPeriodSize;
  }
  else
  {
    /*
     * @log ALSA stream init failed
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ALSA stream init failed, reason=", int32_t(ret));
    cleanup();
  }

  return ret;
}


/*
 *  Reset method.
 */
IasAvbProcessingResult IasAlsaVirtualDeviceStream::resetBuffers()
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  IasLocalAudioBufferDesc *descQ = NULL;
  if (true == hasBufferDesc())
  {
    descQ = getBufferDescQ();
    AVB_ASSERT(NULL != descQ);
    descQ->lock();
  }

  for(int channelIdx=0; channelIdx< mNumChannels; channelIdx++)
  {
    IasLocalAudioBuffer *buffer = IasLocalAudioStream::getChannelBuffers()[channelIdx];
    AVB_ASSERT(NULL != buffer);

    if (true == hasBufferDesc())
    {
      IasLocalAudioBuffer::AudioData * dummyData = NULL;

      uint32_t fillLevel = buffer->getFillLevel();
      if (mOptimalFillLevel == fillLevel)
      {
        continue;
      }
      else if (mOptimalFillLevel < fillLevel)
      {
        // discard samples
        uint32_t bufferSize = fillLevel - mOptimalFillLevel;
        uint16_t samplesRead = 0;

        dummyData = new (nothrow) IasLocalAudioBuffer::AudioData[bufferSize];
        AVB_ASSERT(dummyData != NULL);

        /*
         * dumpFromLocalAudioBuffer() should not be used to discard samples here
         * because we need to discard corresponding descriptors as well.
         */
        uint64_t timeStamp = 0u;
        (void) readLocalAudioBuffer(uint16_t(channelIdx), dummyData, bufferSize, samplesRead, timeStamp);
      }
      else // fillLevel < mOptimalFillLevel
      {
        /*
         * The time-aware buffer accumulates samples up to half-full before allowing initial read access.
         * The resetBuffers() is typically called when the local stream becomes active. If we reset the buffer
         * with mOptimalFillLevel, since it is typically half-full of the total buffer size, it would immediately
         * become readable. To avoid the unexpected buffer filling we do not fill the buffer here.
         *
         * Note: if the buffer is underrun the AVB tx stream will make a dummy packet. And the ALSA interface
         * freewheels. We don't need to fill the buffer with dummy samples here.
         */
      }

      if (NULL != dummyData)
      {
        delete[] dummyData;
        dummyData = NULL;
      }
    }
    else
    {
      buffer->reset(mOptimalFillLevel);
    }
  }

  if (true == hasBufferDesc())
  {
    descQ->unlock();
  }

  return error;
}


void IasAlsaVirtualDeviceStream::updateBufferStatus()
{
  IasLocalAudioStreamClientInterface * const client = getClient();
  if (NULL != client)
  {
    AVB_ASSERT(mChannelBuffers.size() > 0u);
    IasLocalAudioBuffer * buf = mChannelBuffers[0];
    AVB_ASSERT(NULL != buf);

    int32_t fill = buf->getRelativeFillLevel();

    client->updateRelativeFillLevel(fill);

    if (eIasActive == getClientState())
    {
      IasLocalAudioStreamClientInterface::DiscontinuityEvent event = IasLocalAudioStreamClientInterface::eIasUnspecific;

      if (buf->getFillLevel() == buf->getTotalSize())
      {
        event = IasLocalAudioStreamClientInterface::eIasOverrun;
      }
      else if (0 == buf->getFillLevel())
      {
        event = IasLocalAudioStreamClientInterface::eIasUnderrun;
      }
      else
      {
        // do nothing
      }

      if ((IasLocalAudioStreamClientInterface::eIasUnspecific != event) && client->signalDiscontinuity(event, 0u))
      {
        resetBuffers();
        getDiagnostics()->setResetBuffersCount(getDiagnostics()->getResetBuffersCount() + 1);
      }
    }
  }
}


/**
 *  Cleanup method.
 */
void IasAlsaVirtualDeviceStream::cleanup()
{
   DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

   // shut down IPC
   if (NULL != mShm)
   {
     mShm->abortTransmission();
     delete mShm;
     mShm = NULL;
   }
   mSampleFrequency  = 0u;
   mPeriodSize       = 0u;
   mOptimalFillLevel = 0u;
}


IasAvbProcessingResult IasAlsaVirtualDeviceStream::writeLocalAudioBuffer(uint16_t channelIdx,
                                                                  IasLocalAudioBuffer::AudioData *buffer,
                                                                  uint32_t bufferSize,
                                                                  uint16_t &samplesWritten,
                                                                  uint32_t timestamp)
{
  return IasLocalAudioStream::writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timestamp);
}







} // namespace IasMediaTransportAvb
