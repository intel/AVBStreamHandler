/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoReceiver.cpp
 * @brief   Implementation of the video receiver.
 * @details See header file for details.
 *
 * @date    2018
 */

#include <pthread.h>
#include "avb_video_bridge/IasAvbVideoReceiver.hpp"
#include "avb_video_common/IasAvbVideoStreaming.hpp"


namespace IasMediaTransportAvb {


std::atomic_int IasAvbVideoReceiver::mNumberInstances(0);

IasAvbVideoReceiver::IasAvbVideoReceiver(const char* receiverRole)
  : mReceiverRole(receiverRole)
  , mCallbackH264()
  , mCallbackMpegTS()
  , mIsRunning(false)
  , mWorkerThread(nullptr)
  , mShmConnection(false)
  , mRingBuffer(nullptr)
  , mRingBufferSize(0u)
  , mTimeout(500u)
  , mFormat(eFormatUnknown)
  , mMutex()
{
  mNumberInstances++;
}


IasAvbVideoReceiver::~IasAvbVideoReceiver()
{
  int32_t expected = 0;

  cleanup();

  if (mNumberInstances.compare_exchange_weak(expected, 0) == true)
  {
    // Error: no instance available
  }
  else
  {
    mNumberInstances--;
  }
}


ias_avbvideobridge_result IasAvbVideoReceiver::init()
{
  ias_avbvideobridge_result result = IAS_AVB_RES_OK;

  AVB_ASSERT(!mReceiverRole.empty());

  IasVideoCommonResult res = mShmConnection.init(mReceiverRole);
  result = (res != eIasResultOk ? IAS_AVB_RES_FAILED : IAS_AVB_RES_OK);

  if (IAS_AVB_RES_OK == result)
  {
    res = mShmConnection.findRingBuffer();
    result = (res != eIasResultOk ? IAS_AVB_RES_FAILED : IAS_AVB_RES_OK);
    if (IAS_AVB_RES_OK == result)
    {
      // Store pointer to ring buffer in order to have faster access on reception of data
      mRingBuffer = mShmConnection.getRingBuffer();
      result = (nullptr == mRingBuffer) ? IAS_AVB_RES_FAILED : IAS_AVB_RES_OK;
    }
  }

  if (IAS_AVB_RES_OK == result)
  {
    mRingBufferSize = mRingBuffer->getBufferSize();
    result = createThread();
  }

  return result;
}


void IasAvbVideoReceiver::cleanup()
{
  mIsRunning = false;

  if (nullptr != mWorkerThread)
  {
    // Cancel the wait for condition variable
    pthread_cancel(mWorkerThread->native_handle());
    mWorkerThread->join();

    delete mWorkerThread;
    mWorkerThread = nullptr;
  }

  mFormat = eFormatUnknown;
  mRingBufferSize = 0u;

  mMutex.lock();
  mCallbackH264.clear();
  mCallbackMpegTS.clear();
  mMutex.unlock();
}


ias_avbvideobridge_result IasAvbVideoReceiver::createThread()
{
  ias_avbvideobridge_result result = IAS_AVB_RES_OK;

  // Create worker thread
  mIsRunning = true;
  mWorkerThread = new std::thread([this]{workerThread();});
  if (mWorkerThread == nullptr)
  {
    mIsRunning = false;
    result = IAS_AVB_RES_NULL_PTR;
  }
  else
  {
    pthread_setname_np(mWorkerThread->native_handle(), mReceiverRole.c_str());
  }

  return result;
}


void IasAvbVideoReceiver::workerThread()
{
  ias_avbvideobridge_result result = IAS_AVB_RES_OK;

  AVB_ASSERT(nullptr != mRingBuffer); // Worker thread is started after successful connection to ring buffer

  while (mIsRunning == true)
  {
    // Wait for incoming package
    IasVideoRingBufferResult vres = mRingBuffer->waitRead(1, mTimeout);
    if (vres == eIasRingBuffTimeOut)
    {
      result = IAS_AVB_RES_TIMEOUT;
    }
    else
    {
      uint32_t offset = 0u;
      uint32_t numPackets = 1u; // Copy one packet per time
      void *basePtr = nullptr;
      uint32_t numPacketsTransferred = 0u;

      if (eIasRingBuffOk != (vres = mRingBuffer->beginAccess(eIasRingBufferAccessRead, &basePtr, &offset, &numPackets)))
      {
        vres = mRingBuffer->endAccess(eIasRingBufferAccessRead, 0, 0);
        AVB_ASSERT(vres == eIasRingBuffOk);
        (void)vres;
        result = IAS_AVB_RES_FAILED;
      }
      else
      {
        // Calculation of the data position within the ring buffer
        uint8_t *dataPtr = static_cast<uint8_t*>(basePtr) + (offset * mRingBufferSize);

        if (eFormatH264 == mFormat) // H.264?
        {
          ias_avbvideobridge_buffer buffer;
          TransferPacketH264 *packetH264 = reinterpret_cast<TransferPacketH264*>(dataPtr);
          buffer.size = uint32_t(packetH264->size);
          buffer.data = &packetH264->data;

          // Pass data to application by calling its callback function
          mMutex.lock();
          if (!mCallbackH264.isUnregistered())
          {
            mCallbackH264.callback(reinterpret_cast<ias_avbvideobridge_receiver*>(this), &buffer, mCallbackH264.userPtr);
          }
          mMutex.unlock();
        }
        else if (eFormatMpegTs == mFormat)// MPEG_TS
        {
          ias_avbvideobridge_buffer buffer;
          TransferPacketMpegTS *packetMpegTs = reinterpret_cast<TransferPacketMpegTS*>(dataPtr);
          buffer.size = uint32_t(packetMpegTs->size);
          buffer.data = &packetMpegTs->data;

          // Pass data to application by calling its callback function
          mMutex.lock();
          if (!mCallbackMpegTS.isUnregistered())
          {
            mCallbackMpegTS.callback(reinterpret_cast<ias_avbvideobridge_receiver*>(this), packetMpegTs->sph, &buffer, mCallbackMpegTS.userPtr);
          }
          mMutex.unlock();
        }
        else
        {
          AVB_ASSERT(false); // H.264 and MpegTS is allowed!
        }

        numPacketsTransferred = numPackets;

        vres = mRingBuffer->endAccess(eIasRingBufferAccessRead, offset, numPacketsTransferred);
        AVB_ASSERT(vres == eIasRingBuffOk);
      }
    }
  } // While running
  (void) result;
}


ias_avbvideobridge_result IasAvbVideoReceiver::setCallback(ias_avbvideobridge_receive_H264_cb cb, void* userPtr)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_FAILED;

  mMutex.lock();
  if (nullptr != cb)
  {
    if (mCallbackH264.isUnregistered())
    {
      mCallbackH264.callback = cb;
      mCallbackH264.userPtr  = userPtr;
      if (eFormatMpegTs != mFormat)
      {
        mFormat = eFormatH264;
        res = IAS_AVB_RES_OK;
      }
      else
      {
        // Already registered with different format
        res = IAS_AVB_RES_FAILED;
      }
    }
    else
    {
      // Already registered, return an error
      res = IAS_AVB_RES_FAILED;
    }
  }
  else
  {
    res = IAS_AVB_RES_NULL_PTR;
  }
  mMutex.unlock();

  return res;
}


ias_avbvideobridge_result IasAvbVideoReceiver::setCallback(ias_avbvideobridge_receive_MpegTS_cb cb, void* userPtr)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_FAILED;

  mMutex.lock();
  if (nullptr != cb)
  {
    if (mCallbackMpegTS.isUnregistered())
    {
      mCallbackMpegTS.callback = cb;
      mCallbackMpegTS.userPtr  = userPtr;
      if (eFormatH264 != mFormat)
      {
        mFormat = eFormatMpegTs;
        res = IAS_AVB_RES_OK;
      }
      else
      {
        // Already registered with different format
        res = IAS_AVB_RES_FAILED;
      }
    }
    else
    {
      // Already registered, return an error
      res = IAS_AVB_RES_FAILED;
    }
  }
  else
  {
    res = IAS_AVB_RES_NULL_PTR;
  }
  mMutex.unlock();

  return res;
}

} // namespace IasMediaTransportAvb



