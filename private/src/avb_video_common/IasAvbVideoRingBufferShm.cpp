/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBufferShm.cpp
 * @brief   Implementation of the video ring buffer that is used to exchange video data via shared memory.
 * @details See header file for details.
 *
 * @date    2018
 */

#include "internal/audio/common/IasIntProcCondVar.hpp"
#include "internal/audio/common/IasIntProcMutex.hpp"
#include "avb_video_common/IasAvbVideoRingBufferShm.hpp"
#include "avb_helper/ias_debug.h"

// using IasLockGuard from audio/common
using IasAudio::IasLockGuard;


namespace IasMediaTransportAvb {


IasAvbVideoRingBufferShm::IasAvbVideoRingBufferShm()
  : mBufferSize(0u)
  , mNumBuffers(0u)
  , mReadOffset(0u)
  , mWriteOffset(0u)
  , mBufferLevel(0u)      //!< fill level in TODO: ??? bytes or packets
  , mShared(false)        //!< flag to indicate if the buffer is in shared memory
  , mInitialized(false)
  , mReadInProgress(false)
  , mWriteInProgress(false)
  , mDataBuf(nullptr)
  , mMutex()
  , mMutexReadInProgress()
  , mMutexWriteInProgress()
  , mCondRead()
  , mCondWrite()
  , mReadWaitLevel(0)
  , mWriteWaitLevel(0)
{
  // Nothing to do here
}

IasAvbVideoRingBufferShm::~IasAvbVideoRingBufferShm()
{
  // Nothing to do here
}

IasVideoRingBufferResult IasAvbVideoRingBufferShm::init(uint32_t packetSize, uint32_t numBuffers, void* dataBuf, bool shared)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;
  mShared = shared; // not used ATM

  if (0u == packetSize || 0u == numBuffers || nullptr == dataBuf)
  {
    result = eIasRingBuffInvalidParam;
  }
  else
  {
    mBufferSize = packetSize;
    mNumBuffers = numBuffers;
    mDataBuf = dataBuf;
    mInitialized = true;
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBufferShm::updateAvailable(IasRingBufferAccess access, uint32_t *numBuffers)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == numBuffers || eIasRingBufferAccessUndef == access)
  {
    result = eIasRingBuffInvalidParam;
  }
  else if (false == mInitialized)
  {
    result = eIasRingBuffNotInitialized;
  }
  else if (access == eIasRingBufferAccessRead)
  {
    *numBuffers = mBufferLevel;
  }
  else
  {
    *numBuffers = mNumBuffers - mBufferLevel;
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBufferShm::beginAccess(IasRingBufferAccess access, uint32_t* offset, uint32_t* numBuffers)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == offset || nullptr == numBuffers || eIasRingBufferAccessUndef == access)
  {
    result = eIasRingBuffInvalidParam;
  }
  else if (false == mInitialized)
  {
    result = eIasRingBuffNotInitialized;
  }
  else if (eIasRingBufferAccessRead == access)
  {
    if (mReadInProgress)
    {
      result = eIasRingBuffNotAllowed;
    }
    else
    {
      mReadInProgress.exchange(true);
      mMutexReadInProgress.lock();
      *offset = mReadOffset;

      if (*numBuffers > mBufferLevel)
      {
        *numBuffers = mBufferLevel;
      }
      if ((mReadOffset + *numBuffers) >= mNumBuffers)
      {
        *numBuffers = mNumBuffers - mReadOffset;
      }
    }
  }
  else // write access
  {
    if (mWriteInProgress)
    {
      result = eIasRingBuffNotAllowed;
    }
    else
    {
      mWriteInProgress.exchange(true);
      mMutexWriteInProgress.lock();
      *offset = mWriteOffset;

      if ((*numBuffers) > (mNumBuffers - mBufferLevel))
      {
        *numBuffers = mNumBuffers - mBufferLevel;
      }
      if ((mWriteOffset + *numBuffers) >= mNumBuffers)
      {
        *numBuffers = mNumBuffers - mWriteOffset;
      }
    }
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBufferShm::endAccess(IasRingBufferAccess access, uint32_t offset, uint32_t numBuffers)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;
  (void)offset;

  if (eIasRingBufferAccessUndef == access)
  {
    result = eIasRingBuffInvalidParam;
  }
  else if (eIasRingBufferAccessRead == access)
  {
    if (mReadInProgress)
    {
      if (static_cast<int32_t>(mBufferLevel - numBuffers) < 0)
      {
        result = eIasRingBuffInvalidParam;
      }
      else
      {
        IasLockGuard lock(&mMutex);
        if ((mReadOffset + numBuffers) == mNumBuffers)
        {
          mReadOffset = 0;
        }
        else if ((mReadOffset + numBuffers) > mNumBuffers)
        {
          return eIasRingBuffInvalidParam;
        }
        else
        {
          mReadOffset += numBuffers;
        }
        mBufferLevel -= numBuffers;

        mReadInProgress.exchange(false);
        mMutexReadInProgress.unlock();
        if (mBufferLevel <= mWriteWaitLevel)
        {
          mCondWrite.signal();
        }
      }
    }
  }
  else
  {
    if (mWriteInProgress)
    {
      if ((mBufferLevel + numBuffers) > mNumBuffers)
      {
        result = eIasRingBuffInvalidParam;
      }
      else
      {
        IasLockGuard lock(&mMutex);
        if ((mWriteOffset + numBuffers) == mNumBuffers)
        {
          mWriteOffset = 0;
        }
        else if ((mWriteOffset + numBuffers) > mNumBuffers)
        {
          return eIasRingBuffInvalidParam;
        }
        else
        {
          mWriteOffset += numBuffers;
        }
        mBufferLevel += numBuffers;

        mWriteInProgress.exchange(false);
        mMutexWriteInProgress.unlock();
        if (mBufferLevel >= mReadWaitLevel)
        {
          mCondRead.signal();
        }
      }
    }
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBufferShm::waitWrite(uint32_t numBuffers, uint32_t timeout_ms)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if ((numBuffers > mNumBuffers) || 0u == numBuffers || 0u == timeout_ms)
  {
    result = eIasRingBuffInvalidParam;
  }
  else
  {
    IasLockGuard lock(&mMutex);
    mWriteWaitLevel = mNumBuffers - numBuffers;
    IasIntProcCondVar::IasResult cndres = IasIntProcCondVar::eIasOk;
    if (mBufferLevel > mWriteWaitLevel)
    {
      cndres = mCondWrite.wait(mMutex, timeout_ms);
      if (cndres == IasIntProcCondVar::eIasTimeout)
      {
        result = eIasRingBuffTimeOut;
      }
      else if (cndres != IasIntProcCondVar::eIasOk)
      {
        result = eIasRingBuffCondWaitFailed;
      }
    }
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBufferShm::waitRead(uint32_t numBuffers, uint32_t timeout_ms)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if ((numBuffers > mNumBuffers) || 0u == numBuffers || 0u == timeout_ms)
  {
    result = eIasRingBuffInvalidParam;
  }
  else
  {
    IasLockGuard lock(&mMutex);
    mReadWaitLevel = numBuffers;
    IasIntProcCondVar::IasResult cndres = IasIntProcCondVar::eIasOk;
    if (mBufferLevel < mReadWaitLevel)
    {
      cndres = mCondRead.wait(mMutex, timeout_ms);
      if (cndres == IasIntProcCondVar::eIasTimeout)
      {
        result = eIasRingBuffTimeOut;
      }
      else if (cndres != IasIntProcCondVar::eIasOk)
      {
        result = eIasRingBuffCondWaitFailed;
      }
    }
  }

  return result;
}


void IasAvbVideoRingBufferShm::resetFromWriter()
{
  mMutexReadInProgress.lock();
  mReadOffset  = 0;
  mWriteOffset = 0;
  mBufferLevel = 0;
  mMutexReadInProgress.unlock();
}


void IasAvbVideoRingBufferShm::resetFromReader()
{
  mMutexWriteInProgress.lock();
  mReadOffset  = 0;
  mWriteOffset = 0;
  mBufferLevel = 0;
  mMutexWriteInProgress.unlock();
}


void IasAvbVideoRingBufferShm::zeroOut()
{
  // Lock both mutexes, to ensure nobody is accessing the buffer right now
  mMutexReadInProgress.lock();
  mMutexWriteInProgress.lock();
  uint32_t sizeOfBufferInBytes = mNumBuffers * mBufferSize;
  AVB_ASSERT(nullptr != getDataBuffer());
  memset(getDataBuffer(), 0, sizeOfBufferInBytes);
  mMutexWriteInProgress.unlock();
  mMutexReadInProgress.unlock();
}

} // namespace IasMediaTransportAvb



