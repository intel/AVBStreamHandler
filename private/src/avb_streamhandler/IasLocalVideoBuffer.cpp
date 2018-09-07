/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoBuffer.cpp
 * @brief   Implementation of the video ring buffer
 * @details
 * @date    2014
 */
#include <string.h>

#include "avb_streamhandler/IasLocalVideoBuffer.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_helper/ias_safe.h"

#include <cmath>
#include <cstdlib>
#include <dlt/dlt_cpp_extension.hpp>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLocalVideoBuffer::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasLocalVideoBuffer::IasLocalVideoBuffer()
  :mReadIndex(0u)
  ,mWriteIndex(0u)
  ,mReadCnt(0u)
  ,mWriteCnt(0u)
  ,mRtpSequNrLast(0u)
  ,mNumPacketsTotal(0u)
  ,mMaxPacketSize(0u)
  ,mMaxFillLevel(0u)
  ,mBufferState(eIasVideoBufferStateInit)
  ,mBufferStateLast(eIasVideoBufferStateInit)
  ,mInternalBuffers(false)
  ,mLock()
  ,mBuffer(NULL)
  ,mPool(NULL)
  ,mRing(NULL)
  ,mLastRead(0u)
  ,mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_LVB"))
  ,mMsgCount(0u)
  ,mMsgCountMax(0u)
  ,mLocalTimeLast(0u)
{
}


/*
 *  Destructor.
 */
IasLocalVideoBuffer::~IasLocalVideoBuffer()
{
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalVideoBuffer::init(uint16_t numPackets, uint16_t maxPacketSize, bool internalBuffers)
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  if (1 >= numPackets || 0 == maxPacketSize)
  {
    error = eIasAvbProcInvalidParam;
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " bad parameters numPackets:", numPackets,
                "maxPacketSize:", maxPacketSize);
  }

  if (eIasAvbProcOK == error)
  {
    if(true == internalBuffers)
    {
      // use an internal buffer
      mBuffer = new (nothrow) IasLocalVideoBuffer::VideoData[numPackets * maxPacketSize];
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " create local Video buffer size: ",
          (numPackets * maxPacketSize));

      if (NULL == mBuffer)
      {
        error = eIasAvbProcNotEnoughMemory;
      }
    }
    else
    {
      // use buffer from the avb packet pool (dma capable)
    }

    if (eIasAvbProcOK == error)
    {
      mRing = new (nothrow) IasVideoDesc[numPackets];

      if (NULL == mRing)
      {
        error = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        for (int32_t i = 0; i < numPackets; i++)
        {
          mRing[i].avbPacket=NULL;
          mRing[i].isIec61883Packet = false;
          mRing[i].hasSph = false;
          mRing[i].tspsInAvbPacket = 0u;
        }

        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " create local Video buffer packet ring numPackets: ",
            numPackets);

        mInternalBuffers = internalBuffers;
        mNumPacketsTotal = numPackets;
        mMaxPacketSize = maxPacketSize;
        mReadIndex = 0u;
        mWriteIndex = 0u;
      }
    }
  }

  return error;
}

/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalVideoBuffer::reset(uint32_t optimalFillLevel)
{
  IasAvbProcessingResult error = eIasAvbProcOK;
  (void)optimalFillLevel;

  if (NULL == mRing)
  {
    error = eIasAvbProcNotInitialized;
  }
  else
  {
    mLock.lock();

    for (uint32_t i = 0u; i < mNumPacketsTotal; i++)
    {
      if (NULL != mRing[i].avbPacket)
      {
        IasAvbPacketPool::returnPacket(mRing[i].avbPacket);
        mRing[i].avbPacket = NULL;
        mRing[i].isIec61883Packet = false;
        mRing[i].hasSph = false;
        mRing[i].tspsInAvbPacket = 0u;
      }
    }

    mReadIndex = 0u;
    mWriteIndex = 0u;

    mLock.unlock();
  }

  return error;
}

/*
 *  Write method for H.264.
 */
uint32_t IasLocalVideoBuffer::writeH264(IasVideoDesc * packet)
{
  uint32_t bytesWritten = 0u;
  uint16_t rtpSequenceNumberCurrent = 0u;
  avb_safe_result copyResult;

  if (!packet || !packet->buffer.data || 0 == packet->buffer.size || mMaxPacketSize < packet->buffer.size)
  {
   DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, " the input packet or its data is NULL, or the data size is zero (0. Nothing to write.");
  }
  else
  {
    if((NULL != mPool) || (NULL != mBuffer))
    {
      mLock.lock();

      if(getFillLevel()>mMaxFillLevel)
      {
        mMaxFillLevel = static_cast<uint16_t>(getFillLevel());
        if(getFillLevel() == (mNumPacketsTotal-1))
        {
           // Warning if buffer pool is full
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "ring buffer pool is full, mWriteIndex: ",
                      mWriteIndex, "mMaxFillLevel: ", mMaxFillLevel);
        }
      }

      // check of remaining write buffer space
      // @@TRICKY We need a 'hole' of 1 packet to be able to determine between whether the ring is empty or full.
      const uint32_t remaining = mNumPacketsTotal - getFillLevel() - 1;

      if(remaining > 0u)
      {
        bool increment = false;

        /* Always copy the ring descriptor */
        // copyResult = avb_safe_memcpy(&mRing[mWriteIndex], sizeof(IasVideoDesc), packet, sizeof(IasVideoDesc));
        // AVB_ASSERT(e_avb_safe_result_ok == copyResult);

        mRing[mWriteIndex].buffer.data = packet->buffer.data;
        mRing[mWriteIndex].buffer.size = packet->buffer.size;
        mRing[mWriteIndex].isIec61883Packet = packet->isIec61883Packet;
        mRing[mWriteIndex].mptField = packet->mptField;

        /* Now determine which memory mode we are dealing with */
        if(NULL != mBuffer)
        {
          mLock.unlock();
          DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Buffered memory model not supported!!!");
          return bytesWritten;
        }
        else
        {
          IasAvbPacket *avbPacket = mPool->getPacket();
          if (NULL != avbPacket)
          {
            int32_t headroom = int32_t((avbPacket->getHomePool()->getPacketSize() - avbPacket->getPayloadOffset()) - packet->buffer.size);
            if (headroom < 0)
            {
              /**
               * @log The buffer size exceeds the available packet size. Cannot write to packet.
               */
              DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "WRITE ERROR ------- headroom = ", headroom);
            }
            else
            {
              copyResult = avb_safe_memcpy(avbPacket->getPayloadPointer(), packet->buffer.size,
                  packet->buffer.data, packet->buffer.size);
              if (e_avb_safe_result_ok != copyResult)
              {
                DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "Safe copy returned error!!!");
              }
              avbPacket->len = uint32_t(avbPacket->getPayloadOffset() + packet->buffer.size);
              bytesWritten = packet->buffer.size;

              mRing[mWriteIndex].avbPacket = avbPacket;
              mRing[mWriteIndex].rtpSequenceNumber = packet->rtpSequenceNumber;
              mRing[mWriteIndex].rtpTimestamp = packet->rtpTimestamp;
              mRing[mWriteIndex].buffer.size = 0u;   //data stored in avbPacket, so set buffer back to NULL
              mRing[mWriteIndex].buffer.data = NULL; //data stored in avbPacket, so set buffer back to NULL
              // rtp sequence number check
              rtpSequenceNumberCurrent = mRing[mWriteIndex].rtpSequenceNumber;
              if((rtpSequenceNumberCurrent != static_cast<uint16_t>(mRtpSequNrLast+1)) && (mRtpSequNrLast != 0))
              {
                DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "SEQUENCE ERROR rtpSequenceNumber: ",
                    rtpSequenceNumberCurrent,
                    "rtpSequenceNumberLast: ",
                    mRtpSequNrLast);
              }
              mRtpSequNrLast=rtpSequenceNumberCurrent;
            }
            increment = true;
          }
        }

        if (increment)
        {
          mWriteIndex++;

          if (mMaxFillLevel == mNumPacketsTotal-1)
          {
            // set back fillLevel, if in a running state again after the ring buffers went full(e.g. missing network link)
            mMaxFillLevel = 0u;
          }

  #if 0 // DEBUG OUTPUT
          // statistics: msgs/s
          struct timespec tp;
          (void) clock_gettime(CLOCK_MONOTONIC, &tp);
          uint64_t localTime = (uint64_t(tp.tv_sec) * uint64_t(1000000000u)) + tp.tv_nsec;
          if ((localTime-mLocalTimeLast)<=cObservationInterval) //default: 1s
          {
            mMsgCount++;
          }
          else
          {
            mLocalTimeLast=localTime;
            if(mMsgCount > mMsgCountMax)
            {
              mMsgCountMax=mMsgCount;
              //printf("IasLocalVideoBuffer::write: msgs/s max: %d \n",mMsgCountMax);
            }
            //printf("IasLocalVideoBuffer::write: msgs/s: %d, msgs/s max: %d \n",mMsgCount, mMsgCountMax);
            mMsgCount=0u;
          }

          if (0==(mWriteCnt%8000))
          {
             DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, " 8000 packets written, fillLevel: "),
                 DLT_UINT32(getFillLevel()),
                 DLT_STRING("mMaxFillLevel: "),
                 DLT_UINT16(mMaxFillLevel),
                 DLT_STRING("mMsgCountMax: "),
                 DLT_UINT16(static_cast<uint16_t>(mMsgCountMax))
                 );

             mWriteCnt=0u;
             mMaxFillLevel=0u;
          }
          mWriteCnt++;
  #endif

          if (mWriteIndex == mNumPacketsTotal)
          {
            mWriteIndex = 0u;
          }
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mWriteIndex: ", mWriteIndex, "  --------NO BUFFERS REMAINING ------");
      }

      mLock.unlock();
    }
  }

  return bytesWritten;
}

/*
 *  Write method for MPEG TS.
 */
uint32_t IasLocalVideoBuffer::writeMpegTS(IasVideoDesc * packet)
{
  uint32_t bytesWritten = 0u;
  avb_safe_result copyResult;

  if (!packet || !packet->buffer.data || 0 == packet->buffer.size || mMaxPacketSize < packet->buffer.size)
  {
   DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "the input packet or its data is NULL, or the data size is zero (0). Nothing to write.");
  }
  else
  {
    if (NULL != mPool)
    {
      mLock.lock();

      if(getFillLevel()>mMaxFillLevel)
      {
        // mMaxFillLevel = static_cast<uint16_t>(getFillLevel());
        if(getFillLevel() == (mNumPacketsTotal-1))
        {
           // Warning if buffer pool is full
          DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "ring buffer pool is full, mWriteIndex: ",
                      mWriteIndex, "mMaxFillLevel: ", mMaxFillLevel);
        }
      }

      // check of remaining write buffer space
      // @@TRICKY We need a 'hole' of 1 packet to be able to determine between whether the ring is empty or full.
      const uint32_t remaining = mNumPacketsTotal - getFillLevel() - 1;

      if(remaining > 0u)
      {
        bool increment = false;

        /* Always copy the ring descriptor */
        // copyResult = avb_safe_memcpy(&mRing[mWriteIndex], sizeof(IasVideoDesc), packet, sizeof(IasVideoDesc));
        // AVB_ASSERT(e_avb_safe_result_ok == copyResult);

        mRing[mWriteIndex].buffer.data = packet->buffer.data;
        mRing[mWriteIndex].buffer.size = packet->buffer.size;
        mRing[mWriteIndex].isIec61883Packet = packet->isIec61883Packet;

        /* Now determine which memory mode we are dealing with */
        if(NULL != mBuffer)
        {
          mLock.unlock();
          DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Buffered memory model not supported!!!");
          return bytesWritten;
        }
        else
        {
          uint32_t sphSize = 4;
          uint32_t tspSize = 188;
          static IasAvbPacket *avbPacket = NULL;
          uint32_t maxTspsInAvbPacket = mMaxPacketSize / (sphSize + tspSize);
          bool recycling = false;

          if ( (0 < mWriteIndex)
                && (mNumPacketsTotal > mWriteIndex)
                && (NULL != mRing[mWriteIndex - 1].avbPacket)
                && (mRing[mWriteIndex - 1].isIec61883Packet)
                && (maxTspsInAvbPacket > mRing[mWriteIndex - 1].tspsInAvbPacket)
                // ensure 2 frame gap between reader and writer to avoid race condition
                && (2 < getFillLevel())
                && ((mNumPacketsTotal - 2) > getFillLevel()) )
          {
            recycling = true;
            avbPacket = mRing[mWriteIndex - 1].avbPacket;
          }
          else
          {
            avbPacket = mPool->getPacket();
            if (NULL != avbPacket)
            {
              mRing[mWriteIndex].tspsInAvbPacket = 0;
              mRing[mWriteIndex].avbPacket = avbPacket;
              // initially packet has header only
              avbPacket->len = uint32_t(avbPacket->getPayloadOffset());
            }
          }

          if (NULL != avbPacket)
          {
            int32_t headroom = int32_t((avbPacket->getHomePool()->getPacketSize() - avbPacket->getPayloadOffset()) - packet->buffer.size);
            if (headroom < 0)
            {
              /**
               * @log The buffer size exceeds the available packet size. Cannot write to packet.
               */
              DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "WRITE ERROR ------- headroom = ", headroom);
            }
            else
            {
              uint8_t * readPtr = static_cast<uint8_t *>(packet->buffer.data);
              uint8_t * writePtr = static_cast<uint8_t *>(avbPacket->getPayloadPointer());
              // if SHP is provided in payload bump up tsp size
              if (packet->hasSph)
              {
                tspSize += sphSize;
              }
              uint32_t tspCount = packet->buffer.size / tspSize;
              uint32_t tspsForNext = 0;

              // Advance writePtr by tspsInAvbPacket to support recycling
              // Unused AVB packets will have 0 tsps written
              if (recycling)
              {
                writePtr += mRing[mWriteIndex - 1].tspsInAvbPacket * (tspSize + sphSize);

                if (maxTspsInAvbPacket < tspCount + mRing[mWriteIndex -1].tspsInAvbPacket)
                {
                  tspsForNext = tspCount + mRing[mWriteIndex -1].tspsInAvbPacket - maxTspsInAvbPacket;
                  tspCount = maxTspsInAvbPacket - mRing[mWriteIndex -1].tspsInAvbPacket;
                }
              }

              // Work through each tsp in payload.
              // Request new AVB packet when current packet is full and advance mWriteIndex
              for (; tspCount > 0; tspCount --)
              {
                if (!packet->hasSph)
                {
                  writePtr += sphSize;
                  avbPacket->len += sphSize;
                  bytesWritten += sphSize;
                }
                copyResult = avb_safe_memcpy(writePtr, tspSize, readPtr, tspSize);
                if (e_avb_safe_result_ok != copyResult)
                {
                  DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "Safe copy returned error!!!");
                }
                readPtr  += tspSize;
                writePtr += tspSize;
                avbPacket->len += tspSize;
                bytesWritten += tspSize;

                if (!recycling)
                {
                  mRing[mWriteIndex].tspsInAvbPacket++;
                }
                else
                {
                  mRing[mWriteIndex - 1].tspsInAvbPacket++;
                }
              }
              if ( (recycling) && (tspsForNext) )
              {
                avbPacket = mPool->getPacket();
                if (NULL != avbPacket)
                {
                  mRing[mWriteIndex].tspsInAvbPacket = 0;
                  mRing[mWriteIndex].avbPacket = avbPacket;
                  avbPacket->len = uint32_t(avbPacket->getPayloadOffset());

                  writePtr = static_cast<uint8_t *>(avbPacket->getPayloadPointer());
                  for (; tspsForNext > 0; tspsForNext--)
                  {
                    if (!packet->hasSph)
                    {
                      writePtr += sphSize;
                      avbPacket->len += sphSize;
                      bytesWritten += sphSize;
                    }
                    copyResult = avb_safe_memcpy(writePtr, tspSize, readPtr, tspSize);
                    if (e_avb_safe_result_ok != copyResult)
                    {
                      DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "Safe copy returned error!!!");
                    }
                    readPtr  += tspSize;
                    writePtr += tspSize;
                    avbPacket->len += tspSize;
                    bytesWritten += tspSize;
                    mRing[mWriteIndex].tspsInAvbPacket++;
                  }
                  recycling = false;
                }
              }
            }

            if (!recycling)
            {
              mRing[mWriteIndex].avbPacket = avbPacket;
              mRing[mWriteIndex].rtpSequenceNumber = packet->rtpSequenceNumber;
              mRing[mWriteIndex].rtpTimestamp = packet->rtpTimestamp;
              mRing[mWriteIndex].buffer.size = 0u;   //data stored in avbPacket, so set buffer back to NULL
              mRing[mWriteIndex].buffer.data = NULL; //data stored in avbPacket, so set buffer back to NULL

              increment = true;
            }
          }
        }

        if (increment)
        {
          mWriteIndex++;

          if (mMaxFillLevel == mNumPacketsTotal-1)
          {
            // set back fillLevel, if in a running state again after the ring buffers went full(e.g. missing network link)
            mMaxFillLevel = 0u;
          }

          if (mWriteIndex == mNumPacketsTotal)
          {
            mWriteIndex = 0u;
          }
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mWriteIndex: ", mWriteIndex, "  --------NO BUFFERS REMAINING ------");
      }

      mLock.unlock();
    }
  }

  return bytesWritten;
}

/*
 *  Read method.
 */
uint32_t IasLocalVideoBuffer::read(void * buffer, IasVideoDesc * descPacket)
{
  uint32_t written = 0u;
  avb_safe_result copyResult;

  if((NULL != mPool) || (NULL != mBuffer))
  {
    mLock.lock();

    const uint32_t fill = getFillLevel();
    if (fill > 0u)
    {
      // in case of shm to AVB that's all you have to do, because the data is already written into the AvbPacket dma memory
      copyResult = avb_safe_memcpy(descPacket, sizeof(IasVideoDesc), &mRing[mReadIndex], sizeof(IasVideoDesc));
      AVB_ASSERT(e_avb_safe_result_ok == copyResult);

      mRing[mReadIndex].avbPacket = NULL;

      // in case of AVB to video shm
      // the mRing[mReadIndex].buffer.data points to an mBuffer location
      if((NULL != buffer) && (NULL != mBuffer))
      {
        copyResult = avb_safe_memcpy(buffer, mRing[mReadIndex].buffer.size, mRing[mReadIndex].buffer.data, mRing[mReadIndex].buffer.size);
        AVB_ASSERT(e_avb_safe_result_ok == copyResult);
        (void) copyResult;

        descPacket->buffer.data = buffer;
        written = mRing[mReadIndex].buffer.size;
      }

      mReadIndex++;
      if (mReadIndex == mNumPacketsTotal)
      {
        mReadIndex = 0u;
      }
    }
    else
    {
    }

    mLock.unlock();
  }

  return written;
}

/*
 *  Cleanup method.
 */
void IasLocalVideoBuffer::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  delete[] mBuffer;
  mBuffer = NULL;

  delete[] mRing;
  mRing = NULL;
}


} // namespace IasMediaTransportAvb
