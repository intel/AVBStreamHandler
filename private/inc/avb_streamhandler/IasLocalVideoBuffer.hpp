/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoBuffer.hpp
 * @brief   This class contains all methods to access the ring buffers
 * @details Each local Video stream handles its data via a
 *          separate ring buffer.
 *
 * @date    2014
 */

#ifndef IASLOCALVIDEOBUFFER_HPP_
#define IASLOCALVIDEOBUFFER_HPP_


#include "avb_streamhandler/IasAvbTypes.hpp"
#include "IasAvbPacketPool.hpp"
#include <cstring>

namespace IasMediaTransportAvb {


class IasLocalVideoBuffer
{
  public:
    typedef unsigned char VideoData;

    enum IasVideoBufferState
    {
      eIasVideoBufferStateInit = 0,
      eIasVideoBufferStateOk,
      eIasVideoBufferStateUnderrun,
      eIasVideoBufferStateOverrun,
    };
    /**
     *  @brief Constructor.
     */
    IasLocalVideoBuffer();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLocalVideoBuffer();


    struct IasVideoDesc
    {

      /*!
       * @brief mpegts - number of TSPs in AVB  packet
       */
      uint32_t tspsInAvbPacket;

      /*!
       *  @brief source packet header indicator.
       */
      bool hasSph;

      /*!
       *  @brief  Iec 61883 indicator.
       */
      bool isIec61883Packet;

      /*!
       * @brief presentation timestamp, in NS
       */
      uint64_t pts;

      /*!
       * @brief decoding timestamp, in NS
       */
      uint64_t dts;

      /*!
       * @brief rtp timestamp
       */
      uint32_t rtpTimestamp;

      /*!
       * @brief rtp sequence number
       */
      uint16_t rtpSequenceNumber;

      /*!
       * @brief rtp marker bit and payload type M|PT
       */
      uint8_t mptField;

      /*!
       * @brief rtp marker bit and payload type M|PT
       */
      void * rtpPacketPtr;

      /*!
       * @brief pointer to an avbPacket
       */
      IasAvbPacket *avbPacket;

      /*!
       * @brief the real payload
       */
      Buffer buffer;

      /*!
       * @brief default constructor
       */
      IasVideoDesc()
      : tspsInAvbPacket(0u)
      , hasSph(false)
      , isIec61883Packet(false)
      , pts(0u)
      , dts(0u)
      , rtpTimestamp(0u)
      , rtpSequenceNumber(0u)
      , mptField(0u)
      , rtpPacketPtr(NULL)
      , avbPacket(NULL)
      {
      }
    };

    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *
     *  @param[in] numPackets This has a minimum value of 2 as a packet is needed internally to separate the start
     *                        and end of the ring buffer.
     */
    IasAvbProcessingResult init(uint16_t numPackets, uint16_t maxPacketSize, bool internalBuffers);

    /**
     *  @brief Reset functionality for the channel buffers
     */
    IasAvbProcessingResult reset(uint32_t optimalFillLevel);

    /**
     *  @brief Writes H.264 data into the local ring buffer
     */
    uint32_t writeH264(IasLocalVideoBuffer::IasVideoDesc * packet);

    /**
     *  @brief Writes MPEG2-TS data into the local ring buffer
     */
    uint32_t writeMpegTS(IasLocalVideoBuffer::IasVideoDesc * packet);

    /**
     *  @brief Reads data from the local ring buffer
     */
    uint32_t read(void * buffer, IasVideoDesc * descPacket);

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief get current fill level
     */
    inline uint32_t getFillLevel() const;

    /**
     * @brief get maximum fill level (i.e. total size)
     */
    inline uint32_t getTotalSize() const;

    /**
     * @brief set the avbPacketPool pointer for payload pointer access
     */
    inline void setAvbPacketPool(IasAvbPacketPool * avbPacketPool);

    /**
     * @brief get info whether internal buffers or dma were used
     */
    inline bool getInternalBuffers() const;

  private:

    /**
     * @brief Constant used to define the measurement window for calculating packets/s value for debug
     */
    static const uint64_t cObservationInterval = 1000000000u;

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLocalVideoBuffer(IasLocalVideoBuffer const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLocalVideoBuffer& operator=(IasLocalVideoBuffer const &other);

    uint32_t   mReadIndex;
    uint32_t   mWriteIndex;
    uint32_t   mReadCnt;
    uint32_t   mWriteCnt;
    uint16_t   mRtpSequNrLast;
    uint32_t   mNumPacketsTotal;  //in samples (IasLocalVideoBuffer::VideoData)
    uint16_t   mNumPackets;
    uint16_t   mMaxPacketSize;
    uint16_t   mMaxFillLevel;
    IasVideoBufferState mBufferState;
    IasVideoBufferState mBufferStateLast;
    bool     mInternalBuffers;
    std::mutex mLock;
    unsigned char  *mBuffer;
    IasAvbPacketPool *mPool;
    IasVideoDesc *mRing;

    uint32_t   mLastRead;
    DltContext *mLog;

    uint32_t  mMsgCount;
    uint32_t  mMsgCountMax;
    uint64_t  mLocalTimeLast;
};


inline uint32_t IasLocalVideoBuffer::getFillLevel() const
{
  uint32_t ret = mWriteIndex - mReadIndex;

  if (ret > mNumPacketsTotal)
  {
    ret += mNumPacketsTotal;
  }

  return ret;
}

inline void IasLocalVideoBuffer::setAvbPacketPool(IasAvbPacketPool * avbPacketPool)
{
  (void) mLock.lock();
  mPool = avbPacketPool;
  (void) mLock.unlock();
}


inline uint32_t IasLocalVideoBuffer::getTotalSize() const
{
  return mNumPacketsTotal;
}

inline bool IasLocalVideoBuffer::getInternalBuffers() const
{
    return mBuffer == nullptr;
}


} // namespace IasMediaTransportAvb

#endif /* IASLOCALVIDEOBUFFER_HPP_ */
