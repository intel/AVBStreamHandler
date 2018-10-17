/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoInStream.cpp
 * @brief   Implementation of a video data container of an incoming video stream
 * @details See header file for details.
 *
 * @date    2018
 */

#include "avb_streamhandler/IasLocalVideoInStream.hpp"
#include "avb_streamhandler/IasLocalVideoBuffer.hpp"
#include "avb_video_common/IasAvbVideoStreaming.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_helper/ias_safe.h"
#include <dlt/dlt_cpp_extension.hpp>
#include <arpa/inet.h>
#include <cstdlib>
#include <sstream>
#include <iomanip>

#include <sys/syscall.h>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLocalVideoInStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):" + "(" + mIpcName + ")"


/*
 *  Constructor.
 */
IasLocalVideoInStream::IasLocalVideoInStream(DltContext &dltContext, uint16_t streamId)
  : IasLocalVideoStream(dltContext, IasAvbStreamDirection::eIasAvbTransmitToNetwork, eIasLocalVideoInStream, streamId)
  , mIpcName()
  , mShmConnection(true) // AVB SH is the creator
  , mRingBuffer(nullptr)
  , mRingBufferSize(0u)
  , mThread(nullptr)
  , mThreadIsRunning(false)
  , mOptimalFillLevel(0u)
  , mRtpSequNrLast(0u)
  , mTimeout(500u)
{
}


/*
 *  Destructor.
 */
IasLocalVideoInStream::~IasLocalVideoInStream()
{
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalVideoInStream::init(std::string ipcName, uint16_t maxPacketRate, uint16_t maxPacketSize,
                                                   IasAvbVideoFormat format, uint16_t numPackets,
                                                   uint32_t optimalFillLevel, bool internalBuffers)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "maxPacketSize=", maxPacketSize, "numPackets=", numPackets);

  if (isInitialized())
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
    result = IasLocalVideoStream::init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers);

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
        uint32_t size = maxPacketSize;
        size += static_cast<uint32_t>((IasAvbVideoFormat::eIasAvbVideoFormatIec61883 == format) ?
                sizeof(TransferPacketMpegTS) : sizeof(TransferPacketH264));
        IasVideoCommonResult res = mShmConnection.createRingBuffer(numPackets, size);
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

          mRingBuffer = mShmConnection.getRingBuffer();
          if (nullptr == mRingBuffer)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not get IPC ring buffer");
            mShmConnection.cleanup();
            IasLocalVideoStream::cleanup();
            result = eIasAvbProcInitializationFailed;
          }
          else
          {
            mRingBufferSize = mRingBuffer->getBufferSize();
            AVB_ASSERT(0u != mRingBufferSize);

            /* Create the worker thread */
            AVB_ASSERT(mThread == nullptr);
            mThread = new IasThread(this, ipcName);
            if (mThread == nullptr)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error while creating IasThread object");
              mRingBufferSize = 0u;
              mRingBuffer = nullptr;
              mShmConnection.cleanup();
              IasLocalVideoStream::cleanup();
              result = eIasAvbProcInitializationFailed;
            }
            else
            {
//              (void) mThread->setThreadName(mThread->getThreadId(), "test"); // TODO
              mIpcName = ipcName;
              mOptimalFillLevel = optimalFillLevel;
            }
          }
        }
      }
    }
  }

  return result;
}


/*
 *  Cleanup method.
 */
void IasLocalVideoInStream::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  stop();
  mIpcName.clear();
  if (nullptr != mThread)
  {
    if (mThread->isRunning())
    {
      mThread->stop();
    }
    delete mThread;
    mThread = nullptr;
  }

  mRingBuffer = nullptr;
  mRingBufferSize = 0u;
}


IasAvbProcessingResult IasLocalVideoInStream::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "***** start() has been called");

  if (mThread == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error due to non-initialized component");
    result =  eIasAvbProcNotInitialized;
  }
  else
  {
    mThreadIsRunning = true;
    mThread->start(true);
  }

  return result;
}


IasAvbProcessingResult IasLocalVideoInStream::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mThread == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error due to non-initialized component (mThread == nullptr)");
    result =  eIasAvbProcNotInitialized;
  }
  else
  {
    mThreadIsRunning = false;
    mThread->stop();
  }

  return result;
}


IasResult IasLocalVideoInStream::beforeRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  return IasResult::cOk;
}


IasResult IasLocalVideoInStream::run()
{
  IasResult result = IasResult::cOk;
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  pid_t tid = (pid_t)syscall(SYS_gettid);

  mRingBuffer->addReader(tid);
  while (mThreadIsRunning)
  {
    IasVideoRingBufferResult res = mRingBuffer->waitRead(tid, 1, mTimeout);
    if (res != eIasRingBuffTimeOut)
    {
      (void) copyJob(tid);
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "Timeout while waiting for data");
    }
  }
  mRingBuffer->removeReader(tid);

  return result;
}


IasResult IasLocalVideoInStream::shutDown()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  return IasResult::cOk;
}


IasResult IasLocalVideoInStream::afterRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  return IasResult::cOk;
}


IasAvbProcessingResult IasLocalVideoInStream::copyJob(pid_t tid)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(nullptr != mRingBuffer);
  AVB_ASSERT(0u != mRingBufferSize);
  {
    uint32_t offset = 0u;
    uint32_t numPackets = 1u; // copy one packet per time
    void *basePtr = nullptr;
    uint32_t numPacketsTransferred = 0u;

    IasVideoRingBufferResult res;
    if (eIasRingBuffOk != (res = mRingBuffer->beginAccess(eIasRingBufferAccessRead, tid, &basePtr, &offset, &numPackets)))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't acquire buffer for access!");
      res = mRingBuffer->endAccess(eIasRingBufferAccessRead, tid, 0, 0);
      AVB_ASSERT(result == eIasRingBuffOk);
      (void)res;
      result = eIasAvbProcErr;
    }
    else
    {
      // calculation of the data position within the ring buffer
      uint8_t *dataPtr = static_cast<uint8_t*>(basePtr) + (offset * mRingBufferSize);

      if (IasAvbVideoFormat::eIasAvbVideoFormatRtp == mFormat) // H.264?
      {
        PacketH264 packetH264;
        TransferPacketH264 *packet = reinterpret_cast<TransferPacketH264*>(dataPtr);
        packetH264.buffer.size = uint32_t(packet->size);
        packetH264.buffer.data = &packet->data;

        // Pass data to AVB stack
        eventOnNewPacketH264(packetH264);
      }
      else // MPEG_TS
      {
        PacketMpegTS packetMpegTs;
        TransferPacketMpegTS *packet = reinterpret_cast<TransferPacketMpegTS*>(dataPtr);
        packetMpegTs.buffer.size = uint32_t(packet->size);
        packetMpegTs.buffer.data = &packet->data;
        packetMpegTs.sph = packet->sph;

        // Pass data to AVB stack
        eventOnNewPacketMpegTS(packetMpegTs);
      }

      numPacketsTransferred++;

      res = mRingBuffer->endAccess(eIasRingBufferAccessRead, tid, offset, numPacketsTransferred);
      AVB_ASSERT(result == eIasRingBuffOk);
    }
  }

  return result;
}


void IasLocalVideoInStream::eventOnNewPacketH264(PacketH264 const &packet)
{
  if (mMaxPacketSize >= packet.buffer.size)
  {
    if (NULL != packet.buffer.data)
    {
      uint8_t  * rtpBase8   = static_cast<uint8_t *>(packet.buffer.data);
      uint16_t * rtpBase16  = static_cast<uint16_t *>(packet.buffer.data);
      uint32_t * rtpBase32  = static_cast<uint32_t *>(packet.buffer.data);

      uint16_t rtpSequenceNumberCurrent = 0u;

      IasLocalVideoBuffer::IasVideoDesc descPacket;

      descPacket.buffer.data = packet.buffer.data;
      descPacket.buffer.size = packet.buffer.size;
      descPacket.rtpSequenceNumber = ntohs(rtpBase16[1]);

      descPacket.rtpTimestamp = /*ntohl*/(rtpBase32[1]); // no ntohl needed because it is delivered in network
                                                         // byte order from the rtppay function

      // since we are here we cannot be IEC61883
      descPacket.isIec61883Packet = false;

      descPacket.mptField = rtpBase8[1]; //provide marker bit + payload type

      // check if extension header is used
      if(!(rtpBase8[0] & 0x10))
      {
        // no extension header
        descPacket.buffer.data = rtpBase8 + 12;
        descPacket.buffer.size = packet.buffer.size-12;
      }

      // sequence number check
      rtpSequenceNumberCurrent = descPacket.rtpSequenceNumber;
      if((rtpSequenceNumberCurrent != static_cast<uint16_t>(mRtpSequNrLast + 1)) && (mRtpSequNrLast != 0))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " SEQUENCE ERROR rtpSequenceNumber: ", rtpSequenceNumberCurrent,
                    "rtpSequenceNumberLast: ", mRtpSequNrLast);
      }
      mRtpSequNrLast=rtpSequenceNumberCurrent;

      if(NULL != getLocalVideoBuffer())
      {
        getLocalVideoBuffer()->writeH264(&descPacket);
      }
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "buffer.data is NULL!");
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "buffer size (", packet.buffer.size, ") exceeds configured max! (",
                mMaxPacketSize, ")");
  }
}


void IasLocalVideoInStream::eventOnNewPacketMpegTS(PacketMpegTS const &packet)
{
  uint32_t payloadTspWidth = cTspSize;

  if (packet.sph)
  {
    payloadTspWidth = cTSP_SIZE_SPH;
  }

  if (NULL != packet.buffer.data)
  {
    // don't allow packets that are not multiples of TSP size
    if (0 != (packet.buffer.size % payloadTspWidth))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "data size not multiple of TSP-size! Packet Rejected!!");
    }
    else
    {
      IasLocalVideoBuffer::IasVideoDesc descPacket;
      uint32_t packetOffset = 0u;
      uint32_t tspsInPayload = packet.buffer.size / payloadTspWidth;
      uint32_t maxTspsInAvbPacket = mMaxPacketSize / cTSP_SIZE_SPH;

      uint8_t  * packetBase8   = static_cast<uint8_t *>(packet.buffer.data);

      // if sph is in data we do not need to add sph header
      descPacket.hasSph = packet.sph;

      // since we are here we must be IEC61883
      descPacket.isIec61883Packet = true;

      if (0u < maxTspsInAvbPacket)
      {
        // write data in chunks of mMaxPackeSize first
        while (tspsInPayload > maxTspsInAvbPacket)
        {
          descPacket.buffer.data = packetBase8 + packetOffset;
          descPacket.buffer.size = maxTspsInAvbPacket * payloadTspWidth;
          packetOffset += maxTspsInAvbPacket * payloadTspWidth;
          tspsInPayload -= maxTspsInAvbPacket;
          if(NULL != getLocalVideoBuffer())
          {
            getLocalVideoBuffer()->writeMpegTS(&descPacket);
          }
        }
      }

      // what remains is (packet.buffer.size % mMaxPacketSize)
      if (0u != tspsInPayload)
      {
        descPacket.buffer.data = packetBase8 + packetOffset;
        descPacket.buffer.size = tspsInPayload * payloadTspWidth;

        if(NULL != getLocalVideoBuffer())
        {
          getLocalVideoBuffer()->writeMpegTS(&descPacket);
        }
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "buffer.data is NULL!");
  }
}


void IasLocalVideoInStream::updateBufferStatus()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);
}


IasAvbProcessingResult IasLocalVideoInStream::resetBuffers()
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if(NULL != mLocalVideoBuffer)
  {
    mLocalVideoBuffer->reset(0);
  }

  return error;
}

} // namespace IasMediaTransportAvb
