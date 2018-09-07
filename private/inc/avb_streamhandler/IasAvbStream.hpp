/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStream.hpp
 * @brief   The definition of the IasAvbStream class.
 * @details This is the base class for IasAvbAudioStream.
 * @date    2013
 */

#ifndef IASAVBSTREAM_HPP_
#define IASAVBSTREAM_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAvbStreamId.hpp"
#include "avb_streamhandler/IasAvbTSpec.hpp"
#include "avb_helper/ias_safe.h"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"

#include "avb_streamhandler/IasAvbClockDomain.hpp"

#if defined(DIRECT_RX_DMA)
#include <linux/if_ether.h>
#endif /* DIRECT_RX_DMA */

namespace IasMediaTransportAvb {

class IasAvbPacket;
class IasAvbPacketPool;
class IasAvbClockDomain;

class IasAvbStream
{
  public:

    /**
     *  @brief Constructor.
     */
    IasAvbStream(DltContext &dltContext, const IasAvbStreamType streamType);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbStream();

    // do not override; implement derivedCleanup() instead
    /*final*/ void cleanup();

    inline IasAvbStreamType getStreamType() const;
    inline bool isInitialized() const;
    inline bool isTransmitStream() const;
    inline bool isReceiveStream() const;
    inline IasAvbStreamDirection getDirection() const;
    inline bool isActive() const;
    inline const IasAvbStreamId & getStreamId() const;
    void changeStreamId(const IasAvbStreamId & newId);
    inline const IasAvbTSpec & getTSpec() const;
    inline IasAvbClockDomain* getClockDomain() const;
    inline const IasAvbMacAddress & getDmac() const;
    inline const IasAvbMacAddress & getSmac() const;
    inline void setSmac(const uint8_t * sMac);
    inline IasAvbStreamState getStreamState() const;
    IasAvbProcessingResult resetPacketPool() const;

    virtual void dispatchPacket(const void* packet, size_t length, uint64_t now);
    virtual IasAvbPacket* preparePacket(uint64_t nextWindowStart);
    void activate(bool isError=false);
    void deactivate(bool isError=false);
    IasAvbProcessingResult hookClockDomain(IasAvbClockDomain * clockDomain);

  private:

#if defined(DIRECT_RX_DMA)
    static const uint16_t cMaxFrameSize = ETH_FRAME_LEN + 4u; // consider VLAN TAG
#endif /* DIRECT_RX_DMA */

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbStream(IasAvbStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbStream& operator=(IasAvbStream const &other);

  protected:

    IasAvbProcessingResult initTransmit(const IasAvbTSpec & tSpec, const IasAvbStreamId & streamId, uint32_t poolSize,
                                        IasAvbClockDomain * clockDomain, const IasAvbMacAddress & dmac, uint16_t vid,
                                        bool preconfigured);
    IasAvbProcessingResult initReceive(const IasAvbTSpec & tSpec, const IasAvbStreamId & streamId,
                                       const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured);

    virtual void readFromAvbPacket(const void* packet, size_t length) = 0;
    virtual bool writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart) = 0;
    virtual void activationChanged() {}
    virtual void derivedCleanup() = 0;

    inline IasAvbPacketPool & getPacketPool() const;
    inline uint16_t getVlanData() const;
    inline uint32_t getPresentationTimeOffset() const;
    inline uint32_t adjustPresentationTimeOffset(uint32_t stepWidth);
    inline void setStreamState( IasAvbStreamState newState );

  private:
    //Helpers
    IasAvbProcessingResult initCommon(const IasAvbTSpec & tSpec, const IasAvbStreamId & streamId,
                                      const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured);
    void doCleanup();

  public:
    inline const IasAvbStreamDiagnostics& getDiagnostics() const { return mDiag; }
    inline const bool& getPreconfigured() const { return mPreconfigured; }

  protected:
    friend class IasAvbTransmitSequencer;
    IasAvbStreamDiagnostics mDiag;
    IasAvbClockDomain::IasAvbLockState mCurrentAvbLockState;

  public:
    uint32_t incFramesTx();
    const IasAvbClockDomain* getAvbClockDomain() const { return mAvbClockDomain; }

    // Members
  protected:
    DltContext              *mLog;
    IasAvbStreamState       mStreamStateInternal;
  private:
    const IasAvbStreamType  mStreamType;
    IasAvbStreamDirection   mDirection;
    bool                    mActive;
    IasAvbStreamState       mStreamState;
    IasAvbStreamId*         mAvbStreamId;
    IasAvbTSpec*            mTSpec;
    IasAvbPacketPool *      mPacketPool;
    IasAvbClockDomain *     mAvbClockDomain;
    IasAvbMacAddress        mDmac;
    IasAvbMacAddress        mSmac;
    uint16_t                  mVlanData;
    uint32_t                  mPresentationTimeOffset;
    bool                    mPreconfigured;
};


inline IasAvbStreamType IasAvbStream::getStreamType() const
{
  return mStreamType;
}


inline bool IasAvbStream::isInitialized() const
{
    return (NULL != mTSpec);
}


inline bool IasAvbStream::isTransmitStream() const
{
    return (mDirection == IasAvbStreamDirection::eIasAvbTransmitToNetwork);
}


inline bool IasAvbStream::isReceiveStream() const
{
  return (mDirection == IasAvbStreamDirection::eIasAvbReceiveFromNetwork);
}

inline IasAvbStreamDirection IasAvbStream::getDirection() const
{
  return mDirection;
}

inline bool IasAvbStream::isActive() const
{
  return mActive;
}


inline const IasAvbStreamId & IasAvbStream::getStreamId() const
{
  AVB_ASSERT( NULL != mAvbStreamId );
  return *mAvbStreamId;
}


inline const IasAvbTSpec & IasAvbStream::getTSpec() const
{
  AVB_ASSERT( NULL != mTSpec );
  return *mTSpec;
}


inline IasAvbClockDomain* IasAvbStream::getClockDomain() const
{
  return mAvbClockDomain;
}


inline const IasAvbMacAddress & IasAvbStream::getDmac() const
{
  return mDmac;
}

inline const IasAvbMacAddress & IasAvbStream::getSmac() const
{
  return mSmac;
}

inline IasAvbPacketPool & IasAvbStream::getPacketPool() const
{
  AVB_ASSERT( NULL != mPacketPool );
  return *mPacketPool;
}


inline uint16_t IasAvbStream::getVlanData() const
{
  return mVlanData;
}


inline uint32_t IasAvbStream::getPresentationTimeOffset() const
{
  return mPresentationTimeOffset;
}

inline uint32_t IasAvbStream::adjustPresentationTimeOffset(uint32_t stepWidth)
{
  uint32_t steps = 0u;

  if (0u != stepWidth)
  {
    steps = (mPresentationTimeOffset + stepWidth - 1u) / stepWidth;
    mPresentationTimeOffset = stepWidth * steps;
  }
  return steps;
}


inline IasAvbStreamState IasAvbStream::getStreamState() const
{
  return mStreamState;
}


inline void IasAvbStream::setStreamState(IasAvbStreamState newState)
{
  mStreamState = newState;
}


inline void IasAvbStream::setSmac(const uint8_t * sMac)
{
  avb_safe_result copyResult = avb_safe_memcpy(&mSmac, sizeof mSmac, sMac, cIasAvbMacAddressLength);
  AVB_ASSERT(e_avb_safe_result_ok == copyResult);
  (void) copyResult;
}

} // namespace IasMediaTransportAvb

#endif /* IASAVBSTREAM_HPP_ */
