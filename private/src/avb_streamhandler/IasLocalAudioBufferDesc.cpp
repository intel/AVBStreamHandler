/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalAudioBufferDesc.cpp
 * @brief   Implementation of the audio buffer descriptor
 * @details
 * @date    2016
 */

#include "avb_streamhandler/IasLocalAudioBufferDesc.hpp"

namespace IasMediaTransportAvb {


/*
 *  Constructor.
 */
IasLocalAudioBufferDesc::IasLocalAudioBufferDesc(uint32_t qSize)
  : mLock()
  , mDescQ(0)
  , mDescQsz(qSize)
  , mResetRequest(false)
  , mDbgPresentationWarningTime(0u)
  , mAlsaRxSyncStart(false)
{
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaSyncRxReadStart, mAlsaRxSyncStart);
}

/*
 *  Destructor.
 */
IasLocalAudioBufferDesc::~IasLocalAudioBufferDesc()
{
  cleanup();
}

/*
 *  Cleanup method.
 */
void IasLocalAudioBufferDesc::cleanup()
{
  lock();

  mDescQ.clear();

  unlock();
}

void IasLocalAudioBufferDesc::enqueue(const IasLocalAudioBufferDesc::AudioBufferDesc &desc)
{
  lock();

  // put out the oldest descriptor if FIFO is full
  if (mDescQ.size() >= mDescQsz)
  {
    mDescQ.pop_back();
  }

  mDescQ.insert(mDescQ.begin() + 0, desc);

  unlock();
}

IasAvbProcessingResult IasLocalAudioBufferDesc::dequeue(IasLocalAudioBufferDesc::AudioBufferDesc &desc)
{
  IasAvbProcessingResult ret = eIasAvbProcErr;

  lock();

  if (mDescQ.empty() != true)
  {
    desc = mDescQ.back();

    mDescQ.pop_back();

    ret = eIasAvbProcOK;
  }

  unlock();

  return ret;
}

IasAvbProcessingResult IasLocalAudioBufferDesc::peekX(IasLocalAudioBufferDesc::AudioBufferDesc &desc, uint32_t index)
{
  IasAvbProcessingResult ret = eIasAvbProcErr;

  lock();

  const std::size_t qSize = mDescQ.size();

  if (index < qSize)
  {
    desc = mDescQ[(qSize - 1u) - index];
    ret = eIasAvbProcOK;
  }

  unlock();

  return ret;
}

} // namespace IasMediaTransportAvb
