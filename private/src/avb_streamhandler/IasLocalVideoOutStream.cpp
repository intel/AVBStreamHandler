/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoOutStream.cpp
 * @brief   Implementation of a video data container of an outgoing video stream
 * @details See header file for details.
 *
 * @date    2018
 */

#include <dlt/dlt_cpp_extension.hpp>
#include <arpa/inet.h>
#include <cstdlib>
#include <sstream>
#include <iomanip>

#include "avb_video_common/IasAvbVideoShmConnection.hpp"
#include "avb_streamhandler/IasLocalVideoOutStream.hpp"
#include "avb_video_common/IasAvbVideoStreaming.hpp"
#include "avb_helper/ias_safe.h"

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLocalVideoOutStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"


/*
 *  Constructor.
 */
IasLocalVideoOutStream::IasLocalVideoOutStream(DltContext &dltContext, uint16_t streamId)
  : IasLocalVideoStream(dltContext, IasAvbStreamDirection::eIasAvbReceiveFromNetwork, eIasLocalVideoOutStream, streamId)
  , mIpcName()
  , mShmConnection(true) // AVB SH is the creator
  , mOptimalFillLevel(0u)
  , mRtpSequenceNumber(0u)
  , mWriteCount(0u)
{
}


/*
 *  Destructor.
 */
IasLocalVideoOutStream::~IasLocalVideoOutStream()
{
  cleanup();
}


/*
 *  Cleanup method.
 */
void IasLocalVideoOutStream::cleanup()
{
   DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

   mIpcName.clear();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalVideoOutStream::init(std::string ipcName, uint16_t maxPacketRate, uint16_t maxPacketSize,
                                                    IasAvbVideoFormat format, uint16_t numPackets,
                                                    uint32_t optimalFillLevel, bool internalBuffers)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, ipcName, "maxPacketSize=", maxPacketSize, "numPackets=", numPackets);

  if (!mIpcName.empty())
  {
    // already initalized
    result = eIasAvbProcInitializationFailed;
  }
  else if ((0u == maxPacketSize) || (ipcName.empty()))
  {
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    // numPackets == 0 because we do not need base class' local buffer, using own ring buffer in shared memory instead
    result = IasLocalVideoStream::init(format, 0, maxPacketRate, maxPacketSize, internalBuffers);

    if (eIasAvbProcOK == result)
    {
      std::string videoGroupName;
      IasVideoCommonResult res;
      if (!IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cVideoGroupName, videoGroupName))
      {
        res = mShmConnection.init(ipcName);
      }
      else
      {
        res = mShmConnection.init(ipcName, videoGroupName);
      }
      result = (res != eIasResultOk ? eIasAvbProcErr : eIasAvbProcOK);
      if (result != eIasAvbProcOK)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not create IPC!");
        IasLocalVideoStream::cleanup();
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        uint32_t ringBufferSize = maxPacketSize;
        ringBufferSize += static_cast<uint32_t>((IasAvbVideoFormat::eIasAvbVideoFormatIec61883 == format) ?
                sizeof(TransferPacketMpegTS) : sizeof(TransferPacketH264));
        IasVideoCommonResult res = mShmConnection.createRingBuffer(numPackets, ringBufferSize);
        result = (res != eIasResultOk ? eIasAvbProcErr : eIasAvbProcOK);
        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Creation of ring buffer failed!");
          mShmConnection.cleanup();
          IasLocalVideoStream::cleanup();
          result = eIasAvbProcInitializationFailed;
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Connection established, name =", ipcName);

          mIpcName = ipcName;
          mOptimalFillLevel = optimalFillLevel;
        }
      }
    }
  }

  return result;
}


IasAvbProcessingResult IasLocalVideoOutStream::writeLocalVideoBuffer(IasLocalVideoBuffer * buffer, IasLocalVideoBuffer::IasVideoDesc * descPacket)
{
  (void) buffer;
  IasAvbProcessingResult result = eIasAvbProcErr;
  PacketH264 packetH264;
  PacketMpegTS packetMpegTs;

  if (NULL != descPacket && NULL != descPacket->rtpPacketPtr)
  {
    uint8_t  * rtpBase8   = static_cast<uint8_t *>(descPacket->rtpPacketPtr);
    uint16_t * rtpBase16  = static_cast<uint16_t *>(descPacket->rtpPacketPtr);
    uint32_t * rtpBase32  = static_cast<uint32_t *>(descPacket->rtpPacketPtr);

    if (!descPacket->isIec61883Packet)	// assume H264
    {
      // restore RTP header
      rtpBase8[0] = 0x80; // RFC 1889 version(2)
      rtpBase8[1] = descPacket->mptField; //provide marker bit + payload type: hardcoded H264
      rtpBase16[1]= htons(descPacket->rtpSequenceNumber);
      rtpBase32[1]= /*htonl*/(descPacket->rtpTimestamp);
      rtpBase32[2]= htonl(0x4120db95); // SSRC hard-coded

      // copy pay-load to UFIP-RTP packet
      packetH264.buffer.data = rtpBase8;  // hand over filled rtp packet
      packetH264.buffer.size = descPacket->buffer.size + 12;

      if(0==(mWriteCount%8000))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "hand-over of 8000 received video packets to shm");
        mWriteCount=0u;
      }
      mWriteCount++;

      // hand-over
      result = pushPayload(packetH264);
    }
    else
    {
      // IEC61883 packet
      // pass data to shm
      packetMpegTs.sph = true;// no option yet to provide the data without the sph header.
      packetMpegTs.buffer.data = descPacket->buffer.data;
      packetMpegTs.buffer.size = descPacket->buffer.size;

      // hand-over
      result = pushPayload(packetMpegTs);
    }
  }

  return result;
}


IasAvbProcessingResult IasLocalVideoOutStream::pushPayload(PacketH264 const & packet)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  IasAvbVideoRingBuffer *ringBuffer = mShmConnection.getRingBuffer();
  if (nullptr == ringBuffer)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not get IPC ring buffer");
    result = eIasAvbProcErr;
  }
  else
  {
    uint32_t offset = 0u;
    uint32_t numPackets = 1u; // copy one packet per time
    void *basePtr = nullptr;
    uint32_t numPacketsTransferred = 0u;

    if (ringBuffer->beginAccess(eIasRingBufferAccessWrite, getpid(), &basePtr, &offset, &numPackets))  // numPackets returned here is the overall numbers of packets available for read or write, but not more than requested
    {
      // this could happen if multiple threads concurrently access the ring buffer in the same direction
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "error in beginAccess of ring buffer");
      result = eIasAvbProcErr;
    }
    else
    {
//      isLocked = true;
      DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE/*DLT_LOG_ERROR*/, LOG_PREFIX, "beginAccess() succeeded, numPackets=",
                  numPackets, " offset=", offset);

      if (0u != numPackets)
      {
        // copy the data packet
        uint8_t * dataPtr = static_cast<uint8_t*>(basePtr) + (offset * ringBuffer->getBufferSize());
        TransferPacketH264 *packetH264 = reinterpret_cast<TransferPacketH264*>(dataPtr);
        packetH264->size = packet.buffer.size;

        avb_safe_result copyResult = avb_safe_memcpy(&packetH264->data, packet.buffer.size, packet.buffer.data, packet.buffer.size);
        (void) copyResult;
        numPacketsTransferred++;
      }

      if (ringBuffer->endAccess(eIasRingBufferAccessWrite, getpid(), offset, numPacketsTransferred))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error in endAccess of sink ring buffer");
        result = eIasAvbProcErr;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "endAccess() succeeded, numPacketsTransferred =", numPacketsTransferred);
      }
    }
  }

  return result;
}


IasAvbProcessingResult IasLocalVideoOutStream::pushPayload(PacketMpegTS const & packet)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  IasAvbVideoRingBuffer *ringBuffer = mShmConnection.getRingBuffer();
  if (nullptr == ringBuffer)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not get IPC ring buffer");
    result = eIasAvbProcErr;
  }
  else
  {
    uint32_t offset = 0u;
    uint32_t numPackets = 1u; // copy one packet per time
    void *basePtr = nullptr;
    uint32_t numPacketsTransferred = 0u;

    if (ringBuffer->beginAccess(eIasRingBufferAccessWrite, getpid(), &basePtr, &offset, &numPackets))  // numPackets returned here is the overall numbers of packets available for read or write, but not more than requested
    {
      // this could happen if multiple threads concurrently access the ring buffer in the same direction
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "error in beginAccess of ring buffer");
      result = eIasAvbProcErr;
    }
    else
    {
//      isLocked = true;
      DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "beginAccess() succeeded, numPackets=",
                  numPackets, " offset=", offset);

      if (0u != numPackets)
      {
        // copy the data packet
        uint8_t * dataPtr = static_cast<uint8_t*>(basePtr) + (offset * ringBuffer->getBufferSize());
        TransferPacketMpegTS *packetMpegTs = reinterpret_cast<TransferPacketMpegTS*>(dataPtr);
        packetMpegTs->size = packet.buffer.size;
        packetMpegTs->sph = packet.sph;

        avb_safe_result copyResult = avb_safe_memcpy(&packetMpegTs->data, packet.buffer.size, packet.buffer.data, packet.buffer.size);
        (void) copyResult;
        numPacketsTransferred++;
      }

      if (ringBuffer->endAccess(eIasRingBufferAccessWrite, getpid(), offset, numPacketsTransferred))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error in endAccess of sink ring buffer");
        result = eIasAvbProcErr;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "endAccess() succeeded, numPacketsTransferred =", numPacketsTransferred);
      }
    }
  }

  return result;
}


void IasLocalVideoOutStream::updateBufferStatus()
{
}


IasAvbProcessingResult IasLocalVideoOutStream::resetBuffers()
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if(NULL != mLocalVideoBuffer)
  {
    mLocalVideoBuffer->reset(mOptimalFillLevel);
  }

  return error;
}


} // namespace IasMediaTransportAvb
