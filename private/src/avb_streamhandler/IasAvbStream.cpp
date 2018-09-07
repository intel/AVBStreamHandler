/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStream.cpp
 * @brief   The implementation of the IasAvbStream class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbStream.hpp"

#include "avb_streamhandler/IasAvbPacketPool.hpp"

#include <cstring>
#include <netinet/in.h>
#include <dlt/dlt_cpp_extension.hpp>


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbStream::IasAvbStream(DltContext &dltContext, const IasAvbStreamType streamType)
  : mDiag()
  , mCurrentAvbLockState(IasAvbClockDomain::eIasAvbLockStateInit)
  , mLog(&dltContext)
  , mStreamStateInternal( IasAvbStreamState::eIasAvbStreamInactive )
  , mStreamType( streamType )
  , mDirection() // intentionally undefined
  , mActive(false)
  , mStreamState(IasAvbStreamState::eIasAvbStreamInactive )
  , mAvbStreamId()
  , mTSpec(NULL)
  , mPacketPool(NULL)
  , mAvbClockDomain(NULL)
  , mDmac()
  , mSmac()
  , mVlanData(0u)
  , mPresentationTimeOffset(0u)
  , mPreconfigured(true)
{
  // yes, I know, the default ctor should have zeroed it already, but you never know.
  (void) std::memset( &mDmac, 0, cIasAvbMacAddressLength);
  (void) std::memset( &mSmac, 0, cIasAvbMacAddressLength);
}


/*
 *  Destructor.
 */
IasAvbStream::~IasAvbStream()
{
  /* typically, this call won't do anything anymore
   * since the derived class should already have called cleanup()
   * from its destructor
   */
  IasAvbStream::doCleanup();
}

/*final*/ void IasAvbStream::cleanup()
{
  // first make sure the stream is deactivated
  deactivate();

  // then clean up derived class
  derivedCleanup();

  // and finally, clean up our own stuff
  doCleanup();
}

/*final*/ void IasAvbStream::doCleanup()
 {
  mPresentationTimeOffset = 0u;
  mAvbClockDomain = NULL;

  delete mPacketPool;
  mPacketPool = NULL;

  delete mAvbStreamId;
  mAvbStreamId = NULL;

  delete mTSpec;
  mTSpec = NULL;
}

uint32_t IasAvbStream::incFramesTx()
{
  uint32_t framesTx = mDiag.getFramesTx();
  ++framesTx;
  mDiag.setFramesTx(framesTx);
  return framesTx;
}

IasAvbProcessingResult IasAvbStream::initCommon(const IasAvbTSpec & tSpec, const IasAvbStreamId & streamId,
                                                const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  avb_safe_result copyResult = avb_safe_memcpy(&mDmac, sizeof mDmac, &dmac, sizeof dmac);
  AVB_ASSERT(e_avb_safe_result_ok == copyResult);
  (void) copyResult;

  mTSpec = new (nothrow) IasAvbTSpec( tSpec );

  if (NULL == mTSpec)
  {
    /**
     * @log Not enough memory: Unable to create a new instance of Tspec.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate IasAvbTSpec!");
    ret = eIasAvbProcNotEnoughMemory;
  }

  if (eIasAvbProcOK == ret)
  {
    mPreconfigured = preconfigured;
    mVlanData = uint16_t( (tSpec.getVlanPriority() << 13) | vid );

    mAvbStreamId = new (nothrow) IasAvbStreamId( streamId );

    if (NULL == mAvbStreamId)
    {
      /**
       * @log Not enough memory: Unable to create a new instance of StreamId.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate IasAvbStreamId!");
      ret = eIasAvbProcNotEnoughMemory;
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error occurred! error=", int32_t(ret));
  }

  return ret;
}


IasAvbProcessingResult IasAvbStream::initTransmit(const IasAvbTSpec & tSpec, const IasAvbStreamId & streamId,
    uint32_t poolSize, IasAvbClockDomain * clockDomain, const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (isInitialized())
  {
    // already initialized
    ret = eIasAvbProcInitializationFailed;
  }
  else
  {
    mDirection = IasAvbStreamDirection::eIasAvbTransmitToNetwork;

    if ((NULL == clockDomain)
        || (0u == tSpec.getMaxFrameSize())
        || (0u == tSpec.getMaxIntervalFrames())
        || (0u == poolSize)
        )
    {
      ret = eIasAvbProcInvalidParam;
    }

#if defined(DIRECT_RX_DMA)
    if (eIasAvbProcOK == ret)
    {
      /*
       * When the direct RX feature is enabled, IasAvbPacketPool::init()
       * accepts to create a packet pool of which packetSize is up to
       * 2048 bytes. This size is required by the libigb driver for the
       * receive functionality. However at the same time it is possible
       * for users to create transmit streams that would have longer
       * packets than the allowed maximum. Added the following size
       * checking to prevent such misuse.
       */
      if (IasAvbStream::cMaxFrameSize < tSpec.getMaxFrameSize() + IasAvbTSpec::cIasAvbPerFrameOverhead)
      {
        ret = eIasAvbProcInvalidParam;
      }
    }
#endif /* DIRECT_RX_DMA */

    if (eIasAvbProcOK == ret)
    {
      const IasAvbMacAddress & sourceMac = *IasAvbStreamHandlerEnvironment::getSourceMac();
      setSmac(&sourceMac[0]);

      mAvbClockDomain = clockDomain;
      ret = initCommon( tSpec, streamId, dmac, vid, preconfigured );
    }


    if (eIasAvbProcOK == ret)
    {
      mPacketPool = new (nothrow) IasAvbPacketPool(*mLog);

      if (NULL == mPacketPool)
      {
        ret = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        ret = mPacketPool->init( tSpec.getMaxFrameSize() + IasAvbTSpec::cIasAvbPerFrameOverhead, poolSize );
      }
    }

    mPresentationTimeOffset = tSpec.getPresentationTimeOffset();

    if (eIasAvbProcOK != ret)
    {
      IasAvbStream::cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error occurred! error=", int32_t(ret));
    }
  }

  return ret;
}


IasAvbProcessingResult IasAvbStream::initReceive(const IasAvbTSpec & tSpec, const IasAvbStreamId & streamId,
    const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (isInitialized())
  {
    ret = eIasAvbProcInitializationFailed;
  }
  else
  {
    mDirection = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
    mStreamState = IasAvbStreamState::eIasAvbStreamInactive;
    mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;

    if (0u == tSpec.getMaxFrameSize())
    {
      ret = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == ret)
    {
      (void) std::memset( &mSmac, 0, cIasAvbMacAddressLength);
      ret = initCommon( tSpec, streamId, dmac, vid, preconfigured );
    }

    if (eIasAvbProcOK != ret)
    {
      IasAvbStream::cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error occurred! error=", int32_t(ret));
    }
  }

   return ret;
}


void IasAvbStream::changeStreamId(const IasAvbStreamId & newId)
{
  if (NULL == mAvbStreamId)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " no previous streamId to be replaced!");
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " replacing streamId",
        uint64_t(*mAvbStreamId), "by", (uint64_t(newId)));
    *mAvbStreamId = newId;
  }
}


IasAvbPacket* IasAvbStream::preparePacket(uint64_t nextWindowStart)
{
   IasAvbPacket* packet = NULL;

   if (isInitialized() && isTransmitStream())
   {
     AVB_ASSERT(NULL != mPacketPool);

     packet = mPacketPool->getPacket();

     if (NULL != packet)
     {
       if (!writeToAvbPacket(packet, nextWindowStart))
       {
         // packet preparation failed, dispose packet
         IasAvbPacketPool::returnPacket( packet );
         packet = NULL;
       }
     }
   }

   return packet;
}


void IasAvbStream::dispatchPacket(const void* packet, size_t length, uint64_t now)
{
   if (isInitialized() && isReceiveStream())
   {
     if (NULL != packet)
     {
       const uint8_t* avtpBase8   = reinterpret_cast<const uint8_t*>(packet);
       // If time stamp valid bit is set
       if (avtpBase8[1] & 0x01)
       {
         const uint16_t* avtpBase16 = reinterpret_cast<const uint16_t*>(avtpBase8);
         const uint32_t* avtpBase32 = reinterpret_cast<const uint32_t*>(avtpBase16);

         // Handle wrap case
         const int32_t delta = int32_t(ntohl(avtpBase32[3]) - static_cast<uint32_t>(now));

         // @@DIAG check for early/late packets and increment counters
         // Time stamp - now is -ve, packet is late.
         if (delta < 0)
         {
           mDiag.setLateTimestamp(mDiag.getLateTimestamp()+1);

         }
         // Time stamp is further than maxTransitTime in the future
         else if (delta > static_cast<int32_t>(mTSpec->getMaxTransitTime()))
         {
           mDiag.setEarlyTimestamp(mDiag.getEarlyTimestamp()+1);
         }
       }
     }
     readFromAvbPacket(packet, length);
   }
}


IasAvbProcessingResult IasAvbStream::resetPacketPool() const
{
  AVB_ASSERT( NULL != mPacketPool );
  return mPacketPool->reset();
}

// @@DIAG handle STREAM_INTERRUPTED counter in the two methods below during case error handling

void IasAvbStream::activate(bool isError)
{
  if (!mActive)
  {
    mActive = true;
    activationChanged();

    if (isError)
    {
      mDiag.setStreamInterrupted(mDiag.getStreamInterrupted()+1);
    }
  }
}


void IasAvbStream::deactivate(bool isError)
{
  if (mActive)
  {
    mActive = false;
    activationChanged();

    if (isError)
    {
      mDiag.setStreamInterrupted(mDiag.getStreamInterrupted()+1);
    }
  }
}


IasAvbProcessingResult IasAvbStream::hookClockDomain(IasAvbClockDomain * clockDomain)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (isTransmitStream())
    {
      result = eIasAvbProcErr;
    }
    else if ((NULL != clockDomain) && (NULL != mAvbClockDomain))
    {
      // note that it's possible to un-hook by providing NULL as clockDomain argument
      result = eIasAvbProcAlreadyInUse;
    }
    else
    {
      mAvbClockDomain = clockDomain;
    }
  }

  if (eIasAvbProcOK != result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error occurred! error=", int32_t(result));
  }

  return result;
}


} // namespace IasMediaTransportAvb
