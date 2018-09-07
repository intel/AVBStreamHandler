/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoSender.cpp
 * @brief   Implementation of the video sender.
 * @details See header file for details.
 * @date    2018
 */

#include "avb_video_bridge/IasAvbVideoSender.hpp"
#include "avb_video_common/IasAvbVideoStreaming.hpp"
#include "avb_helper/ias_safe.h"


namespace IasMediaTransportAvb {


std::atomic_int IasAvbVideoSender::mNumberInstances(0);

IasAvbVideoSender::IasAvbVideoSender(const char* senderRole)
  : mSenderRole(senderRole)
  , mShmConnection(false)
  , mRingBuffer(nullptr)
{
  mNumberInstances++;
}


IasAvbVideoSender::~IasAvbVideoSender()
{
  int32_t expected = 0;
  if (mNumberInstances.compare_exchange_weak(expected, 0) == true)
  {
    // Error: no instance available
  }
  else
  {
    mNumberInstances--;
  }
}


ias_avbvideobridge_result IasAvbVideoSender::init()
{
  ias_avbvideobridge_result result = IAS_AVB_RES_OK;

  // Create connection to shared memory
  AVB_ASSERT(!mSenderRole.empty());

  IasVideoCommonResult res = mShmConnection.init(mSenderRole);
  result = (res != eIasResultOk ? IAS_AVB_RES_FAILED : IAS_AVB_RES_OK);

  if (result == IAS_AVB_RES_OK)
  {
    res = mShmConnection.findRingBuffer();
    result = (res != eIasResultOk ? IAS_AVB_RES_FAILED : IAS_AVB_RES_OK);
    if (IAS_AVB_RES_OK == result)
    {
      // Store pointer to ring buffer in order to have faster access on sending data
      mRingBuffer = mShmConnection.getRingBuffer();
      result = (nullptr == mRingBuffer) ? IAS_AVB_RES_FAILED : IAS_AVB_RES_OK;
    }
  }

  return result;
}


ias_avbvideobridge_result IasAvbVideoSender::sendPacketH264(ias_avbvideobridge_buffer const * packet)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_OK;

  if ((nullptr == mRingBuffer) || (nullptr == packet))
  {
    res = IAS_AVB_RES_NULL_PTR;
  }
  else
  {
    uint32_t offset = 0u;
    uint32_t numPackets = 1u; // Copy one packet per time
    void *basePtr = nullptr;
    uint32_t numPacketsTransferred = 0u;
    const uint32_t bufferSize = mRingBuffer->getBufferSize(); // get the size of the buffer shm provides

    if (packet->size > bufferSize)
    {
      // payload too large for buffer provided by shm
      res = IAS_AVB_RES_PAYLOAD_TOO_LARGE;
    }
    else
    {
      IasVideoRingBufferResult result = mRingBuffer->beginAccess(eIasRingBufferAccessWrite, &basePtr, &offset, &numPackets);
      if (eIasRingBuffOk != result)
      {
        (void) mRingBuffer->endAccess(eIasRingBufferAccessWrite, offset, numPacketsTransferred);
        res = IAS_AVB_RES_NO_SPACE;
      }
      else
      {
        if (0u != numPackets)
        {
          // Copy the data packet to shared memory
          uint8_t * dataPtr = static_cast<uint8_t*>(basePtr) + (offset * bufferSize);
          TransferPacketH264 *packetH264 = reinterpret_cast<TransferPacketH264*>(dataPtr);
          packetH264->size = packet->size;
          avb_safe_result copyResult = avb_safe_memcpy(&packetH264->data, packet->size, packet->data, packet->size);
          (void) copyResult;
          numPacketsTransferred++;
        }

        if (mRingBuffer->endAccess(eIasRingBufferAccessWrite, offset, numPacketsTransferred))
        {
          res = IAS_AVB_RES_FAILED;
        }
      }
    }
  }

  return res;
}


ias_avbvideobridge_result IasAvbVideoSender::sendPacketMpegTs(bool sph, ias_avbvideobridge_buffer const * packet)
{
  ias_avbvideobridge_result res = IAS_AVB_RES_OK;

  if ((nullptr == mRingBuffer) || (nullptr == packet))
  {
    res = IAS_AVB_RES_NULL_PTR;
  }
  else
  {
    uint32_t offset = 0u;
    uint32_t numPackets = 1u; // Copy one packet per time
    void *basePtr = nullptr;
    uint32_t numPacketsTransferred = 0u;
    const uint32_t bufferSize = mRingBuffer->getBufferSize(); // get the size of the buffer shm provides

    if (packet->size > bufferSize)
    {
      // payload too large for buffer provided by shm
      res = IAS_AVB_RES_PAYLOAD_TOO_LARGE;
    }
    else
    {
      IasVideoRingBufferResult result = mRingBuffer->beginAccess(eIasRingBufferAccessWrite, &basePtr, &offset, &numPackets);
      if (eIasRingBuffOk != result)
      {
        (void) mRingBuffer->endAccess(eIasRingBufferAccessWrite, offset, numPacketsTransferred);
        res = IAS_AVB_RES_NO_SPACE;
      }
      else
      {
        if (0u != numPackets)
        {
          // Copy the data packet to shared memory
          uint8_t * dataPtr = static_cast<uint8_t*>(basePtr) + (offset * bufferSize);
          TransferPacketMpegTS *packetMpegTs = reinterpret_cast<TransferPacketMpegTS*>(dataPtr);
          packetMpegTs->size = packet->size;
          packetMpegTs->sph = sph;
          avb_safe_result copyResult = avb_safe_memcpy(&packetMpegTs->data, packet->size, packet->data, packet->size);
          (void) copyResult;
          numPacketsTransferred++;
        }

        if (mRingBuffer->endAccess(eIasRingBufferAccessWrite, offset, numPacketsTransferred))
        {
          return IAS_AVB_RES_FAILED;
        }
      }
    }
  }

  return res;
}


} // namespace IasMediaTransportAvb



