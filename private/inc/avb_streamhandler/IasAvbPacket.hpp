/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbPacket.hpp
 * @brief   The definition of the IasAvbPacket class.
 * @details This is an AVB packet that inherits from igb_packet C-struct in order
 *          to convert back and forth between the types without the need for ugly
 *          pointer arithmetics (offsetof, etc.)
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBPACKET_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBPACKET_HPP

#include "IasAvbTypes.hpp"
extern "C"
{
  #include "igb.h"
}

namespace IasMediaTransportAvb
{

class IasAvbPacketPool;

/*
 * We inherit from the igb_packet C-struct in order to convert back and forth between the types without
 * the need for ugly pointer arithmetics (offsetof, etc.)
 */
class IasAvbPacket : private igb_packet
{
  public:

    /**
     *  @brief Constructor.
     */
    IasAvbPacket();

    /**
     *  @brief Destructor, non-virtual as we don't have any virtual methods and we inherit from a C struct
     */
    /*non-virtual*/ ~IasAvbPacket();

    /**
     * @brief Assignment operator
     */
    IasAvbPacket& operator=(IasAvbPacket const &other);

    /**
     * @brief Retrieve pointer to the packet pool the packet belongs to.
     */
    inline IasAvbPacketPool* getHomePool() const;

    /**
     * @brief Sets the packet pool the packet belongs to. Can only be called once.
     */
    inline void setHomePool(IasAvbPacketPool* const homePool);

    /**
     * @brief checks if packet pointed to is a valid, initalized IasAvbPacket
     * Use this to double-check before down-casting from igb_packet to IasAvbPacket
     */
    inline bool isValid() const;

    /**
     * @brief set offset between start of packet and begin of payload
     * Used by AVB streams to indicate to the local streams where to put the payload
     * @param offset offset in bytes
     */
    inline void setPayloadOffset(size_t offset);

    /**
     * @brief return the payload offset (see @ref setPayloadOffset)
     */
    inline size_t getPayloadOffset() const;

    /**
     * @brief return the payload pointer according to the offset (see @ref setPayloadOffset)
     */
    inline void* getPayloadPointer() const;

    /**
     * @brief checks whether this packet is a dummy packet
     */
    inline bool isDummyPacket() const;

    /**
     * @brief turn this packet into a dummy packet
     */
    inline void makeDummyPacket();

    /**
     * get pointer to DMA memory for this packet
     */
    inline void* getBasePtr() const;

    /**
     * wrapper for igb_xmit call
     */
    inline int32_t xmit(device_t *dev, uint32_t queue_index);

    /**
     * expose those base class members as mutable that are safe to change
     */
    using igb_packet::attime;
    using igb_packet::len;

private:

    friend class IasAvbPacketPool;

    static const uint32_t cMagic = 0xFB210871u;

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbPacket(IasAvbPacket const &other);

    // Members
    IasAvbPacketPool* mHome;
    uint32_t mMagic;
    size_t mPayloadOffset;
    bool mDummyFlag;
};

inline void* IasAvbPacket::getBasePtr() const
{
  return this->vaddr;
}

inline int32_t IasAvbPacket::xmit(device_t *dev, uint32_t queue_index)
{
  return igb_xmit(dev, queue_index, this);
}

inline IasAvbPacketPool* IasAvbPacket::getHomePool() const
{
  return mHome;
}


inline void IasAvbPacket::setHomePool(IasAvbPacketPool* const homePool)
{
  if ((NULL != homePool) && (NULL == mHome))
  {
    mHome = homePool;
  }
}

inline bool IasAvbPacket::isValid() const
{
  return ((NULL != mHome) && (cMagic == mMagic));
}

inline void IasAvbPacket::setPayloadOffset(size_t offset)
{
  mPayloadOffset = offset;
}

inline size_t IasAvbPacket::getPayloadOffset() const
{
  return mPayloadOffset;
}

inline void* IasAvbPacket::getPayloadPointer() const
{
  return static_cast<void*>(static_cast<uint8_t*>(vaddr) + mPayloadOffset);
}

inline bool IasAvbPacket::isDummyPacket() const
{
  return mDummyFlag;
}

inline void IasAvbPacket::makeDummyPacket()
{
  mDummyFlag = true;
}


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBPACKET_HPP */
