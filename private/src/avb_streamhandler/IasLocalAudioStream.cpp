/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalAudioStream.cpp
 * @brief   Implementation of a local audio buffer
 * @details
 *
 * @date    2013
 */

#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasLocalAudioBuffer.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include <algorithm>
#include <cmath>
#include <dlt_cpp_extension.hpp>

using std::min;

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLocalAudioStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasLocalAudioStream::IasLocalAudioStream(DltContext &dltContext, IasAvbStreamDirection direction,
    IasLocalStreamType type, uint16_t streamId):
    mLog(&dltContext),
    mDirection(direction),
    mType(type),
    mStreamId(streamId),
    mChannelLayout(0),
    mNumChannels(0),
    mSampleFrequency(0),
    mHasSideChannel(false),
    mClientState(eIasNotConnected),
    mClient(NULL),
    mBufferDescQ(NULL),
    mDescMode(AudioBufferDescMode::eIasAudioBufferDescModeOff),
    mPtpProxy(NULL),
    mEpoch(0u),
    mLastTimeStamp(0u),
    mLastSampleCnt(0u),
    mLaunchTimeDelay(0u),
    mAudioRxDelay(0u),
    mPeriodSz(0u),
    mWorkerRunning(true),
    mPendingSamples(0),
    mNullData(NULL),
    mPendingDesc(),
    mDiag(),
    mAlsaRxSyncStart(false)
{
  // do nothing
}


/*
 *  Destructor.
 */
IasLocalAudioStream::~IasLocalAudioStream()
{
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalAudioStream::init(uint8_t channelLayout, uint16_t numChannels, bool hasSideChannel,
                                                 uint32_t totalSize, uint32_t sampleFrequency, uint32_t alsaPeriodSize)
{
  IasAvbProcessingResult error = eIasAvbProcOK;
  bool doAnalysis = true;

  if (isInitialized())
  {
    error = eIasAvbProcInitializationFailed;
  }

  if (    ( 0 == sampleFrequency)
       || ( 0 == numChannels)
       || ((1 == numChannels) && hasSideChannel)
     )
  {
    error = eIasAvbProcInvalidParam;
  }

  if (eIasAvbProcOK == error)
  {
    mNumChannels     = numChannels;
    mChannelLayout   = channelLayout;
    mHasSideChannel  = hasSideChannel;
    mSampleFrequency = sampleFrequency;

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " mNumChannels=", mNumChannels,
        ", channelLayout=", mChannelLayout);

    // totalSize == 0 means that the derived class uses something else instead of the ring buffers
    if (totalSize > 0)
    {
      mDiag.setTotalBufferSize(totalSize);
      uint32_t readThreshold = 0u;
      uint32_t baseFillMultiplier = 15u; // the value is 0.1 step so 15 means actually 1.5

      // Enable the time-awareness buffering for ALSA interface if required
      if (eIasAlsaStream == mType)
      {
        IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioTstampBuffer, mDescMode);

        if (hasBufferDesc())
        {
          mPeriodSz = alsaPeriodSize;

          uint32_t baseFreq   = 48000u;
          uint32_t basePeriod = 128u;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBaseFreq, baseFreq);
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBasePeriod, basePeriod);

          const uint64_t p = uint64_t(alsaPeriodSize * uint64_t(baseFreq));
          const uint64_t q = uint64_t(basePeriod * sampleFrequency);

          if (0u == p % q)
          {
            const uint32_t cycle = uint32_t(p / q);

            if (0u != cycle)
            {
              mPeriodSz = mPeriodSz / cycle;
            }
          }

          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioBaseFillMultiplier, baseFillMultiplier);
          AVB_ASSERT(0u != baseFillMultiplier);
          mDiag.setBaseFillMultiplier(baseFillMultiplier);

          /*
           * Tx latency must be a multiple of mPeriodSz because AvbAlsaWrk writes samples at mPeriodSz unit.
           * For example suppose basePeriod = 128 and baseFillMultiplier = 15 (= 1.5), we expect AvbTxWrk
           * starts grabbing samples once the ring buffer's fill level reached at 192 but actually it can
           * start only after 256 (=128 * 2) samples were written to the ring buffer because AvbAlsaWrk writes
           * samples to the buffer at mPeriodSz unit.
           */
          const uint32_t readThresholdRx = baseFillMultiplier * mPeriodSz / 10u;
          uint32_t readThresholdTx = uint32_t(::ceil((double)readThresholdRx / (double)mPeriodSz)) * mPeriodSz;

          uint32_t baseFillMultiplierTx = 0u;
          if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioBaseFillMultiplierTx, baseFillMultiplierTx))
          {
            // overwritten
            readThresholdTx = uint32_t(::ceil((double)baseFillMultiplierTx / 10.0f)) * mPeriodSz;
            mDiag.setBaseFillMultiplierTx(baseFillMultiplierTx);
          }

          readThreshold = (IasAvbStreamDirection::eIasAvbTransmitToNetwork == mDirection) ?
                                                       (readThresholdTx) : (readThresholdRx);

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local stream =", getStreamId(),
                      (IasAvbStreamDirection::eIasAvbTransmitToNetwork == mDirection) ? "(tx)" : "(rx)",
                          "bufReadStartThreshold =", readThreshold,
                          "Latency =", uint64_t(readThreshold * 1e9 / mSampleFrequency), "ns",
                          "baseFillMultiplier =", double(readThreshold) / (double)mPeriodSz);

          // time required to fill buffer at readable threshold (default at half-hull)
          const uint32_t readThresholdDelayRx = uint32_t(double(readThresholdRx) / double(mSampleFrequency) * 1e9);
          const uint32_t readThresholdDelayTx = uint32_t(double(readThresholdTx) / double(mSampleFrequency) * 1e9);

          mLaunchTimeDelay = readThresholdDelayTx;

          /*
           * latency which could happen on the listener side to write received AVBTP packet into
           * a buffer containing data samples. It will be added to the presentation time which is encapsulated
           * in xmit AVBTP audio packets.
           */
          uint32_t cycleWait = 2000000u; // ns
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxCycleWait, cycleWait);
          mDiag.setCycleWait(cycleWait);

          mAudioRxDelay = cycleWait + readThresholdDelayRx;

          // Determine the required FIFO size
          const uint32_t descQsz = uint32_t((totalSize + (mPeriodSz - 1u)) / mPeriodSz);

          // Create a buffer descriptor fifo (additional one descriptor just in case for buffer overrun)
          mBufferDescQ = new (nothrow) IasLocalAudioBufferDesc(descQsz + 1u);
          if (NULL != mBufferDescQ)
          {
            if (getDirection() == IasAvbStreamDirection::eIasAvbReceiveFromNetwork)
            {
              mAlsaRxSyncStart = mBufferDescQ->getAlsaRxSyncStartMode();
              if (mAlsaRxSyncStart)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local stream =", getStreamId(), ": alsa.sync.rx.read.start on");
              }
            }

            mNullData = new (nothrow) IasLocalAudioBuffer::AudioData[mPeriodSz];
            if (NULL != mNullData)
            {
              (void) memset(mNullData, 0, sizeof (IasLocalAudioBuffer::AudioData) * mPeriodSz);

              mPtpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
              if (NULL != mPtpProxy)
              {
                mEpoch = mPtpProxy->getEpochCounter();
              }
              else
              {
                error = eIasAvbProcInitializationFailed;
              }
            }
            else
            {
              error = eIasAvbProcNotEnoughMemory;
            }
          }
          else
          {
            error = eIasAvbProcNotEnoughMemory;
          }
        }
      }

      for(uint16_t i = 0; i < mNumChannels; i++)
      {
        IasLocalAudioBuffer *newLocalAudioBuffer = new (nothrow) IasLocalAudioBuffer();

        if (NULL != newLocalAudioBuffer)
        {
          /*
           * The additional one sample is needed to prevent sample dropping at buffer full.
           * The local audio buffer preserves one sample area from being used to avoid read & write
           * indexes overlap. Because if readIndex became equal with writeIndex getFillLevel() cannot
           * determine if buffer is full or empty. However due to that the buffer allows the local
           * audio stream use up to 'totalSize - 1' samples at a time. If totalSize is small enough
           * and buffer's utilization efficiency is very high, the fill level could be close to full.
           * To prevent one sample dropping in such a situation the additional one sample area is
           * needed for the buffer.
           */
          if ((error = newLocalAudioBuffer->init(totalSize + 1u, doAnalysis)) == eIasAvbProcOK)
          {
            mChannelBuffers.push_back(newLocalAudioBuffer);
            doAnalysis = false;
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Create Ringbuffer for channel:",
                i);

            error = newLocalAudioBuffer->setReadThreshold(readThreshold);
            mDiag.setBufferReadThreshold(readThreshold);
            if (error != eIasAvbProcOK)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " setReadThreshold failed, threshold =", readThreshold,
                  "adjust time-aware params peridSz/baseFillMultiplier/bufSz =", mPeriodSz, "/", baseFillMultiplier,
                  "/", (newLocalAudioBuffer->getTotalSize() - 1u));

              /* newLocalAudioBuffer will be deleted by cleanup() since it's been pushed to mChannelBuffers */
            }
          }
          else
          {
            /**
             * @log Failed to initialize the local audio buffer.
             */

            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, " newLocalAudioBuffer->init fails, mNumChannels:",
                mNumChannels);
            delete newLocalAudioBuffer;
          }
        }
        else
        {
          error = eIasAvbProcNotEnoughMemory;
        }

        if(eIasAvbProcOK != error)
        {
          break;
        }
      }
    }

    if(eIasAvbProcOK != error)
    {
      cleanup();
    }
  }

  return error;
}


IasAvbProcessingResult IasLocalAudioStream::writeLocalAudioBuffer(uint16_t channelIdx,
                                                                  IasLocalAudioBuffer::AudioData *buffer,
                                                                  uint32_t bufferSize,
                                                                  uint16_t &samplesWritten,
                                                                  uint32_t timestamp)
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  IasLocalAudioBufferDesc *descQ = mBufferDescQ;

  if (!isInitialized())
  {
    error = eIasAvbProcNotInitialized;
  }
  else if ((channelIdx >= mNumChannels) || (NULL == buffer) || (0u == bufferSize))
  {
    error = eIasAvbProcInvalidParam;
  }
  else if (mWorkerRunning)
  {
    /*
     * Produce samples to local audio buffer only when the work thread is running,
     * otherwise unexpected buffer overrun would happen.
     */

    IasLocalAudioBuffer *ringBuf = getChannelBuffers()[channelIdx];
    AVB_ASSERT( getChannelBuffers().size() == mNumChannels );
    AVB_ASSERT(NULL != ringBuf);

    bool useDesc = hasBufferDesc();

    if (useDesc)
    {
      if (0 == channelIdx)
      {
        /*
         * If no space is available in the buffer of channel_0 we should reset the whole buffers
         * before calling the write method, otherwise sample count could be differ for each channel.
         *
         * e.g. writeLocalAudioBuffer() is called against channel_0, the write method fails due to
         * buffer overflow, resetBuffers() resets whole channels, next time writeLocalAudioBuffer()
         * will be called against channel_1, the write method will be successful since the buffer
         * has been reset, as a result sample count on channel_0 does not consist with the one on
         * channel_1.
         *
         * To avoid such inconsistency we should reset the buffers before calling the write method,
         * when buffer overflow is predicted.
         */
        const uint16_t remaining = uint16_t( ringBuf->getTotalSize() - ringBuf->getFillLevel() - 1u );
        if((bufferSize > remaining) && (NULL != mClient) && (eIasActive == mClientState))
        {
          DLT_LOG(*mLog,  DLT_LOG_WARN, DLT_STRING("[IasLocalAudioStream::writeLocalAudioBuffer]"),
              DLT_STRING("buffer overrun happened"), DLT_UINT32(bufferSize - remaining));

          if (mClient->signalDiscontinuity(IasLocalAudioStreamClientInterface::eIasOverrun, bufferSize - remaining))
          {
            resetBuffers();
            mDiag.setResetBuffersCount(mDiag.getResetBuffersCount() + 1);
          }
        }

        if (descQ->getResetRequest())
        {
          mEpoch          = 0u;
          mLastTimeStamp  = 0u;
          mLastSampleCnt  = 0u;
          mPendingSamples = 0u;
          (void) memset(&mPendingDesc, 0, sizeof (mPendingDesc));
        }
      }
    }

    samplesWritten = uint16_t( ringBuf->write(buffer, bufferSize) );

    if((bufferSize != samplesWritten) && (NULL != mClient) && (eIasActive == mClientState))
    {
      if (mClient->signalDiscontinuity(IasLocalAudioStreamClientInterface::eIasOverrun, bufferSize - samplesWritten))
      {
        resetBuffers();
        mDiag.setResetBuffersCount(mDiag.getResetBuffersCount() + 1);
      }
    }
    else if (useDesc)
    {
      if ((0 != samplesWritten) && ((getNumChannels() - 1) == channelIdx))
      {
        if (0u == mPendingDesc.timeStamp)
        {
          uint64_t prev = mLastTimeStamp;
          (void) updateRxTimestamp(timestamp);  // update mLastTimeStamp
          uint64_t now = mLastTimeStamp;

          if (0u == mPendingDesc.sampleCnt)
          {
            // start recording samples on a new descriptor
            mPendingDesc.timeStamp = mLastTimeStamp + getAudioRxDelay();
            mPendingDesc.bufIndex  = ringBuf->getMonotonicWriteIndex() - samplesWritten;
            mPendingDesc.sampleCnt = samplesWritten;
          }
          else
          {
            /*
             * interpolate samples to the existing pending descriptor
             * timestamp is not recorded yet, do the linear interpolation
             */
            uint64_t  timeOffset = 0u;

            if ((now > prev) && (mLastSampleCnt > mPendingDesc.sampleCnt))
            {
              /*
               * the number of samples belonged to the previous timestamp fifo
               * i.e. sampleCnt = 'previous bufferSize' - 'overflow'
               */
              uint32_t sampleCnt = mLastSampleCnt - mPendingDesc.sampleCnt;

              // ((TSy - TSx) / (CNTy - CNTx)) * sampleCnt
              double timePerSample = double(now - prev) / double(mLastSampleCnt);
              timeOffset = uint64_t(timePerSample * double(sampleCnt));
            }
            else
            {
              // for debugging purposes although it's unlikely to happen
              DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "rx timestamp interpolation fail",
                  "now/prev/last-samples/remainings", now, "/", prev, "/", mLastSampleCnt, "/",
                  mPendingDesc.sampleCnt);
            }

            mPendingDesc.timeStamp  = prev + timeOffset + getAudioRxDelay();
            mPendingDesc.sampleCnt += samplesWritten;
          }
        }
        else
        {
          // interpolate samples to the existing pending descriptor
          mPendingDesc.sampleCnt += samplesWritten;
        }

        mPendingSamples += bufferSize;
        if (mPendingSamples >= mPeriodSz)
        {
          /*
           * Now we should pass samples to AvbAlsaWrk since we have received the samples of the
           * alsa period size from AVB stream.
           */
          uint32_t overflow = mPendingSamples - mPeriodSz;
          mPendingDesc.sampleCnt = mPeriodSz;

          // enqueue the timestamp to fifo so that AvbAlsaWrk can read samples
          descQ->enqueue(mPendingDesc);

          // reset the counters to store next samples into a new descriptor
          (void) memset(&mPendingDesc, 0, sizeof (mPendingDesc));
          mPendingSamples = 0u;
          mLastSampleCnt  = 0u;

          // store the overflowed samples in the next descriptor
          if (0u != overflow)
          {
            // this case can happen when alsaBasePeriodSz mod numSamplesPerChannelPerPkt != 0
            // e.g. numSamplesPerChannelPerPkt = 6 (classA) and alsaBasePeriodSz = 128

            mPendingSamples = overflow;
            mLastSampleCnt  = bufferSize;

            // update mLastTimeStamp
            (void) updateRxTimestamp(timestamp);

            mPendingDesc.timeStamp = 0u; // to be updated later on receiving next data
            mPendingDesc.bufIndex  = ringBuf->getMonotonicWriteIndex() - overflow;
            mPendingDesc.sampleCnt = overflow;
          }
        }
        else
        {
          // nop: need further samples to be interpolated
        }
      }
    }
  }

  if (eIasAvbProcOK != error)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error=", int32_t(error));
  }

  return error;
}


IasAvbProcessingResult IasLocalAudioStream::readLocalAudioBuffer(uint16_t channelIdx,
                                                                 IasLocalAudioBuffer::AudioData *buffer,
                                                                 uint32_t bufferSize,
                                                                 uint16_t &samplesRead,
                                                                 uint64_t &timeStamp)
{

  IasAvbProcessingResult error = eIasAvbProcOK;

  if (!isInitialized())
  {
    error = eIasAvbProcNotInitialized;
  }
  else if ((channelIdx >= mNumChannels) || (NULL == buffer) || (0u == bufferSize))
  {
    error = eIasAvbProcInvalidParam;
  }
  else
  {
    AVB_ASSERT(mChannelBuffers.size() > channelIdx);
    AVB_ASSERT(NULL != getChannelBuffers()[channelIdx]);

    bool isReadReady = true;
    bool useDesc = hasBufferDesc();
    IasLocalAudioBufferDesc *descQ = mBufferDescQ;

    if (useDesc)
    {
      AVB_ASSERT( NULL != descQ );
      // allow initial read access once the fill level reached at half full
      isReadReady = getChannelBuffers()[channelIdx]->isReadReady();
    }

    if (isReadReady)
    {
      if (useDesc)
      {
        IasLocalAudioBufferDesc::AudioBufferDesc desc;

        // read a descriptor w/o dequeuing
        if (eIasAvbProcOK == descQ->peek(desc))
        {
          /*
           * Because the timestamp contained in the descriptor might be needed later again
           * if 'bufferSize' is smaller than 'sampleCnt' belonging to the descriptor,
           * first we need to read it w/o dequeuing.
           */
          IasLocalAudioBuffer *ringBuf = getChannelBuffers()[channelIdx];
          AVB_ASSERT( NULL != ringBuf );

          uint64_t readIndex = ringBuf->getMonotonicReadIndex();

          samplesRead = uint16_t( ringBuf->read(buffer, bufferSize) );

          if ((desc.bufIndex <= readIndex) && (readIndex < desc.bufIndex + desc.sampleCnt))
          {
            // valid descriptor since current readIndex is within its range
            timeStamp = desc.timeStamp + mLaunchTimeDelay;

            if (readIndex != desc.bufIndex)
            {
              // adjust timestamp
              uint64_t tstampOffset = 0u;

              // we have already sent the number of samples below
              uint64_t samplesSent = readIndex - desc.bufIndex;

              IasLocalAudioBufferDesc::AudioBufferDesc descY;
              IasAvbProcessingResult ret;

              ret = descQ->peekX(descY, 1u);

              if ((eIasAvbProcOK == ret) &&
                  (descY.timeStamp > desc.timeStamp) && (descY.bufIndex > desc.bufIndex))
              {
                // ((TSy - TSx) / (CNTy - CNTx)) * sampleSent
                double timePerSample = double(descY.timeStamp - desc.timeStamp) /
                                              double(descY.bufIndex - desc.bufIndex);

                tstampOffset = uint64_t(timePerSample * double(samplesSent));
              }
              else
              {
                /*
                 * Estimate the timestamp value based on the sample frequency.
                 *
                 * Since FIFO holds only one descriptor at the moment, unable to use TSy to
                 * interpolate the timestamp value. This might happen typically at the startup
                 * time when AVB stream starts grabbing samples from the Local Audio buffer.
                 * The TX engine might make bursty reading and the buffer could be close to empty
                 * if the buffer size is short.
                 */
                tstampOffset = uint64_t(double((samplesSent)) / double(mSampleFrequency) * 1e9);
              }

              timeStamp = timeStamp + tstampOffset;
            }

            // delete used descriptors when the samples of the last channel were grabbed
            if (channelIdx == (mNumChannels - 1))
            {
              // delete used descriptors
              uint64_t curReadIndex = ringBuf->getMonotonicReadIndex();
              do
              {
                if ((desc.bufIndex + desc.sampleCnt) <= curReadIndex)
                {
                  // delete the descriptor because all of its samples were read
                  descQ->dequeue(desc);
                }
                else
                {
                  // nop: the descriptor must remain in fifo since it still has remaining samples
                  break;
                }
                // read the next descriptor
              } while (eIasAvbProcOK == descQ->peek(desc));
            }
          }
          else
          {
            /*
             * invalid descriptor
             *
             * Have we lost timestamp due to buffer overrun? No theoretically it will not happen.
             * Because IasLocalAudioBuffer::write() never overwrite existing data which is not yet read,
             * in ring buffer. If there is no space in buffer at all, the write() method will return zero.
             * Either IasLocalAudioStream::writeLocalAudioBuffer or IasAvbAudioShmProvider::copyJob
             * does not enqueue a descriptor to fifo in such a case. Thus theoretically there is no
             * possibility we come across an out-of-date descriptor.
             */
            if (channelIdx == (mNumChannels - 1))
            {
              DLT_LOG(*mLog,  DLT_LOG_ERROR, DLT_STRING("[IasLocalAudioStream::readLocalAudioBuffer]"),
                      DLT_STRING("detected invalid timestamp"), DLT_STRING("bufIndex="), DLT_UINT64(desc.bufIndex),
                      DLT_STRING("sampleCnt="), DLT_UINT64(desc.sampleCnt), DLT_STRING("readIndex="), DLT_UINT64(readIndex));

              descQ->dequeue(desc);
            }
          }
        }
      }
      else
      {
        timeStamp = 0u;
        samplesRead = uint16_t( getChannelBuffers()[channelIdx]->read(buffer, bufferSize) );
      }

      if((0 == samplesRead) && (NULL != mClient) && (eIasActive == mClientState))
      {
        if (mClient->signalDiscontinuity(IasLocalAudioStreamClientInterface::eIasUnderrun, bufferSize - samplesRead))
        {
          resetBuffers();
          mDiag.setResetBuffersCount(mDiag.getResetBuffersCount() + 1);
        }
      }
    }
    else // !isReadReady
    {
      samplesRead = 0u;
    }
  }

  return error;
}


IasAvbProcessingResult IasLocalAudioStream::dumpFromLocalAudioBuffer(uint16_t &numSamples)
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  if (!isInitialized() || mChannelBuffers.empty())
  {
    error = eIasAvbProcNotInitialized;
  }
  else
  {
    uint32_t fill = getChannelBuffers()[0]->getFillLevel();

    for (uint32_t channelIdx = 1u; channelIdx < mNumChannels; channelIdx++)
    {
      fill = std::min(fill, getChannelBuffers()[channelIdx]->getFillLevel());
    }

    if (fill < numSamples)
    {
      numSamples = uint16_t(fill);
    }

    IasLocalAudioBuffer::AudioData dummy[numSamples];

    lock();
    for (uint32_t channelIdx = 0u; channelIdx < mNumChannels; channelIdx++)
    {
      uint16_t read      = 0u;
      uint64_t timeStamp = 0u;
      (void) readLocalAudioBuffer(uint16_t(channelIdx), dummy, uint32_t(numSamples), read, timeStamp);

      AVB_ASSERT(read == numSamples);
      (void) read;
    }
    unlock();
  }

  return error;
}


/*
 *  Cleanup method.
 */
void IasLocalAudioStream::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  for(uint32_t channelIdx=0; channelIdx< mChannelBuffers.size(); channelIdx++)
  {
    delete getChannelBuffers()[channelIdx];
  }

  if (NULL != mBufferDescQ)
  {
    delete mBufferDescQ;
    mBufferDescQ = NULL;
  }

  if (NULL != mNullData)
  {
    delete[] mNullData;
    mNullData = NULL;
  }

  mChannelBuffers.clear();
  mNumChannels     = 0;
  mSampleFrequency = 0;
}

IasAvbProcessingResult IasLocalAudioStream::connect( IasLocalAudioStreamClientInterface * client )
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (NULL == client)
  {
    ret = eIasAvbProcInvalidParam;
  }
  else if (NULL != mClient)
  {
    ret = eIasAvbProcAlreadyInUse;
  }
  else
  {
    mClient = client;
    mClientState = eIasIdle;

    if (hasBufferDesc())
    {
      uint32_t minBufSz   = 0u;
      uint32_t minRxBufSz = 0u;
      uint32_t minTxBufSz = 0u;

      IasLocalAudioBuffer *ringBuf = mChannelBuffers[0];
      AVB_ASSERT(NULL != ringBuf);

      const uint32_t maxTransmitTime = client->getMaxTransmitTime();
      const uint32_t presentationTimeOffset = maxTransmitTime + getAudioRxDelay();

      minRxBufSz = uint32_t((double)presentationTimeOffset * (double)mSampleFrequency / 1e9);

      if (IasAvbStreamDirection::eIasAvbTransmitToNetwork == mDirection)
      {
        const uint32_t periodCycle = uint32_t (double(mPeriodSz) / double(mSampleFrequency) * 1e9);

        // get the threshold which can avoid buffer underrun
        minTxBufSz = client->getMinTransmitBufferSize(periodCycle);
        minTxBufSz = uint32_t(::ceil((double)(minTxBufSz) / (double)(mPeriodSz))) * mPeriodSz;

        const uint32_t readThresholdDelayTx = uint32_t(double(minTxBufSz) / double(mSampleFrequency) * 1e9);

        if (ringBuf->getReadThreshold() < minTxBufSz)
        {
          lock();

          // reset buffers (required when AVB and Local streams are re-connected on the fly)
          for (uint32_t i = 0; i < mNumChannels; i++)
          {
            ringBuf = mChannelBuffers[i];
            AVB_ASSERT(NULL != ringBuf);
            ringBuf->setReadThreshold(minTxBufSz);
            ringBuf->reset(0u);
            mDiag.setBufferReadThreshold(minTxBufSz);
          }

          mBufferDescQ->reset();

          // update the read threshold
          mLaunchTimeDelay = readThresholdDelayTx;

          unlock();

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local stream =", getStreamId(), "(tx)",
                          "bufReadStartThreshold =", minTxBufSz,
                          "Latency =", readThresholdDelayTx, "ns",
                          "baseFillMultiplier =", double(minTxBufSz) / (double)mPeriodSz,
                          "overwritten by AVB stream");
        }

        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " TxBufLatency =", mLaunchTimeDelay, "ns");
      }

      minBufSz = std::max(minTxBufSz, minRxBufSz);
      minBufSz = uint32_t((::ceil((double)(minBufSz) / (double)(mPeriodSz)) + 1u) * mPeriodSz);

      const uint32_t ringBufSz = ringBuf->getTotalSize() - 1u;
      if (ringBufSz < minBufSz)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Increase local buffer size (cAlsaRingBufferSz) to",
            minBufSz, "( currently", ringBufSz, ")");
      }
    }
  }

  return ret;
}

IasAvbProcessingResult IasLocalAudioStream::disconnect()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  mClientState = eIasNotConnected;
  mClient = NULL;

  return ret;
}

void IasLocalAudioStream::setClientActive( bool active )
{
  if (NULL != mClient)
  {
    if (mAlsaRxSyncStart) // if -k alsa.sync.rx.read.start=1
    {
      /*
       * double-check the runtime value of alsaRxSyncStart because ALSA worker may forcibly disable the feature
       * when unexpected error happened and activated the fallback mode.
       */
      if (hasBufferDesc() && getDirection() == IasAvbStreamDirection::eIasAvbReceiveFromNetwork) // RX stream
      {
        // locking is a bit costly but should be affordable since setClientActive is not frequently called
        lock();
        mAlsaRxSyncStart = getBufferDescQ()->getAlsaRxSyncStartMode();
        unlock();
      }
    }

    if (active)
    {
      if(eIasActive != mClientState)
      {
        mClientState = eIasActive;
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "=true, resetBuffers");

        if (mAlsaRxSyncStart)  // if -k alsa.sync.rx.read.start=1
        {
          /*
           * The resetBuffers() call at here may change buffer fill level. If ALSA worker has already
           * started reading samples, changing buffer fill level from outside of ALSA worker affects
           * calculation of RX path latency. The fix of 'alsaRxSyncStart = true' can be disabled
           * by the registry key for GP releases to avoid causing any regression for existing users.
           */
        }
        else
        {
          (void) resetBuffers();
        }

        // initialize the counters to be used to expand received timestamp to 64 bit
        mLastTimeStamp = mEpoch = 0u;

        if (hasBufferDesc() &&
              getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork)
        {
          lock();

          // flush all data samples and clear the read threshold flag
          for (uint32_t i = 0; i < getNumChannels(); i++)
          {
            IasLocalAudioBuffer *ringBuf = getChannelBuffers()[i];
            AVB_ASSERT(NULL != ringBuf);
            ringBuf->reset(0u);
          }
          getBufferDescQ()->reset();

          unlock();
        }
      }
    }
    else
    {
      mClientState = eIasIdle;

      if (mAlsaRxSyncStart) // if -k alsa.sync.rx.read.start=1
      {
        /* DisconnectStreams: Flush out all samples, otherwise ALSA might pull those samples when
         * streams are reconnected. ALSA stops pulling samples when streams are disconnected,
         * due to that old samples could stay in the local audio buffer which might be read by ALSA
         * when streams are reconnected next time.
         */

        lock();

        // flush all data samples and clear the read threshold flag
        for (uint32_t i = 0; i < getNumChannels(); i++)
        {
          IasLocalAudioBuffer *ringBuf = getChannelBuffers()[i];
          AVB_ASSERT(NULL != ringBuf);
          ringBuf->reset(0u);
        }
        getBufferDescQ()->reset();

        unlock();
      }
    }
  }
}

void IasLocalAudioStream::setWorkerActive(bool active)
{
  if (hasBufferDesc())
  {
    lock();

    mWorkerRunning = active;
    if (false == mWorkerRunning)
    {
      if (mDirection == IasAvbStreamDirection::eIasAvbReceiveFromNetwork)
      {
        /*
         * flush out samples and descriptors when the worker stopped (suspend/shutdown)
         * otherwise overrun might happen and output unnecessary warning messages when
         * going into the suspend or the shutdown state. In addition if we left samples
         * old samples might be reproduced at the resume timing.
         */
        for (uint32_t i = 0; i < mNumChannels; i++)
        {
          IasLocalAudioBuffer *ringBuf = mChannelBuffers[i];
          AVB_ASSERT(NULL != ringBuf);
          ringBuf->reset(0u);
        }
        mBufferDescQ->reset();
      }
    }

    unlock();
  }
}

void IasLocalAudioStream::updateRxTimestamp(const uint32_t timestamp)
{
  // expand 32-bit time stamp into 64 bits
  uint32_t epoch = mPtpProxy->getEpochCounter();

  // derived from IasAvbRxStreamClockDomain::update
  if ((0u == mLastTimeStamp) || (epoch != mEpoch))
  {
    // initialize
    mEpoch = epoch;

    const uint64_t now = mPtpProxy->getLocalTime();

    mLastTimeStamp = (now & uint64_t(0xFFFFFFFF00000000)) + timestamp;

    if ((int32_t(timestamp - uint32_t(now)) > 0) && (timestamp < uint32_t(now)))
    {
      mLastTimeStamp += uint64_t(0x100000000);
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " detected wrap");
    }
  }
  else
  {
    // update
    if (uint32_t(mLastTimeStamp) < timestamp)
    {
      mLastTimeStamp = (mLastTimeStamp & uint64_t(0xFFFFFFFF00000000)) + timestamp;
    }
    else
    {
      mLastTimeStamp = ((mLastTimeStamp + uint64_t(0x100000000)) & uint64_t(0xFFFFFFFF00000000)) + timestamp;
    }
  }
}

uint64_t IasLocalAudioStream::getCurrentTimestamp()
{
  uint64_t timeStamp = 0u;

  if (hasBufferDesc())
  {
    lock();

    IasLocalAudioBufferDesc *descQ = mBufferDescQ;
    IasLocalAudioBufferDesc::AudioBufferDesc desc;
    if (eIasAvbProcOK == descQ->peek(desc))
    {
      timeStamp = desc.timeStamp + mLaunchTimeDelay;

      IasLocalAudioBuffer *ringBuf = getChannelBuffers()[0u];
      uint64_t readIndex = ringBuf->getMonotonicReadIndex();
      if (readIndex != desc.bufIndex)
      {
        // adjust timestamp
        uint64_t tstampOffset = 0u;

        // the number of samples which have been grabbed out
        uint64_t samplesSent = readIndex - desc.bufIndex;

        IasLocalAudioBufferDesc::AudioBufferDesc descY;

        if ((eIasAvbProcOK == descQ->peekX(descY, 1u)) &&
            (descY.timeStamp > desc.timeStamp) && (descY.bufIndex > desc.bufIndex))
        {
          // ((TSy - TSx) / (CNTy - CNTx)) * sampleSent
          double timePerSample = double(descY.timeStamp - desc.timeStamp) /
                                        double(descY.bufIndex - desc.bufIndex);

          tstampOffset = uint64_t(timePerSample * double(samplesSent));
        }
        else
        {
          /*
           * Estimate the timestamp value based on the sample frequency.
           */
          tstampOffset = uint64_t(double((samplesSent)) / double(mSampleFrequency) * 1e9);
        }

        timeStamp = timeStamp + tstampOffset;
      }
    }
    unlock();
  }

  return timeStamp;
}


IasLocalAudioStreamAttributes::IasLocalAudioStreamAttributes()
  : direction(eIasAvbTransmitToNetwork)
  , numChannels(0u)
  , sampleFrequency(0u)
  , format(eIasAvbAudioFormatIec61883)
  , periodSize(0u)
  , numPeriods(0u)
  , channelLayout(0u)
  , hasSideChannel(false)
  , deviceName(std::string(""))
  , streamId(0u)
  , connected(false)
  , streamDiagnostics()
{
}

IasLocalAudioStreamAttributes::IasLocalAudioStreamAttributes(const IasLocalAudioStreamAttributes &iOther)
  : direction(iOther.direction)
  , numChannels(iOther.numChannels)
  , sampleFrequency(iOther.sampleFrequency)
  , format(iOther.format)
  , periodSize(iOther.periodSize)
  , numPeriods(iOther.numPeriods)
  , channelLayout(iOther.channelLayout)
  , hasSideChannel(iOther.hasSideChannel)
  , deviceName(iOther.deviceName)
  , streamId(iOther.streamId)
  , connected(iOther.connected)
  , streamDiagnostics(iOther.streamDiagnostics)
{
}

IasLocalAudioStreamAttributes::IasLocalAudioStreamAttributes(
    IasAvbStreamDirection iDirection,
    uint16_t iNumChannels,
    uint32_t iSampleFrequency,
    IasAvbAudioFormat iFormat,
    uint32_t iPeriodSize,
    uint32_t iNumPeriods,
    uint8_t iChannelLayout,
    bool iHasSideChannel,
    std::string iDeviceName,
    uint16_t iStreamId,
    bool iConnected,
    IasLocalAudioStreamDiagnostics iStreamDiagnostics)
  : direction(iDirection)
  , numChannels(iNumChannels)
  , sampleFrequency(iSampleFrequency)
  , format(iFormat)
  , periodSize(iPeriodSize)
  , numPeriods(iNumPeriods)
  , channelLayout(iChannelLayout)
  , hasSideChannel(iHasSideChannel)
  , deviceName(iDeviceName)
  , streamId(iStreamId)
  , connected(iConnected)
  , streamDiagnostics(iStreamDiagnostics)
{
}

IasLocalAudioStreamAttributes &IasLocalAudioStreamAttributes::operator =(const IasLocalAudioStreamAttributes &iOther)
{
  if (this != &iOther)
  {
    direction = iOther.direction;
    numChannels = iOther.numChannels;
    sampleFrequency = iOther.sampleFrequency;
    format = iOther.format;
    periodSize = iOther.periodSize;
    numPeriods = iOther.numPeriods;
    channelLayout = iOther.channelLayout;
    hasSideChannel = iOther.hasSideChannel;
    deviceName = iOther.deviceName;
    streamId = iOther.streamId;
    connected = iOther.connected;
    streamDiagnostics = iOther.streamDiagnostics;
  }

  return *this;
}

bool IasLocalAudioStreamAttributes::operator ==(const IasLocalAudioStreamAttributes &iOther) const
{
  return (this == &iOther)
      || (   (direction == iOther.direction)
          && (numChannels == iOther.numChannels)
          && (sampleFrequency == iOther.sampleFrequency)
          && (format == iOther.format)
          && (periodSize == iOther.periodSize)
          && (numPeriods == iOther.numPeriods)
          && (channelLayout == iOther.channelLayout)
          && (hasSideChannel == iOther.hasSideChannel)
          && (deviceName == iOther.deviceName)
          && (streamId == iOther.streamId)
          && (connected == iOther.connected)
          && (streamDiagnostics == iOther.streamDiagnostics));
}

bool IasLocalAudioStreamAttributes::operator !=(const IasLocalAudioStreamAttributes &iOther) const
{
  return !(*this == iOther);
}

IasLocalAudioStreamDiagnostics::IasLocalAudioStreamDiagnostics()
  : basePeriod(0u)
  , baseFreq(0u)
  , baseFillMultiplier(0u)
  , baseFillMultiplierTx(0u)
  , cycleWait(0u)
  , totalBufferSize(0u)
  , bufferReadThreshold(0u)
  , resetBuffersCount(0u)
  , deviationOutOfBounds(0u)
{
}

IasLocalAudioStreamDiagnostics::IasLocalAudioStreamDiagnostics(const IasLocalAudioStreamDiagnostics &iOther)
  : basePeriod(iOther.basePeriod)
    , baseFreq(iOther.baseFreq)
    , baseFillMultiplier(iOther.baseFillMultiplier)
    , baseFillMultiplierTx(iOther.baseFillMultiplierTx)
    , cycleWait(iOther.cycleWait)
    , totalBufferSize(iOther.totalBufferSize)
    , bufferReadThreshold(iOther.bufferReadThreshold)
    , resetBuffersCount(iOther.resetBuffersCount)
    , deviationOutOfBounds(iOther.deviationOutOfBounds)
{
}

IasLocalAudioStreamDiagnostics::IasLocalAudioStreamDiagnostics(uint32_t iBasePeriod, uint32_t iBaseFreq, uint32_t iBaseFillMultiplier, uint32_t iBaseFillMultiplierTx, uint32_t iCycleWait, uint32_t iTotalBufferSize, uint32_t iBufferReadThreshold, uint32_t iResetCount, uint32_t iDeviationCount)
  : basePeriod(iBasePeriod)
    , baseFreq(iBaseFreq)
    , baseFillMultiplier(iBaseFillMultiplier)
    , baseFillMultiplierTx(iBaseFillMultiplierTx)
    , cycleWait(iCycleWait)
    , totalBufferSize(iTotalBufferSize)
    , bufferReadThreshold(iBufferReadThreshold)
    , resetBuffersCount(iResetCount)
    , deviationOutOfBounds(iDeviationCount)
{
}

IasLocalAudioStreamDiagnostics &IasLocalAudioStreamDiagnostics::operator =(const IasLocalAudioStreamDiagnostics &iOther)
{
  if (this != &iOther)
  {
      basePeriod = iOther.basePeriod;
      baseFreq = iOther.baseFreq;
      baseFillMultiplier = iOther.baseFillMultiplier;
      baseFillMultiplierTx = iOther.baseFillMultiplierTx;
      cycleWait = iOther.cycleWait;
      totalBufferSize = iOther.totalBufferSize;
      bufferReadThreshold = iOther.bufferReadThreshold;
      resetBuffersCount = iOther.resetBuffersCount;
      deviationOutOfBounds = iOther.deviationOutOfBounds;
  }
  return *this;
}

bool IasLocalAudioStreamDiagnostics::operator ==(const IasLocalAudioStreamDiagnostics &iOther) const
{
  return (this == &iOther)
      || ( (basePeriod == iOther.basePeriod)
        && (baseFreq == iOther.baseFreq)
        && (baseFillMultiplier == iOther.baseFillMultiplier)
        && (baseFillMultiplierTx == iOther.baseFillMultiplierTx)
        && (cycleWait == iOther.cycleWait)
        && (totalBufferSize == iOther.totalBufferSize)
        && (bufferReadThreshold == iOther.bufferReadThreshold)
        && (resetBuffersCount == iOther.resetBuffersCount)
        && (deviationOutOfBounds == iOther.deviationOutOfBounds));
}

bool IasLocalAudioStreamDiagnostics::operator !=(const IasLocalAudioStreamDiagnostics &iOther) const
{
  return !(*this == iOther);
}

} // namespace IasMediaTransportAvb
