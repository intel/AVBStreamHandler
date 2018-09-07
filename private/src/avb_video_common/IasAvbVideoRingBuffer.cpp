/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBuffer.cpp
 * @brief   Implementation of the video ring buffer that is used to exchange video data via shared memory.
 * @details See header file for details.
 *
 * @date    2018
 */

#include "avb_video_common/IasAvbVideoRingBuffer.hpp"
#include "avb_video_common/IasAvbVideoRingBufferShm.hpp"


namespace IasMediaTransportAvb {


IasAvbVideoRingBuffer::IasAvbVideoRingBuffer()
  : mRingBufShm(nullptr)
  , mDataPtr(nullptr)
  , mIsShm(true)
  , mName("uninitialized")
{
  // Nothing to do here
}


IasAvbVideoRingBuffer::~IasAvbVideoRingBuffer()
{
  cleanup();
}


void IasAvbVideoRingBuffer::cleanup()
{
  // Nothing to do here
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::init(uint32_t bufferSize,
                                                       uint32_t numBuffers,
                                                       void* dataBuf,
                                                       bool shared,
                                                       IasAvbVideoRingBufferShm* ringBufShm)
{
  if (nullptr == ringBufShm || nullptr == dataBuf || 0u == numBuffers || 0u == bufferSize )
  {
    return eIasRingBuffInvalidParam;
  }
  mRingBufShm = ringBufShm;
  IasVideoRingBufferResult result = mRingBufShm->init(bufferSize, numBuffers, dataBuf, shared);

  if(result != eIasRingBuffOk)
  {
    return result;
  }

  mDataPtr = mRingBufShm->getDataBuffer();
  mIsShm = true; // At the moment always true

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::setup(IasAvbVideoRingBufferShm* ringBufShm)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == ringBufShm)
  {
    result = eIasRingBuffInvalidParam;
  }
  else
  {
    mRingBufShm = ringBufShm;

    uint32_t bufferSize = mRingBufShm->getBufferSize();
    AVB_ASSERT(bufferSize > 0u);
    (void) bufferSize;

    uint32_t numberBuffers = mRingBufShm->getNumberBuffers();
    AVB_ASSERT(numberBuffers > 0u);
    (void) numberBuffers;

    void* dataBuf = mRingBufShm->getDataBuffer();
    AVB_ASSERT(nullptr != dataBuf);
    mDataPtr = dataBuf;

    mIsShm = true; // At the moment always true

    // Placeholder for future extensions.
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::updateAvailable(IasRingBufferAccess access, uint32_t* numBuffers)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == numBuffers || eIasRingBufferAccessUndef == access)
  {
    result =  eIasRingBuffInvalidParam;
  }
  else if (nullptr == mRingBufShm)
  {
    result = eIasRingBuffNullPtrAccess;
  }
  else if (mIsShm)
  {
    result = mRingBufShm->updateAvailable(access, numBuffers);
  }
  else
  {
    result = eIasRingBuffNotAllowed; // only shm allowed
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::beginAccess(IasRingBufferAccess access, void **dataPtr, uint32_t* offset, uint32_t* numBuffers)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == offset || nullptr == numBuffers  || eIasRingBufferAccessUndef == access)
  {
    result = eIasRingBuffInvalidParam;
  }
  else if (nullptr == mRingBufShm)
  {
    result = eIasRingBuffNullPtrAccess;
  }
  else if (mIsShm)
  {
    *dataPtr = mDataPtr;
    result = mRingBufShm->beginAccess(access, offset, numBuffers);
  }
  else
  {
    result = eIasRingBuffNotAllowed; // only shm allowed
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::endAccess(IasRingBufferAccess access, uint32_t offset, uint32_t numBuffers)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (eIasRingBufferAccessUndef == access)
  {
    result = eIasRingBuffInvalidParam;
  }
  else if (nullptr == mRingBufShm)
  {
    result = eIasRingBuffNullPtrAccess;
  }
  else if (mIsShm)
  {
    result = mRingBufShm->endAccess(access, offset, numBuffers);
  }
  else
  {
    result = eIasRingBuffNotAllowed; // only shm allowed
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::waitRead(uint32_t numBuffers, uint32_t timeout_ms)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == mRingBufShm)
  {
    result = eIasRingBuffNullPtrAccess;
  }
  else if (mIsShm)
  {
    result = mRingBufShm->waitRead(numBuffers, timeout_ms);
  }
  else
  {
    result = eIasRingBuffNotAllowed;
  }

  return result;
}


IasVideoRingBufferResult IasAvbVideoRingBuffer::waitWrite(uint32_t numBuffers, uint32_t timeout_ms)
{
  IasVideoRingBufferResult result = eIasRingBuffOk;

  if (nullptr == mRingBufShm)
  {
    result = eIasRingBuffNullPtrAccess;
  }
  else if (mIsShm)
  {
    result = mRingBufShm->waitWrite(numBuffers, timeout_ms);
  }
  else
  {
    result = eIasRingBuffNotAllowed;
  }

  return result;
}


uint32_t IasAvbVideoRingBuffer::getReadOffset() const
{
  uint32_t result = 0u;

  if (nullptr == mRingBufShm)
  {
    AVB_ASSERT(false);
    result = 0u;
  }
  else if (mIsShm)
  {
    result = mRingBufShm->getReadOffset();
  }
  else
  {
    result = 0u;
  }

  return result;
}


uint32_t IasAvbVideoRingBuffer::getWriteOffset() const
{
  uint32_t result = 0u;

  if (nullptr == mRingBufShm)
  {
    AVB_ASSERT(false);
    result = 0u;
  }
  else if (mIsShm)
  {
    return mRingBufShm->getWriteOffset();
  }
  else
  {
    return 0u;
  }

  return result;
}


uint32_t IasAvbVideoRingBuffer::getBufferSize() const
{
  uint32_t result = 0u;

  if (nullptr == mRingBufShm)
  {
    AVB_ASSERT(false);
    result = 0u;
  }
  else if (mIsShm)
  {
    return mRingBufShm->getBufferSize();
  }
  else
  {
    return 0u;
  }

  return result;
}


void IasAvbVideoRingBuffer::resetFromWriter()
{
  if (nullptr == mRingBufShm)
  {
    AVB_ASSERT(false);
  }
  else if (mIsShm)
  {
    mRingBufShm->resetFromWriter();
  }
}


void IasAvbVideoRingBuffer::resetFromReader()
{
  if (nullptr == mRingBufShm)
  {
    AVB_ASSERT(false);
  }
  else if (mIsShm)
  {
    mRingBufShm->resetFromReader();
  }
}


void IasAvbVideoRingBuffer::zeroOut()
{
  if (nullptr == mRingBufShm)
  {
    AVB_ASSERT(false);
  }
  else if (mIsShm)
  {
    mRingBufShm->zeroOut();
  }
}

} // namespace IasMediaTransportAvb



