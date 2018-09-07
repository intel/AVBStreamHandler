/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbPacketPool.hpp
 * @brief   The definition of the IasAvbPacketPool class.
 * @details This is the pool for AVB packets
 * @date    2013
 */

#ifndef IASAVBPACKETPOOL_HPP_
#define IASAVBPACKETPOOL_HPP_

#include "IasAvbPacket.hpp"
#include "IasAvbTypes.hpp"
#include "IasAvbStreamHandlerEnvironment.hpp"
#include <vector>
#include <linux/if_ether.h>
#include <mutex>

extern "C"
{
  #include "igb.h"
}


namespace IasMediaTransportAvb
{

class IasAvbPacketPool
{
  public:

    /**
     *  @brief Constructor.
     */
    explicit IasAvbPacketPool(DltContext &dltContext);

    /**
     *  @brief Destructor, non-virtual as we don't have any virtual methods and do not belong to a class hierarchy.
     */
    /*non-virtual*/
    ~IasAvbPacketPool();

    // Operations

    IasAvbProcessingResult init(size_t packetSize, uint32_t poolSize);
    void cleanup();
    IasAvbPacket* getPacket();
    inline IasAvbPacket* getDummyPacket();
    IasAvbProcessingResult initAllPacketsFromTemplate(const IasAvbPacket* templatePacket);
    inline static IasAvbProcessingResult returnPacket(igb_packet* packet);
    static IasAvbProcessingResult returnPacket(IasAvbPacket* packet);
    inline size_t getPacketSize() const;
    inline uint32_t getPoolSize() const;
    IasAvbProcessingResult reset();

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbPacketPool(IasAvbPacketPool const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbPacketPool& operator=(IasAvbPacketPool const &other);

  private:
    // Local Types
    typedef igb_dma_alloc Page;
    typedef std::vector<IasAvbPacket*> PacketStack;
    typedef std::vector<Page*> PageList;

    // Constants

    static const uint32_t cMaxPoolSize = 2048u;                // derived from max TX ring size / 2
#if defined(DIRECT_RX_DMA)
    static const size_t cMaxBufferSize = 2048u;              // fixed value by libigb
#else
    static const size_t cMaxBufferSize = ETH_FRAME_LEN + 4u; // consider VLAN TAG
#endif /* DIRECT_RX_DMA */

    // helpers
    IasAvbProcessingResult initPage(Page * page, const uint32_t packetsPerPage, uint32_t & packetCountTotal);
    IasAvbProcessingResult doReturnPacket(IasAvbPacket* packet);

    // Members
    DltContext *mLog;
    std::mutex mLock;
    size_t mPacketSize;
    uint32_t mPoolSize;
    PacketStack mFreeBufferStack;
    IasAvbPacket* mBase;
    PageList mDmaPages;
};


inline IasAvbProcessingResult IasAvbPacketPool::returnPacket(igb_packet* packet)
{
  // simply downcast - probing will be done in the called function
  return returnPacket(static_cast<IasAvbPacket*>(packet));
}

inline size_t IasAvbPacketPool::getPacketSize() const
{
  return mPacketSize;
}

inline uint32_t IasAvbPacketPool::getPoolSize() const
{
  return mPoolSize;
}

inline IasAvbPacket* IasAvbPacketPool::getDummyPacket()
{
  IasAvbPacket* ret = getPacket();

  if (NULL != ret)
  {
    ret->mDummyFlag = true;
  }

  return ret;
}


} // namespace IasMediaTransportAvb

#endif /* IASAVBCLOCKDOMAIN_HPP_ */
