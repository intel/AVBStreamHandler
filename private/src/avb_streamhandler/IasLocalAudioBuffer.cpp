/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalAudioBuffer.cpp
 * @brief   Implementation of the audio ring buffer
 * @details
 * @date    2013
 */
#include <string.h>

#include "avb_streamhandler/IasLocalAudioBuffer.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/ias_safe.h"
#include <dlt/dlt_cpp_extension.hpp>

#include <cmath>
#include <cstdlib>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLocalAudioBuffer::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

IasLocalAudioBuffer::DiagData::DiagData()
  : numOverrun(0u)
  , numUnderrun(0u)
  , numOverrunTotal(0u)
  , numUnderrunTotal(0u)
  , numReset(0u)
{
  // do nothing
}

/*
 *  Constructor.
 */
IasLocalAudioBuffer::IasLocalAudioBuffer()
  : mReadIndex(0u)
  , mWriteIndex(0u)
  , mReadCnt(0u)
  , mWriteCnt(0u)
  , mTotalSize(0u)
  , mReferenceFill(0u)
  , mBufferState(eIasAudioBufferStateInit)
  , mBufferStateLast(eIasAudioBufferStateInit)
  , mDoAnalysis(0u)
  , mReadIndexLastWriteCall(0u)
  , mLock()
  , mBuffer(NULL)
  , mLastRead(0u)
  , mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_LAB"))
  , mReadReady(false)
  , mReadThreshold(0u)
  , mMonotonicReadIndex(0u)
  , mMonotonicWriteIndex(0u)
{
}


/*
 *  Destructor.
 */
IasLocalAudioBuffer::~IasLocalAudioBuffer()
{
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalAudioBuffer::init(uint32_t totalSize, bool doAnalysis)
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  mTotalSize  = totalSize;
  mDoAnalysis = doAnalysis;
  mBuffer = new (nothrow) IasLocalAudioBuffer::AudioData[mTotalSize];

  if (NULL == mBuffer)
  {
    error = eIasAvbProcNotEnoughMemory;
  }

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " create local audio buffer size: ", mTotalSize);

  return error;
}

/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalAudioBuffer::reset(uint32_t optimalFillLevel)
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  mLock.lock();

  //initialization of the read and write pointer of the ring buffer
  uint32_t newReadIndex = mWriteIndex - optimalFillLevel;

  if (newReadIndex > mTotalSize)
  {
    // newReadIndex is effectively negative, wrap back into positive range
    newReadIndex += mTotalSize;
  }

  if (getFillLevel() < optimalFillLevel)
  {
    // not enough samples in buffer, add zeros
    if (newReadIndex < mReadIndex)
    {
      (void) memset(mBuffer + newReadIndex, 0, (mReadIndex - newReadIndex) * sizeof (AudioData));
    }
    else
    {
      (void) memset(mBuffer + newReadIndex, 0, (mTotalSize - newReadIndex) * sizeof (AudioData));
      (void) memset(mBuffer, 0, mReadIndex * sizeof (AudioData));
    }
  }

  mReadIndex = newReadIndex;

  mBufferState = eIasAudioBufferStateOk;
  mBufferStateLast = eIasAudioBufferStateOk;

  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " resets the local audio buffer. TotalSize=",
      mTotalSize, ", optimalFillLevel=", optimalFillLevel);

  // reset reference, will be set to new value upon first write
  mReferenceFill = 0u;

  // reset diagnostic counters
  mDiagData.numOverrun  = 0u;
  mDiagData.numUnderrun = 0u;
  mDiagData.numReset++;

  mReadReady = false;
  mMonotonicReadIndex  = 0u;
  mMonotonicWriteIndex = 0u;

  mLock.unlock();

  return error;
}

/*
 *  Write method.
 */
uint32_t IasLocalAudioBuffer::write(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples)
{
  avb_safe_result copyResult;
  size_t size;
  uint32_t samplesWritten = 0u;

  mLock.lock();

  // check of remaining write buffer space
  const uint32_t remaining = mTotalSize - getFillLevel() - 1u;
  if(nrSamples > remaining)
  {
    mDiagData.numOverrun++;
    mDiagData.numOverrunTotal++;
    nrSamples = remaining;
  }

  samplesWritten = nrSamples;

  // check of remaining write buffer space
  const uint32_t beforeWrap = mTotalSize - mWriteIndex;

  if (nrSamples > beforeWrap)
  {
    size = beforeWrap * sizeof (AudioData);
    copyResult = avb_safe_memcpy(mBuffer + mWriteIndex, size, buffer, size);
    AVB_ASSERT(e_avb_safe_result_ok == copyResult);

    buffer     += beforeWrap;
    nrSamples  -= beforeWrap;
    mWriteIndex = 0u;
  }

  size = nrSamples * sizeof (AudioData);
  copyResult = avb_safe_memcpy(mBuffer + mWriteIndex, size, buffer, size);
  AVB_ASSERT(e_avb_safe_result_ok == copyResult);
  (void) copyResult;

  mWriteIndex += nrSamples;

  // if reference has been reset, set it now
  if (0u == mReferenceFill)
  {
    mReferenceFill = getFillLevel();
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " new reference fill:", mReferenceFill);
  }

  mMonotonicWriteIndex += samplesWritten;

  if ((false == mReadReady) && (getFillLevel() >= mReadThreshold))
  {
    mReadReady = true;
  }

  mLock.unlock();
  return samplesWritten;
}

/*
 *  Write method with local iteration.
 */
uint32_t IasLocalAudioBuffer::write(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples, uint32_t stride)
{
  avb_safe_result copyResult;
  size_t size;
  uint32_t samplesWritten = 0u;

  mLock.lock();

  // check of remaining write buffer space
  const uint32_t remaining = mTotalSize - getFillLevel() - 1u;
  if(nrSamples > remaining)
  {
    mDiagData.numOverrun++;
    mDiagData.numOverrunTotal++;
    nrSamples = remaining;
  }

  samplesWritten = nrSamples;

  // check of remaining write buffer space
  uint32_t beforeWrap = mTotalSize - mWriteIndex;

  if (sizeof(IasLocalAudioBuffer::AudioData) == stride)  // not interleaved
  {
    if (nrSamples > beforeWrap)
    {
      size = beforeWrap * sizeof (AudioData);
      copyResult = avb_safe_memcpy(mBuffer + mWriteIndex, size, buffer, size);
      AVB_ASSERT(e_avb_safe_result_ok == copyResult);

      buffer     += beforeWrap;
      nrSamples  -= beforeWrap;
      mWriteIndex = 0u;
    }

    size = nrSamples * sizeof (AudioData);
    copyResult = avb_safe_memcpy(mBuffer + mWriteIndex, size, buffer, size);
    AVB_ASSERT(e_avb_safe_result_ok == copyResult);
    (void) copyResult;

    mWriteIndex += nrSamples;

    // if reference has been reset, set it now
    if (0u == mReferenceFill)
    {
      mReferenceFill = getFillLevel();
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " new reference fill:", mReferenceFill);
    }
  }
  else    // interleaved
  {
    for (uint32_t sample = 0u; sample < nrSamples; sample++)
    {
      *(mBuffer + mWriteIndex) = *buffer;
      buffer += stride/sizeof(IasLocalAudioBuffer::AudioData);
      beforeWrap = mTotalSize - mWriteIndex;

      if (1u == beforeWrap)
      {
        mWriteIndex = 0u;
      }
      else
      {
        mWriteIndex++;
      }

      // if reference has been reset, set it now
      if (0u == mReferenceFill)
      {
        mReferenceFill = getFillLevel();
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " new reference fill:", mReferenceFill);
      }
    }
  }

  mMonotonicWriteIndex += samplesWritten;

  if ((false == mReadReady) && (getFillLevel() >= mReadThreshold))
  {
    mReadReady = true;
  }

  mLock.unlock();
  return samplesWritten;
}

/*
 *  Read method.
 */
uint32_t IasLocalAudioBuffer::read(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples)
{
  avb_safe_result copyResult;
  size_t size;
  uint32_t samplesRead;

  mLock.lock();

  const uint32_t fill = getFillLevel();
  if (nrSamples > fill)
  {
    nrSamples = fill;
  }

  samplesRead = nrSamples;
  const uint32_t beforeWrap = mTotalSize - mReadIndex;

  if (nrSamples > beforeWrap)
  {
    size = beforeWrap * sizeof (AudioData);
    copyResult = avb_safe_memcpy(buffer, size, mBuffer + mReadIndex, size);
    AVB_ASSERT(e_avb_safe_result_ok == copyResult);

    buffer    += beforeWrap;
    nrSamples -= beforeWrap;
    mReadIndex = 0u;
  }

  size = nrSamples * sizeof (AudioData);
  copyResult = avb_safe_memcpy(buffer, size, mBuffer + mReadIndex, size);
  AVB_ASSERT(e_avb_safe_result_ok == copyResult);
  (void) copyResult;

  mReadIndex          += nrSamples;
  mMonotonicReadIndex += samplesRead;

  mLock.unlock();

  if(mDoAnalysis)
  {
    if((0 == (mReadCnt%32000)) || (samplesRead != mLastRead))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " mReadCnt=", mReadCnt,
          "mReadIndex=",  mReadIndex,
          "mWriteIndex=", mWriteIndex,
          "distance=",    fill,
          "state=",       int32_t(mBufferState),
          "numread=",     samplesRead);
      mLastRead = samplesRead;
    }
    mReadCnt++;
  }

  return samplesRead;
}

/*
 *  Read method with local iteration.
 */
uint32_t IasLocalAudioBuffer::read(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples, uint32_t stride)
{
  avb_safe_result copyResult;
  size_t size;

  mLock.lock();

  const uint32_t fill = getFillLevel();
  if (nrSamples > fill)
  {
    nrSamples = fill;
  }

  uint32_t samplesRead = nrSamples;
  uint32_t beforeWrap  = mTotalSize - mReadIndex;

  if (sizeof(IasLocalAudioBuffer::AudioData) == stride)      //not interleaved
  {
    if (nrSamples > beforeWrap)
    {
      size = beforeWrap * sizeof (AudioData);
      copyResult = avb_safe_memcpy(buffer, size, mBuffer + mReadIndex, size);
      AVB_ASSERT(e_avb_safe_result_ok == copyResult);

      buffer    += beforeWrap;
      nrSamples -= beforeWrap;
      mReadIndex = 0u;
    }

    size = nrSamples * sizeof (AudioData);
    copyResult = avb_safe_memcpy(buffer, size, mBuffer + mReadIndex, size);
    AVB_ASSERT(e_avb_safe_result_ok == copyResult);
    (void) copyResult;

    mReadIndex += nrSamples;

  }
  else    // interleaved
  {
    size = sizeof (AudioData);
    for (uint32_t sample = 0u; sample < nrSamples; sample++)
    {
      *buffer = *(mBuffer + mReadIndex);
      buffer += stride/sizeof(IasLocalAudioBuffer::AudioData);
      beforeWrap = mTotalSize - mReadIndex;
      if (1u == beforeWrap)
      {
        mReadIndex = 0;
      }
      else
      {
        mReadIndex++;
      }
    }
  }

  mMonotonicReadIndex += samplesRead;

  mLock.unlock();

  if(mDoAnalysis)
  {
    if((0 == (mReadCnt%32000)) || (samplesRead != mLastRead))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " mReadCnt=", mReadCnt,
          "mReadIndex=",  mReadIndex,
          "mWriteIndex=", mWriteIndex,
          "distance=",    fill,
          "state=",       int32_t(mBufferState),
          "numread=",     samplesRead);
      mLastRead = samplesRead;
    }
    mReadCnt++;
  }

  return samplesRead;
}

/*
 *  Cleanup method.
 */
void IasLocalAudioBuffer::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  delete[] mBuffer;
  mBuffer = NULL;
}


} // namespace IasMediaTransportAvb
