/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * @file    IasAvbAudioShmProvider.cpp
 * @brief   The definition of the IasAvbAudioShmProvider class.
 *
 * @date    2016
 */

#include <pthread.h>
#include "avb_streamhandler/IasAvbAudioShmProvider.hpp"
#include "internal/audio/common/audiobuffer/IasAudioRingBuffer.hpp"
#include "internal/audio/common/alsa_smartx_plugin/IasAlsaPluginIpc.hpp"
#include "internal/audio/common/alsa_smartx_plugin/IasAlsaHwConstraintsStatic.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/ias_safe.h"
#define ITERATE_IN_LOCAL_BUFFER

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbAudioShmProvider::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):" + "[" + mParams->name + "]"

IasAvbAudioShmProvider::IasAvbAudioShmProvider(const std::string & deviceName)
  :mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_SHM"))
  ,mDeviceName("avb_" + deviceName)
  ,mParams()
  ,mShmConnection()
  ,mIpcThread(nullptr)
  ,mIsRunning(false)
  ,mInIpc(nullptr)
  ,mOutIpc(nullptr)
  ,mDirWriteToShm(false)
  ,mNullData(nullptr)
  ,mDescMode(AudioBufferDescMode::eIasAudioBufferDescModeOff)
  ,mPtpProxy(nullptr)
  ,mAlsaPrefill(0u)
  ,mBufferState(eIdle)
  ,mBufRstContCnt(0u)
  ,mBufRstContCntThr(0u)
  ,mAlsaPrefilledSz(0u)
  ,mShmBufferLock()
  ,mIsClientSmartX(true)
  ,mLastPtpEpoch(0u)
  ,mDbgLastTxBufOverrunIdx(0u)
{
}


IasAvbAudioShmProvider::~IasAvbAudioShmProvider()
{
  (void) cleanup();
}


IasAvbProcessingResult IasAvbAudioShmProvider::init(uint16_t numChannels, uint32_t alsaPeriodSize, uint32_t numAlsaPeriods,
                                                    uint32_t sampleRate, bool dirWriteToShm)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // Create the parameter object and let a shared pointer point to it
  mParams = std::make_shared<IasAudio::IasAudioDeviceParams>();
  if (nullptr == mParams)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't create shared pointer");
    result = eIasAvbProcErr;
  }

  if (eIasAvbProcOK == result)
  {
    // Save direction of data flow, change device name accordingly
    mDirWriteToShm = dirWriteToShm;
    if (dirWriteToShm)
    {
      mDeviceName += "_c";
    }
    else
    {
      mDeviceName += "_p";
    }

    mParams->name        = mDeviceName;    // name of the AVB Alsa device. This is the ALSA PCM device name
    mParams->numChannels = numChannels;    // number of channels
    mParams->samplerate  = sampleRate;     // sample rate in Hz, e.g. 48000
    mParams->dataFormat  = IasAudio::eIasFormatInt16;
    mParams->clockType   = IasAudio::eIasClockProvided;
    mParams->periodSize  = alsaPeriodSize; // period size in frames
    mParams->numPeriods  = numAlsaPeriods; // number of periods that the ring buffer consists of

    // Set the prefill value for capture devices half the buffer size (in periods)
    mAlsaPrefill = numAlsaPeriods / 2;

    // Look up an overrode default value
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaDeviceBasePrefill, mAlsaPrefill);

    // Lookup device name in registry to check whether an override of the prefill value is requested for this Alsa device
    std::string optName = std::string(IasRegKeys::cAlsaDevicePrefill) + mDeviceName;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(optName, mAlsaPrefill);

    if (mAlsaPrefill != numAlsaPeriods / 2)
    {
      if (mAlsaPrefill > numAlsaPeriods)
      {
        // If the prefill level exceeds the buffer size (in periods) throw a warning message and set prefill level to max
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Requested prefill level for device", mDeviceName.c_str(),
                    "exceeds maximum! (> ", numAlsaPeriods, "! Set to max!", mAlsaPrefill, "->", numAlsaPeriods);
        mAlsaPrefill = numAlsaPeriods;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Changing prefill value for Alsa device",
                    mDeviceName.c_str(), "from", numAlsaPeriods / 2, "to", mAlsaPrefill);
      }
    }

    if (isPrefillEnable())
    {
      optName = std::string(IasRegKeys::cAlsaPrefillBufResetThresh) + mDeviceName;
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(optName, mBufRstContCntThr);
       DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "alsa-prefill: buffer-reset count to trigger pre-fill",
                    uint32_t(mBufRstContCntThr));
     }

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "name=", mParams->name,
                "numChannels=", mParams->numChannels,
                "samplerate=", mParams->samplerate,
                "dataFormat=", (uint64_t)mParams->dataFormat,
                "clockType=", (uint64_t)mParams->clockType,
                "periodSize=", mParams->periodSize,
                "numPeriods=", mParams->numPeriods
                );

    mNullData = new (nothrow) AudioData[alsaPeriodSize];
    AVB_ASSERT(mNullData != nullptr);

    if (!dirWriteToShm)
    {
      // Clear Null data buffer when used in read access
      (void) memset(mNullData, 0, sizeof (AudioData) * alsaPeriodSize);
    }

    std::string alsaGroupName;
    IasAudio::IasAudioCommonResult res = IasAudio::eIasResultUnknown;

    if (!IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaGroupName, alsaGroupName))
    {
      res = mShmConnection.createConnection(mParams->name);
    }
    else
    {
      res = mShmConnection.createConnection(mParams->name, alsaGroupName);
    }

    if (res != IasAudio::eIasResultOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't create connection to ", mParams->name);
      result = eIasAvbProcErr;
    }
  }

  if (eIasAvbProcOK == result)
  {
    IasAudio::IasAudioCommonResult res = mShmConnection.createRingBuffer(mParams);
    if (res != IasAudio::eIasResultOk)
    {
      result = eIasAvbProcErr;
    }
    else
    {
      if (mDirWriteToShm) // capture
      {
        const bufferState nextState = (isPrefillEnable()) ? eIdle
                                                          : eRunning; // pre-filling is not required
        (void) lockShmBuffer();
        result = resetShmBuffer(nextState);
        (void) unlockShmBuffer();
      }

      if (eIasAvbProcOK != result)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't prefill Alsa device");
      }
      else
      {
        // Setup of bidirectional communication channel
        mInIpc  = mShmConnection.getInIpc();
        mOutIpc = mShmConnection.getOutIpc();

        if (mInIpc == nullptr)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mInIpc == nullptr");
        }
        if (mOutIpc == nullptr)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mOutIpc == nullptr");
        }

        AVB_ASSERT(mInIpc  != nullptr);
        AVB_ASSERT(mOutIpc != nullptr);

        // Set the constraints of the Alsa device being created
        setHwConstraints();

        // Create IPC communicator thread
        mIsRunning = true;
        mIpcThread = new std::thread([this]{ipcThread();});
        if (mIpcThread == nullptr)
        {
          mIsRunning = false;
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Out of memory while creating IPC thread");
          result = eIasAvbProcErr;
        }
        else
        {
          pthread_setname_np(mIpcThread->native_handle(), "AvbShmProvider");
        }
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioTstampBuffer, mDescMode);
    if (hasBufferDesc())
    {
      mPtpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
      if (mPtpProxy == nullptr)
      {
        result = eIasAvbProcErr;
      }
      else
      {
        mLastPtpEpoch = mPtpProxy->getEpochCounter();
      }
    }

    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaSmartXSwitch, mIsClientSmartX);
  }

  return result;
}


IasAvbProcessingResult IasAvbAudioShmProvider::cleanup()
{
  mIsRunning = false;

  if (nullptr != mIpcThread)
  {
    // Cancel the wait for condition variable
    pthread_cancel(mIpcThread->native_handle());
    mIpcThread->join();
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "IPC thread successfully ended");
    delete mIpcThread;
    mIpcThread = nullptr;
  }

  delete[] mNullData;
  mNullData = NULL;

  return eIasAvbProcOK;
}


IasAvbProcessingResult IasAvbAudioShmProvider::copyJob(const IasLocalAudioStream::LocalAudioBufferVec & buffers,
                                                       IasLocalAudioBufferDesc * descQ, uint32_t numFrames,
                                                       bool dummy, uint64_t timestamp)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!mIsRunning) // Not initialized yet or IPC communicator thread is not running
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized or IPC communicator thread is not running!");
    result = eIasAvbProcErr;
  }
  else if ((0u == numFrames) || (mParams->periodSize % numFrames != 0u))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "bad number of frames:", numFrames);
    result = eIasAvbProcErr;
  }
  else
  {
    bool resetRingBuf      = false;
    bool restartPrefilling = false;

    (void) lockShmBuffer();

    // Determine transmission direction
    IasAudio::IasRingBufferAccess accessDirection =
        mDirWriteToShm ? IasAudio::eIasRingBufferAccessWrite : IasAudio::eIasRingBufferAccessRead;

    uint32_t shmOffset = 0; // offset in area steps (== frames)
    uint32_t shmFrames = numFrames;
    uint32_t numChannels = mParams->numChannels;
    AVB_ASSERT(buffers.size() == numChannels); // Have to be identical!

    IasAudio::IasAudioArea *shmAreas; // Pointer to the areas already created by the SHM
    bool shmNoData = false; // Indicates that no data is available for read from SHM or no space left to write to SHM

    // Request a pointer to the shared memory ring buffer
    IasAudio::IasAudioRingBuffer *shmBuffer = nullptr;
    shmBuffer = mShmConnection.getRingBuffer();

    if (nullptr == shmBuffer)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not get IPC ring buffer");
      result = eIasAvbProcErr;
    }
    else
    {
      AVB_ASSERT(shmBuffer->getNumChannels() == numChannels); // number of channels has to be identical

      bool isLocked = false;
      if (shmBuffer->beginAccess(accessDirection, &shmAreas, &shmOffset, &shmFrames))  // shmFrames returned here is the overall numbers of frames available for read or write, but not more than requested
      {
        // this could happen when multiple threads concurrently access the ring buffer in the same direction
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "error in beginAccess of ring buffer");
        result = eIasAvbProcErr;
      }
      else
      {
        isLocked = true;

        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "beginAccess() succeeded, shmFrames=", shmFrames,
                    " periodSize=", mParams->periodSize, " dummy=", dummy, " prefill=", mAlsaPrefill);

        if ((isPrefillEnable()) && (IasAudio::eIasRingBufferAccessWrite == accessDirection)) // capture path
        {
          const bufferState prevBufferState = mBufferState;
          uint32_t readOffset = 0u;
          uint32_t readAvailable = 0u;
          uint32_t prefillLevel = mAlsaPrefilledSz;
          IasAudio::IasAudioRingBufferResult rbres = IasAudio::eIasRingBuffOk;
          switch (mBufferState)
          {
            case eIdle:
              rbres = shmBuffer->updateAvailable(IasAudio::eIasRingBufferAccessRead, &readAvailable);
              if (IasAudio::eIasRingBuffOk != rbres)
              {
                // fail-safe
                mBufferState = eRunning;
                mAlsaPrefilledSz = 0u;
              }
              else
              {
                readOffset = shmBuffer->getReadOffset();
                if ((0u == readOffset) && (0u == readAvailable))
                {
                  // special case: prefilled entire buffer space and smartx flushed it
                  mBufferState = ePrefilling;
                  mAlsaPrefilledSz += shmFrames;
                }
                else if (0u != readOffset)
                {
                  // client started pulling data
                  mBufferState = ePrefilling;
                  mAlsaPrefilledSz += shmFrames;
                }
                else
                {
                  // ideling; client has not started pulling data, don't update the ring buffer
                  shmFrames = 0u;
                }
              }

              break;
            case ePrefilling:
              if (mAlsaPrefilledSz >= (mParams->periodSize * mAlsaPrefill))
              {
                mBufferState = eRunning;
                mAlsaPrefilledSz = 0u;
              }
              else
              {
                mAlsaPrefilledSz += shmFrames;
              }

              break;

            case eRunning:
              break;

            default:
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "unexpected buffer state ", uint32_t(mBufferState));
              break;

          }

          if (eRunning != mBufferState)
          {
            shmNoData = true; // discard data from an AVB stream
          }

          if (prevBufferState != mBufferState)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "SHM buffer state change",
                        uint32_t(prevBufferState), "->", uint32_t(mBufferState), "fill-level/total =",
                        prefillLevel, (mParams->periodSize * mParams->numPeriods));
          }
        }

        if (!shmNoData && (shmFrames < numFrames))
        {
          AVB_ASSERT(0 == shmFrames); // This should not happen because the buffer size is a multiple of the period size
                                      // and we do not copy more than one period. So it has to be the requested size
                                      // or '0' if no space is left to write / no data to read is available
          if (0u != shmFrames)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "AVB_ASSERT(0 == shmFrames) failed!!!");
          }

          shmNoData = true;           // Indicate that there is nothing to be read from or to be written to ring buffer

          // In capture use case clear the buffer. This prevents the client from reading old and obsolete samples
          // on the one hand and prevents Alsa from blocking on the other hand.
          if (IasAudio::eIasRingBufferAccessWrite == accessDirection)
          {
            resetRingBuf = true;
            if (isPrefillEnable() && !mIsClientSmartX)
            {
              /*
               * Reset the hw_ptr of shared memory buffer if connected client is SmartXbar.
               *
               * For non-SmartX applications such as arecord, we should not reset the buffer on the fly
               * otherwise it will cause unrecoverable infinite loop on the client side. ALSA ioplug might
               * miss some updates of the hw_ptr due to scheduling issues, under such situations ALSA might
               * report conflicted buffer status to applications, e.g. some API reports that the buffer has
               * readable samples, but other API report no available samples. Such a mixed message confuses
               * application, which might cause an infinite loop as below.
               *
               *  while(1) {
               *    snd_pcm_wait();   // wait function breaks immediately because it considers buffer has samples
               *    snd_pcm_readi();  // but read function returns null-data because it considers no available samples
               *  }
               *
               * ALSA ioplug cannot recover in the worst case, which leads incredible high cpu load of application
               * reaching into 100% cpu consumption. To avoid such a misbehavior we should not reset the hw_ptr
               * once stream started. We introduced call of resetFromWriter() to fix the issue below.
               *
               * Change-Id: I06f0aab02b48aebaebd9517dfe761f934cbc4e44
               * This schould fix blocking arecord issue and eliminates the obsolete samples at the beginning of a recording
               *
               * We can safely remove the call of resetFromWriter() from copyJob() in the alsa-prefilling mode,
               * because the feature resets the buffer at every time arecord gets called. it detects the start of
               * arecord by receicing the eIasAudioIpcStart message and calls resetFromWriter() before stream
               * starts. By doing that we can avoid the former issue caused arecord blocking and capuruging old samples.
               *
               */
              resetRingBuf = false;
            }

            DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "Capture: SHM buffer is full");

            if ((isPrefillEnable()) && (eRunning == mBufferState))
            {
              /*
               * AVBSH considers client stopped data consuming when it detected buffer-full event. And then it restarts
               * pre-filling. In order to avoid frequent buffer state changing, it should happen when buffer-full event
               * continuously happened more than the threshold count. Default threshold count is 0. It might need to be
               * increased under circumstances where thread blocking on client side frequently happens.
               */
              if (mBufRstContCntThr < ++mBufRstContCnt) // to avoid chattering
              {
                /*
                 * trigger prefilling
                 *
                 * Write a dummy period. When SmartX connects to this AVB source port next time, the written period will
                 * be flushed first. After that we can start prefilling.
                 */
                restartPrefilling = true;
                if (!mIsClientSmartX)
                {
                  restartPrefilling = false;
                }
              }
              else
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "waiving SHM buffer state change", uint32_t(mBufRstContCnt));
              }
            }
          }
        }
      }

      if (!dummy && (eIasAvbProcOK == result))
      {
        uint64_t now = 0u;
        uint64_t maxPtGap = 0u; // max presentation time gap
        uint64_t readIndex = 0u;
        uint64_t writeIndex = 0u;
        bool   useDesc = hasBufferDesc();
        bool   isReadReady = true;
        bool   toBePresented = false;
        bool   resetRequested = false;
        IasLocalAudioBufferDesc::AudioBufferDesc desc;

        bool   alsaRxSyncStart = false;
        uint64_t skippedTime = 0u;

        if (true == useDesc)
        {
          AVB_ASSERT(NULL != descQ);
          AVB_ASSERT(NULL != buffers[0]);
          // prevent AvbTxWrk & AvbRxWrk from accessing the local audio stream
          descQ->lock();

          alsaRxSyncStart = descQ->getAlsaRxSyncStartMode();

          if (nullptr != mPtpProxy)
          {
            now = mPtpProxy->getLocalTime();
          }
          else
          {
            // fatal error, output an error message but no way to recover
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ptp proxy == NULL!");
          }

          uint32_t epoch = (nullptr != mPtpProxy) ? mPtpProxy->getEpochCounter() : 0u;
          if (epoch != mLastPtpEpoch)
          {
            mLastPtpEpoch = epoch;
            /*
             * ptp negative timejump: Discard old samples derived from former ptp timebase.
             * Old samples would be seen placed in future because those samples derived from
             * ptp time before the negative time jump happened. Unless resetting the buffer,
             * newly produced samples based on current ptp timebase might be delayed for
             * unpredictable time due to old samples.
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "ptp time warp detected - resetting local audio buffer");
            resetRequested = true;
          }

          if (IasAudio::eIasRingBufferAccessWrite == accessDirection) // receive stream
          {
            /*
             * wait until buffer fill level reaches at least half-full in time-aware buffering mode
             */
            isReadReady = buffers[0]->isReadReady();

            if (true == isReadReady)
            {
              uint32_t totalSz = buffers[0]->getTotalSize() - 1u;
              maxPtGap = uint32_t(double(totalSz) / double(mParams->samplerate) * 1e9);

              if (eIasAvbProcOK == descQ->peek(desc))
              {
                readIndex  = buffers[0]->getMonotonicReadIndex();
                writeIndex = buffers[0]->getMonotonicWriteIndex();
                uint32_t readIndexPerSampleRate = uint32_t(readIndex % mParams->samplerate);

                if (alsaRxSyncStart && (0u != readIndex) && (0u != desc.bufIndex)) // not initial reading
                {
                  // adjust timestamp
                  int64_t skippedSamples = readIndex - desc.bufIndex;
                  if (skippedSamples > 0)
                  {
                    if (skippedSamples < numFrames)
                    {
                      double timePerSample = 0.0f;

                      IasLocalAudioBufferDesc::AudioBufferDesc descY;
                      if ((eIasAvbProcOK == descQ->peekX(descY, 1u)) &&
                          (descY.timeStamp > desc.timeStamp) && (descY.bufIndex > desc.bufIndex))
                      {
                        // timePerSample = ((TSy - TSx) / (CNTy - CNTx))
                        timePerSample = double(descY.timeStamp - desc.timeStamp) /
                                            double(descY.bufIndex - desc.bufIndex);
                      }
                      else
                      {
                        timePerSample = 1e9 / double(mParams->samplerate);
                      }
                      skippedTime = uint32_t(double(skippedSamples) * timePerSample);
                      desc.timeStamp += skippedTime;
                    }
                    else
                    {
                      // don't expect delay larger than one period
                      resetRequested = true;
                      descQ->setAlsaRxSyncStartMode(false); // fall back

                      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "timestamp interpolation out of bounds,"
                                  "skipped =", skippedSamples, ", entering fallback mode");
                    }
                  }
                }

                // detect a far-away timestamp
                uint64_t timeGap = (now > desc.timeStamp) ? (now - desc.timeStamp) : (desc.timeStamp - now);
                if ((maxPtGap < timeGap) && (IasLocalAudioBufferDesc::eIasAudioBufferDescModeHard == mDescMode))
                {
                  DLT_LOG(*mLog, DLT_LOG_WARN, DLT_STRING("[IasAvbAudioShmProvider::copyJob]"),
                      DLT_STRING("detected out-of-bound presentation timestamp"), DLT_STRING("timestamp="),
                      DLT_UINT64(desc.timeStamp), DLT_STRING("now="), DLT_UINT64(now));
                  resetRequested = true;
                }
                else
                {
                  if (desc.timeStamp <= now)
                  {
                    toBePresented = true; // to be presented now

                    if (alsaRxSyncStart && (0u == readIndex) && (0u == desc.bufIndex)) // initial reading
                    {
                      IasLocalAudioBuffer *buffer = buffers[0];
                      if (nullptr != buffer)
                      {
                        /*
                         * skip samples belonging to past timestamps
                         *
                         * Although RX worker grants read access to the local audio buffer once its fill level
                         * reached at 'perioSz * baseFillMultiplier', there would be a time lag until ALSA worker
                         * starts pulling samples because ALSA worker and RX worker are asynchronously running.
                         * In order to keep RX path latency in constant value as much as possible, adjust buffer
                         * level to the expected value by skipping time-expired samples.
                         */
                        const uint32_t cReadThreashold = buffer->getReadThreshold();
                        const uint32_t cFillLevel = buffer->getFillLevel();;
                        uint32_t samplesOverReadThreshold = 0u;

                        if (cReadThreashold < cFillLevel)
                        {
                          samplesOverReadThreshold = cFillLevel - cReadThreashold;

                          double timePerSample = 0.0f;

                          IasLocalAudioBufferDesc::AudioBufferDesc descY;
                          if ((eIasAvbProcOK == descQ->peekX(descY, 1u)) &&
                              (descY.timeStamp > desc.timeStamp) && (descY.bufIndex > desc.bufIndex))
                          {
                            // timePerSample = ((TSy - TSx) / (CNTy - CNTx))
                            timePerSample = double(descY.timeStamp - desc.timeStamp) /
                                                double(descY.bufIndex - desc.bufIndex);
                          }
                          else
                          {
                            timePerSample = 1e9 / double(mParams->samplerate);
                          }

                          uint32_t samplesToSkip = uint32_t(double(now - desc.timeStamp) / timePerSample);

                          if (samplesToSkip > samplesOverReadThreshold)
                          {
                            samplesToSkip = samplesOverReadThreshold;
                          }

                          if (samplesToSkip < numFrames) // don't expect delay more than one period
                          {
                            for (uint32_t channel = 0; channel < numChannels; channel++)
                            {
                              buffer = buffers[channel];
                              if (nullptr != buffer)
                              {
                                buffer->read(mNullData, samplesToSkip);
                              }
                            }
                          }
                          else
                          {
                            /*
                             * fail-safe: just in case unexpected thing happened
                             *
                             * Because ALSA worker checks pts at period time interval, delay more than one period
                             * should not happen. But fall back to the legacy 'non-alsaRxSyncStart' just in case
                             * such unexpected situations happened.
                             */

                            descQ->setAlsaRxSyncStartMode(false); // fall back

                            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "timestamp interpolation out of bounds,"
                                "skipped =", samplesToSkip, ", entering fallback mode");

                          }
                        }
                      }
                    }
                  }
                  else
                  {
                    // we have samples but the time is not ripe yet
                    if (writeIndex < mParams->samplerate) // keep checking until the AVB layer writes samples of 1 sec
                    {
                      // strictly check timestamp at the startup time regardless of the fail-safe mode
                      toBePresented = false;

                      if (alsaRxSyncStart)
                      {
                        if (readIndex != 0u)  // already started reading samples
                        {
                          toBePresented = true; // skip synchronization because it's already done at the beginning
                        }
                      }
                    }
                    else
                    {
                      const uint32_t fillLevel = buffers[0]->getFillLevel();
                      const uint32_t threshold = buffers[0]->getReadThreshold();

                      if (threshold <= fillLevel)
                      {
                        // now we are at the risk of buffer overrun if we don't allow ALSA grab samples

                        if (IasLocalAudioBufferDesc::eIasAudioBufferDescModeFailSafe == mDescMode)
                        {
                          // allow presenting samples but give it warn
                          toBePresented = true;
                        }

                        if ((0u == descQ->getDbgPresentationWarningTime()) ||
                            ((now - descQ->getDbgPresentationWarningTime()) > uint64_t(1e9))) // output once per second
                        {
                          DltLogLevelType logLevel = DLT_LOG_INFO;
                          uint64_t samplesToPresentOnAhead = uint64_t(double(desc.timeStamp - now) * double(mParams->samplerate) / 1e9);
                          if (!shmNoData && (numFrames < samplesToPresentOnAhead))
                          {
                            // warn if we're presenting samples which are not supposed to be presented in the current period
                            logLevel = DLT_LOG_WARN;
                          }

                          DLT_LOG_CXX(*mLog, logLevel, LOG_PREFIX, toBePresented ? "force" : "waive", "audio presentation",
                              ": now / pt / fillLevel / time-aware-mode =",
                              now, "/", desc.timeStamp, "/", fillLevel, "/", descQ->getAudioBufferDescModeString(mDescMode));

                          descQ->setDbgPresentationWarningTime(now);
                        }
                      }
                    }
                  }
                }
                // debug code
                if (readIndexPerSampleRate < mParams->periodSize)
                {
                  // output the debug message once per second
                  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "now =", now, "pt =", desc.timeStamp, "lag =", int64_t(now - desc.timeStamp), "ns");
                }
              } // descQ->peek()
            } // isReadReady
          } // receive stream
        } // time-aware mode

        // Iterate the channels
        for (uint32_t channel = 0; channel < numChannels; channel++)
        {
          uint32_t gap = 0u;
          uint32_t nrSamples = 0u;
          uint32_t samplesRead = 0u;
          uint32_t samplesWritten = 0u;

          IasLocalAudioBuffer *buffer = buffers[channel];
          AVB_ASSERT(NULL != buffer);

          IasAudio::IasAudioArea* area = &(shmAreas[channel]);
          AVB_ASSERT(nullptr != area);

          uint32_t step = 0;         // Step between samples belonging to the same channel in bytes
          char* shmData = nullptr;

          if (true == resetRequested)
          {
            continue;
          }

          if (shmNoData) // If there is no space left to write data or if there is no data available to be read use NullData
          {
            /*
             * clean up the code below once ITERATE_IN_LOCAL_BUFFER compiling flag was deleted.
             * The code logics between shmNoData case and Not-shmNoData case seems very similar thanks to
             * the new implementation of the non-interleaved mode. We can share some code portions
             * for the both once the legacy interleaved mode code was deleted, so that we can reduce
             * code redundancy and also to have better readability.
             */
            if (IasAudio::eIasRingBufferAccessRead == accessDirection)
            {
              if (0u == timestamp)
              {
                /*
                 * reference clock is not available yet, let the ALSA interface freewheel
                 *
                 * Note: if we write samples and descriptors while reference clock is not
                 * available, we need to set launch time with estimated timestamp. Once the
                 * clock becomes available the tx sequencer found this old timestamp is
                 * invalid and resets the stream. To avoid unneeded reseting we should let
                 * the ALSA freewheel if clock reference is not available.
                 */
                continue;
              }

              samplesWritten = buffer->write(mNullData, numFrames);

              if (true == useDesc)
              {
                if ((0 != samplesWritten) && (0u == channel))
                {
                  // store timestamp to FIFO
                  desc.timeStamp = timestamp;
                  desc.bufIndex  = buffer->getMonotonicWriteIndex() - samplesWritten;
                  desc.sampleCnt = samplesWritten;
                  descQ->enqueue(desc);
                }
              }
            }
            else // eIasRingBufferAccessWrite
            {
              if (true != isReadReady)
              {
                continue;
              }

              if (true == useDesc)
              {
                // read a descriptor w/o dequeuing
                if (eIasAvbProcOK == descQ->peek(desc))
                {
                  readIndex = buffer->getMonotonicReadIndex();
                  if (readIndex < desc.bufIndex)
                  {
                    /*
                     * lost a descriptor due to overrun or reset
                     */
                    if (0u == channel) // output message once per stream
                    {
                      DLT_LOG(*mLog, DLT_LOG_ERROR, DLT_STRING("[IasAvbAudioShmProvider::copyJob]"),
                          DLT_STRING("detected unexpected timestamp"), DLT_STRING("bufIndex="), DLT_UINT64(desc.bufIndex),
                          DLT_STRING("sampleCnt="), DLT_UINT64(desc.sampleCnt), DLT_STRING("readIndex="), DLT_UINT64(readIndex));
                    }

                    // discard samples which lost mapping to the descriptor
                    gap = uint32_t(desc.bufIndex - readIndex);
                    while (0u != gap)
                    {
                      // need to discard samples of 'gap' size but mNullData has only buffer of mParams->periodSize
                      nrSamples = std::min(gap, mParams->periodSize);
                      samplesRead = buffer->read(mNullData, nrSamples);
                      if (0u == samplesRead)
                      {
                        /*
                         * this will not happen but just in case infinite loop happened
                         * due to unanticipated problem
                         */
                        break;
                      }
                      readIndex = buffer->getMonotonicReadIndex();
                      gap = uint32_t(desc.bufIndex - readIndex);
                    }
                  }

                  if (true == toBePresented)
                  {
                    // remaining samples which are not yet read
                    nrSamples = desc.sampleCnt - uint32_t(readIndex - desc.bufIndex);
                    if (alsaRxSyncStart)
                    {
                      nrSamples = buffer->getFillLevel();
                    }
                    nrSamples = std::min(numFrames, nrSamples);

                    // to be presented now
                    buffer->read(mNullData, nrSamples);

                    // delete used descriptors when the samples of the last channel were grabbed
                    if (channel == (numChannels - 1))
                    {
                      readIndex = buffer->getMonotonicReadIndex();
                      do
                      {
                        if ((desc.bufIndex + desc.sampleCnt) <= readIndex)
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
                }
              }
              else /* !useDesc */
              {
                buffer->read(mNullData, numFrames);
              }
            }
          }
          else
          {
#ifdef ITERATE_IN_LOCAL_BUFFER
            if (0 == area->step)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "sample stride must be a positive integer gt zero");
              result = eIasAvbProcErr;
            }
            else
            {
              step = area->step / 8; //stride to next sample
              // Calculate pointer to the shared memory location
              shmData = static_cast<char*>(area->start) + area->first / 8 + shmOffset * step;

              if (IasAudio::eIasRingBufferAccessRead == accessDirection)
              {
                if (0u == timestamp)
                {
                  continue;
                }

#if defined(PERFORMANCE_MEASUREMENT)
                if (IasAvbStreamHandlerEnvironment::isAudioFlowLogEnabled()) // latency analysis
                {
                  uint32_t state = 0u;
                  uint64_t logtime = 0u;
                  (void) IasAvbStreamHandlerEnvironment::getAudioFlowLoggingState(state, logtime);

                  if ((2u == state) && (0u == channel))
                  {
                    if (0 != mNullData[0]) // quick check to see if it is really a null data buffer
                    {
                      (void) std::memset(mNullData, 0, mParams->periodSize * sizeof(AudioData));
                    }

                    const uint32_t bufSize = static_cast<uint32_t>(shmFrames * sizeof(AudioData));

                    if ((0 != reinterpret_cast<AudioData*>(shmData)[0]) || // quick check to avoid memcmp as much as possible
                        (0 != std::memcmp(shmData, mNullData, bufSize)))
                    {
                      uint64_t tscNow = mPtpProxy->getTsc();
                      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                                  "latency-analysis(3): passed samples to AVTP system time =", tscNow,
                                  "delta =", (double)(tscNow - logtime) / 1e6, "ms");
                      IasAvbStreamHandlerEnvironment::setAudioFlowLoggingState(3u, tscNow);
                    }
                  }
                }
#endif

                samplesWritten = buffer->write(reinterpret_cast<AudioData*>(shmData), shmFrames, step);

                if (true == useDesc)
                {
                  if ((0u != samplesWritten) && (0 == channel))
                  {
                    // store timestamp to FIFO
                    desc.timeStamp = timestamp;
                    desc.bufIndex  = buffer->getMonotonicWriteIndex() - samplesWritten;
                    desc.sampleCnt = samplesWritten;
                    descQ->enqueue(desc);
                  }

                  if ((shmFrames != samplesWritten) && (0 == channel))
                  {
                    // warn if TX worker is reading data. i.e if read index is counting up.
                    if (buffer->getMonotonicReadIndex() != mDbgLastTxBufOverrunIdx)
                    {
                      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "tx buffer overrun",
                                  "written =", samplesWritten, "expected =", shmFrames, "stream might be inactive");
                    }
                    mDbgLastTxBufOverrunIdx = buffer->getMonotonicReadIndex();
                  }
                }
              }
              else // eIasRingBufferAccessWrite
              {
                if ((isPrefillEnable()) && (0u != shmBuffer->getReadOffset()))
                {
                  mBufRstContCnt = 0u; // client consumed data
                }

                if (true != isReadReady)
                {
                  continue;
                }

                if (true == useDesc)
                {
                  // read a descriptor w/o dequeuing
                  if (eIasAvbProcOK == descQ->peek(desc))
                  {
                    readIndex = buffer->getMonotonicReadIndex();
                    if (readIndex < desc.bufIndex)
                    {
                      /*
                       * lost a descriptor due to overrun or reset
                       */
                      if (0u == channel) // output message once per stream
                      {
                        DLT_LOG(*mLog, DLT_LOG_ERROR, DLT_STRING("[IasAvbAudioShmProvider::copyJob]"),
                            DLT_STRING("detected unexpected timestamp"), DLT_STRING("bufIndex="), DLT_UINT64(desc.bufIndex),
                            DLT_STRING("sampleCnt="), DLT_UINT64(desc.sampleCnt), DLT_STRING("readIndex="), DLT_UINT64(readIndex));
                      }

                      // discard samples which lost mapping to the descriptor
                      gap = uint32_t(desc.bufIndex - readIndex);
                      while (0u != gap)
                      {
                        // need to discard samples of 'gap' size but mNullData has only buffer of mParams->periodSize
                        nrSamples = std::min(gap, mParams->periodSize);
                        samplesRead = buffer->read(mNullData, nrSamples);
                        if (0u == samplesRead)
                        {
                          /*
                           * this will not happen but just in case infinite loop happened
                           * due to unanticipated problem
                           */
                          break;
                        }
                        readIndex = buffer->getMonotonicReadIndex();
                        gap = uint32_t(desc.bufIndex - readIndex);
                      }
                    }

                    if (true == toBePresented)
                    {
                      // remaining samples which are not yet read
                      nrSamples = desc.sampleCnt - uint32_t(readIndex - desc.bufIndex);
                      if (alsaRxSyncStart)
                      {
                        nrSamples = buffer->getFillLevel();
                      }
                      nrSamples = std::min(shmFrames, nrSamples);

                      buffer->read(reinterpret_cast<AudioData*>(shmData), nrSamples, step);

#if defined(PERFORMANCE_MEASUREMENT)
                      if (IasAvbStreamHandlerEnvironment::isAudioFlowLogEnabled()) // latency analysis
                      {
                        uint32_t state = 0u;
                        uint64_t logtime = 0u;
                        (void) IasAvbStreamHandlerEnvironment::getAudioFlowLoggingState(state, logtime);

                        if ((1u == state) && (0u == channel))
                        {
                          if (0 != mNullData[0]) // quick check to see if it is really a null data buffer
                          {
                            (void) std::memset(mNullData, 0, mParams->periodSize * sizeof(AudioData));
                          }

                          const uint32_t bufSize = static_cast<uint32_t>(shmFrames * sizeof(AudioData));

                          if ((0 != reinterpret_cast<AudioData*>(shmData)[0]) || // quick check to avoid memcmp as much as possible
                              (0 != std::memcmp(shmData, mNullData, bufSize)))
                          {
                            uint64_t tscNow = mPtpProxy->getTsc();
                            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                                        "latency-analysis(2): passed samples to Reference Plane (ALSA) system time =", tscNow,
                                        "delta =", (double)(tscNow - logtime) / 1e6, "ms", "fill-level =", buffer->getFillLevel(),
                                        "pts-delta =", int64_t(now - (desc.timeStamp + skippedTime)));
                            IasAvbStreamHandlerEnvironment::setAudioFlowLoggingState(2u, tscNow);
                          }
                        }
                      }
#endif

                      readIndex = buffer->getMonotonicReadIndex();
                      // delete used descriptors when the samples of the last channel were grabbed
                      if (channel == (numChannels - 1))
                      {
                        do
                        {
                          if ((desc.bufIndex + desc.sampleCnt) <= readIndex)
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
                  }
                  else
                  {
                    // underrun: fill the shared memory with silence
                    if (sizeof(IasLocalAudioBuffer::AudioData) == step) //not interleaved
                    {
                      size_t size = shmFrames * sizeof(AudioData);
                      avb_safe_result copyResult = avb_safe_memcpy(shmData, size, mNullData, size);
                      (void) copyResult;
                    }
                    else  // interleaved
                    {
                      AudioData * audioBuffer = reinterpret_cast<AudioData*>(shmData);
                      for (uint32_t sample = 0u; sample < shmFrames; sample++)
                      {
                        *audioBuffer = 0u;
                        audioBuffer += step/sizeof(IasLocalAudioBuffer::AudioData);
                      }
                    }
                  }
                }
                else /* !useDesc */
                {
                  buffer->read(reinterpret_cast<AudioData*>(shmData), shmFrames, step);
                }
              }
            }
#else
            // Iterate the frames and copy the samples
            for (uint32_t frame = 0; frame < shmFrames; frame++)
            {
              // Calculate pointer to the shared memory location
              shmData = static_cast<char*>(area->start) + area->first / 8 + shmOffset * numChannels * sizeof(AudioData) + step;
              step += area->step / 8; // advance to next sample

              if (IasAudio::eIasRingBufferAccessRead == accessDirection)
              {
                uint16_t samplesWritten = 0u;
                samplesWritten = uint16_t( buffer->write(reinterpret_cast<AudioData*>(shmData), 1) );

                if ((true == useDesc) && (0u != samplesWritten) && (frame + 1 == shmFrames))
                {
                  // store the timestamp to FIFO
                  IasLocalAudioBufferDesc::AudioBufferDesc desc;

                  desc.timeStamp = timestamp;
                  desc.bufIndex  = buffer->getMonotonicWriteIndex() - shmFrames;
                  desc.sampleCnt = shmFrames;
                  descQ->enqueue(desc);
                }
              }
              else // eIasRingBufferAccessWrite
              {
                if (true == useDesc)
                {
                  IasLocalAudioBufferDesc::AudioBufferDesc desc;

                  // prevent the write access from deleting a descriptor while being referred
                  descQ->lock();

                  // read a descriptor w/o dequeuing
                  if (eIasAvbProcOK == descQ->peek(desc))
                  {
                    if (desc.timeStamp <= now)
                    {
                      // to be presented now
                      buffer->read(reinterpret_cast<AudioData*>(shmData), 1);

                      if ((desc.bufIndex + desc.sampleCnt) <= buffer->getMonotonicReadIndex())
                      {
                        // delete the descriptor because all of its samples were read
                        descQ->dequeue(desc);
                      }
                    }
                    else
                    {
                      if (buffer->getMonotonicReadIndex() < desc.bufIndex)
                      {
                        // lost the previous descriptor due to buffer overrun? How to deal with the case?
                        buffer->read(reinterpret_cast<AudioData*>(shmData), 1);
                      }
                    }
                  }
                  descQ->unlock();
                }
                else /* !useDesc */
                {
                  buffer->read(reinterpret_cast<AudioData*>(shmData), 1);
                }
              }
            }
#endif
          } // shmNoData or not
        } // for each channel

        if (true == useDesc)
        {
          if (true == resetRequested)
          {
            // completely reset buffer and fifo
            for (uint32_t i = 0; i < numChannels; i++)
            {
              IasLocalAudioBuffer *ringBuf = buffers[i];
              AVB_ASSERT(NULL != ringBuf);
              ringBuf->reset(0u);
            }
            descQ->reset();
          }

          descQ->unlock();
        }
      }

      if (isLocked)
      {
        /*
         * call encAccess() when beginAccess() was successful
         *
         * beginAccess() might return eIasRingBuffNotAllowed while the ring buffer is updated by the IPC thread.
         * We must not call encAccess() in that case otherwise it will corrupt the state of the internal mutex.
         */
        if (shmBuffer->endAccess(accessDirection, shmOffset, shmFrames))
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error in endAccess of sink ring buffer");
          result = eIasAvbProcErr;
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "endAccess() succeeded, shmFrames =", shmFrames);
        }
      }

      if (resetRingBuf)
      {
        (void) shmBuffer->resetFromWriter();
      }

      if (restartPrefilling)
      {
        (void) resetShmBuffer(eIdle, mParams->periodSize);
      }
    }

    (void) unlockShmBuffer();
  }

  return result;
}


void IasAvbAudioShmProvider::abortTransmission()
{
}


void IasAvbAudioShmProvider::setHwConstraints()
{
  // Set the hardware device parameters.
  IasAudio::IasAlsaHwConstraintsStatic *constraints = mShmConnection.getAlsaHwConstraints();
  AVB_ASSERT(constraints != nullptr);
  AVB_ASSERT(mParams != nullptr);
  constraints->formats.list.push_back(mParams->dataFormat);
  constraints->access.list.push_back(IasAudio::eIasLayoutInterleaved);
  constraints->channels.list.push_back(mParams->numChannels);
  constraints->rate.list.push_back(mParams->samplerate);
  uint32_t periodSizeBytes = mParams->periodSize * mParams->numChannels * toSize(mParams->dataFormat);
  constraints->period_size.list.push_back(periodSizeBytes);
  constraints->period_count.list.push_back(mParams->numPeriods);
  constraints->buffer_size.list.push_back(periodSizeBytes * mParams->numPeriods);
  constraints->isValid = true;
}


void IasAvbAudioShmProvider::ipcThread()
{
  IasAudio::IasAudioCommonResult result;

#ifdef __ANDROID__
  cancelable_pthread_init();
#endif // #ifdef __ANDROID__

  while (mIsRunning == true)
  {
    mInIpc->waitForPackage();
    while(mInIpc->packagesAvailable())
    {
      // Receive/Send types
      IasAudio::IasAudioIpcPluginControl inControl;
      IasAudio::IasAudioIpcPluginControlResponse outResponse;
      IasAudio::IasAudioIpcPluginInt32Data outInt32;
      IasAudio::IasAudioIpcPluginParamData inParams;

      // Check if the package is extractable
      if(IasAudio::eIasResultOk == mInIpc->pop_noblock<IasAudio::IasAudioIpcPluginControl>(&inControl))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Received", toString(inControl), "control");
        // Check the contents of the control
        switch(inControl)
        {
          case IasAudio::eIasAudioIpcGetLatency:
          {
            outInt32.control = inControl;
            outInt32.response = 0;
            IasAudio::IasAudioRingBuffer *myRingBuffer = mShmConnection.getRingBuffer();
            // The ringbuffer is created in the init method. Init will fail if creation is not possible
            AVB_ASSERT(myRingBuffer != nullptr);
            uint32_t fillLevel = 0;
            IasAudio::IasAudioRingBufferResult rbres = myRingBuffer->updateAvailable(IasAudio::eIasRingBufferAccessRead, &fillLevel);
            // In this case we always have a real ringBuffer and the only things that could go wrong is passing illegal parameters to
            // the updateAvailable method. This is hardcoded here and thus always correct. And even if the call would fail the fillLevel
            // is still initialized to a valid value.
            AVB_ASSERT(rbres == IasAudio::eIasRingBuffOk);
            (void) rbres;
            outInt32.response = static_cast<int32_t>(fillLevel);
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginInt32Data>(outInt32);
          }
          break;
          case IasAudio::eIasAudioIpcStart:

            if (mDirWriteToShm) // capture
            {
              if (isPrefillEnable())
              {
                (void) lockShmBuffer();
                (void) resetShmBuffer(eRunning);
                (void) unlockShmBuffer();
              }
            }

            outResponse.control = inControl;
            outResponse.response = IasAudio::eIasAudioIpcACK;
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
            break;
          case IasAudio::eIasAudioIpcStop:

            if (mDirWriteToShm) // capture
            {
              if (isPrefillEnable())
              {
                (void) lockShmBuffer();
                (void) resetShmBuffer(eIdle);
                (void) unlockShmBuffer();
              }
            }

            outResponse.control = inControl;
            outResponse.response = IasAudio::eIasAudioIpcACK;
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
            break;
          case IasAudio::eIasAudioIpcDrain:
            outResponse.control = inControl;
            outResponse.response = IasAudio::eIasAudioIpcACK;
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
            break;
          case IasAudio::eIasAudioIpcPause:
            outResponse.control = inControl;
            outResponse.response = IasAudio::eIasAudioIpcACK;
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
            break;
          case IasAudio::eIasAudioIpcResume:
            outResponse.control = inControl;
            outResponse.response = IasAudio::eIasAudioIpcACK;
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
            break;
          default:
            outResponse.control = inControl;
            outResponse.response = IasAudio::eIasAudioIpcNAK;
            result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
        }
        if (result != IasAudio::eIasResultOk)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error sending response to", toString(inControl), "control =>", toString(result));
        }
      }
      else if(IasAudio::eIasResultOk == mInIpc->pop_noblock<IasAudio::IasAudioIpcPluginParamData>(&inParams))
      {
        if(inParams.control == IasAudio::eIasAudioIpcParameters)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Received parameters chosen by application");
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX,
                      "numChannels", inParams.response.numChannels,
                      "sampleRate", inParams.response.sampleRate,
                      "periodSize", inParams.response.periodSize,
                      "numPeriods", inParams.response.numPeriods,
                      "dataFormat", toString(inParams.response.dataFormat)
                     );
          outResponse.control = inParams.control;
          outResponse.response = IasAudio::eIasAudioIpcACK;
          result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Unexpected IasAudioIpcPluginParamData control", static_cast<uint32_t>(inParams.control));
          outResponse.control = inParams.control;
          outResponse.response = IasAudio::eIasAudioIpcNAK;
          result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
        }
        if (result != IasAudio::eIasResultOk)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error sending response to", toString(inParams.control), "control =>", toString(result));
        }
      }
      else
      {
        // Package was not expected;
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Unexpected IasAudioIpcPluginControl control", static_cast<uint32_t>(inParams.control));
        mInIpc->discardNext();
        outResponse.control = inParams.control;
        outResponse.response = IasAudio::eIasAudioIpcNAK;
        result = mOutIpc->push<IasAudio::IasAudioIpcPluginControlResponse>(outResponse);
        if (result != IasAudio::eIasResultOk)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error sending response to", toString(inParams.control), "control =>", toString(result));
        }
      }
    }
  }
}


IasAvbProcessingResult IasAvbAudioShmProvider::resetShmBuffer(bufferState nextState)
{
  IasAvbProcessingResult result = resetShmBuffer(nextState, mParams->periodSize * mAlsaPrefill);

  return result;
}


IasAvbProcessingResult IasAvbAudioShmProvider::resetShmBuffer(bufferState nextState, uint32_t frames)
{
  IasAvbProcessingResult result = eIasAvbProcErr;

  if (!mDirWriteToShm)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "not applicable to playback device");
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    if (nullptr == mParams)
    {
      result = eIasAvbProcNotInitialized;
    }
    else if ((nextState < eIdle) || (eRunning < nextState))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "invalid SHM buffer state change request", uint32_t(nextState));
      result = eIasAvbProcInvalidParam;
    }
    else
    {
      if (!isPrefillEnable())
      {
        mBufferState = nextState;
        result = eIasAvbProcOK;
      }
      else
      {
        bool isLocked = false;

        const IasAudio::IasRingBufferAccess accessDirection = IasAudio::eIasRingBufferAccessWrite;
        IasAudio::IasAudioArea *shmAreas = nullptr;
        uint32_t shmOffset = 0u;
        uint32_t shmFrames = (mParams->periodSize * mParams->numPeriods); // full buffer size

        IasAudio::IasAudioRingBuffer *shmBuffer = mShmConnection.getRingBuffer();

        if (nullptr == shmBuffer)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not get IPC ring buffer");
        }
        else
        {
          /*
           * Fill the shared memory ring buffer with zero at the mAlsaPrefill level
           *
           * Note: IasAudioRingBuffer::beginAccess() holds an internal mutex which is to be released by endAccess() call.
           * Therefore we can safely access the ring buffer from multiple threads i.e. ipcThread calling resetShmBuffer()
           * upon receiving the eIasAudioIpcStart message and AvbAlsaWrk serving the copyJob() processing.
           */

          (void) shmBuffer->zeroOut(); // fill the buffer with zero
          (void) shmBuffer->resetFromWriter(); // make the ring buffer empty

          if (shmBuffer->beginAccess(accessDirection, &shmAreas, &shmOffset, &shmFrames)) // lock
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "error in beginAccess of ring buffer");

            mBufferState = eRunning; // fail-safe
            mBufRstContCnt = 0u;
            mAlsaPrefilledSz = 0u;

            result = eIasAvbProcOK;
          }
          else
          {
            isLocked = true;

            if (nullptr == shmAreas)
            {
              shmFrames = 0u; // do not commit
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not get channel areas");
            }
            else
            {
              uint32_t writeOffset = shmBuffer->getWriteOffset();
              if (0u != writeOffset)  // writeOffset should be zero after reset
              {
                shmFrames = 0u; // do not update the offset
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Unable to reset ring buffer offset =", uint32_t(writeOffset));
              }
              else
              {
                // set the buffer fill level
                shmOffset = 0u;
                shmFrames = frames;
                if (shmFrames > (mParams->periodSize * mParams->numPeriods))
                {
                  shmFrames = (mParams->periodSize * mParams->numPeriods);

                }

                DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "SHM buffer state change",
                            uint32_t(mBufferState), "->", uint32_t(nextState), "fill-level/total =",
                            shmFrames, (mParams->periodSize * mParams->numPeriods));

                mBufferState = nextState; // change state atomically
                mBufRstContCnt = 0u;
                mAlsaPrefilledSz = 0u;
                result = eIasAvbProcOK; // done
              }
            }
          }
        }

        if (isLocked)
        {
          /*
           * Normally we should pass endAccess() the offset value returned by beginAccess(). However shmOffset should be
           * 0 here regardless of the beginAccess()'s returned value since the buffer has been reset by resetFromWriter().
           * And shmFrames should be (mParams->periodSize * mAlsaPrefill).
           */
          if (shmBuffer->endAccess(accessDirection, shmOffset, shmFrames))  // unlock
          {
            /**
             * @log Force or waive audio presentation
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error in endAccess of sink ring buffer");
            result = eIasAvbProcErr;
          }
        }
      }
    }
  }

  return result;
}


} // namespace

// EOF
