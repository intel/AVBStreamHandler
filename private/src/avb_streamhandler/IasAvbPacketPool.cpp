/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbPacketPool.cpp
 * @brief   The implementation of the IasAvbPacketPool class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include <cstring>
#include <unistd.h>
#include <dlt/dlt_cpp_extension.hpp>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbPacketPool::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbPacketPool::IasAvbPacketPool(DltContext &dltContext) :
  mLog(&dltContext),
  mLock(),
  mPacketSize(0u),
  mPoolSize(0u),
  mFreeBufferStack(),
  mBase(NULL),
  mDmaPages()
{
  // do nothing
}


/*
 *  Destructor.
 */
IasAvbPacketPool::~IasAvbPacketPool()
{
  cleanup();
}


IasAvbProcessingResult IasAvbPacketPool::init(const size_t packetSize, const uint32_t poolSize)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (NULL != mBase)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Already initialized");
    ret = eIasAvbProcInitializationFailed;
  }
  else
  {
    if ((0u == packetSize) || (packetSize > cMaxBufferSize))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
              "packetSize = 0 or packetSize > cMaxBufferSize. packetSize =",
              uint64_t(packetSize), "cMaxBufferSize =", uint64_t(cMaxBufferSize));
      ret = eIasAvbProcInvalidParam;
    }
    else if ((0u == poolSize) || (poolSize > cMaxPoolSize))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
              "poolSize = 0 or poolSize > cMaxPoolSize. poolSize =",
              uint64_t(poolSize), "cMaxPoolSize =", uint64_t(cMaxPoolSize));
      ret = eIasAvbProcInvalidParam;
    }
    else
    {
      // all params OK
    }

    if (eIasAvbProcOK == ret)
    {
      mBase = new (nothrow) IasAvbPacket[poolSize];
      if (NULL == mBase)
      {
        /*
         * @log Not enough memory: Packet Pool not created.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
                  "Not enough memory to allocate IasAvbPacket! Poolsize=", poolSize);
        ret = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        mPoolSize = poolSize;
      }
    }

    if (eIasAvbProcOK == ret)
    {
      device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
      Page* page = NULL;

      if (NULL == igbDevice)
      {
        /*
         * @log Init failed: Returned igbDevice == nullptr
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to getIgbDevice!");
        ret = eIasAvbProcInitializationFailed;
      }
      else
      {
        page = new (nothrow) Page;

        if (NULL == page)
        {
          /*
           * @log Not enough memory: Couldn't allocate DMA page, Page corresponds to underlying igb_dma_alloc struct.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to allocate Page!");
          ret = eIasAvbProcNotEnoughMemory;
        }
      }

      if (eIasAvbProcOK == ret)
      {
        // allocate one DMA page to retrieve properties
        if (0 != igb_dma_malloc_page( igbDevice, page ))
        {
          /*
           * @log Init failed: Failed to retrieve DMA page.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to igb_dma_malloc_page!");
          ret = eIasAvbProcInitializationFailed;
        }
        else
        {
          uint32_t packetCountTotal = 0u;
          mPacketSize = packetSize;

          // find out how many packets fit into a page
          const uint32_t packetsPerPage = uint32_t( size_t(page->mmap_size) / packetSize );

          if (0u == packetsPerPage)
          {
            // packetSize larger than dma page size - not supported by libigb
            /*
             * @log Unsupported format: Packet size is larger than the dma page size - not supported by libigb.
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " packet size > ",
                page->mmap_size, "not supported!");
            ret = eIasAvbProcUnsupportedFormat;
          }
          else
          {
            const uint32_t pagesNeeded = (poolSize + (packetsPerPage - 1u)) / packetsPerPage;

            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " DMA page overhead (bytes:",
                (uint64_t(pagesNeeded) * uint64_t(page->mmap_size)) - (uint64_t(poolSize) * uint64_t(packetSize)));

            mDmaPages.reserve(pagesNeeded);

            ret = initPage( page, packetsPerPage, packetCountTotal );

            // 1st page is already allocated
            for (uint32_t pageCount = 1u; (eIasAvbProcOK == ret) && (pageCount < pagesNeeded); pageCount++)
            {
              page = new (nothrow) Page;

              if (NULL == page)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Not enough memory to allocate Page!");
                ret = eIasAvbProcNotEnoughMemory;
              }
              else if (0 != igb_dma_malloc_page( igbDevice, page ))
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " igb dma memory allocation failure");
                ret = eIasAvbProcInitializationFailed;
              }
              else
              {
                ret = initPage( page, packetsPerPage, packetCountTotal );
              }
            }
          }
        }
      }
    }

    if (eIasAvbProcOK != ret)
    {
      cleanup();
    }
  }

  return ret;
}


IasAvbProcessingResult IasAvbPacketPool::initPage(Page * page, const uint32_t packetsPerPage, uint32_t & packetCountTotal)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  AVB_ASSERT( NULL != mBase );
  AVB_ASSERT( NULL != page );
  AVB_ASSERT( mPacketSize > 0u );

  // add page to list
  mDmaPages.push_back(page);

  for (uint32_t packetIdx = 0u; (packetIdx < packetsPerPage) && (packetCountTotal < mPoolSize); packetIdx++, packetCountTotal++)
  {
    /*
     *  assign unique section of dma page to each igb packet
     *  Note that this is not touched anymore during subsequent operation!
     */
    IasAvbPacket & packet = mBase[packetCountTotal];

    packet.offset = uint32_t( packetIdx * mPacketSize );
    packet.vaddr = static_cast<uint8_t*>(page->dma_vaddr) + packet.offset;
    packet.map.mmap_size = page->mmap_size;
    packet.map.paddr = page->dma_paddr;

    packet.setHomePool( this );
    mFreeBufferStack.push_back( &packet );
  }

  return ret;
}


void IasAvbPacketPool::cleanup()
{
  if (mFreeBufferStack.size() < mPoolSize)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX,
                " waiting for remaining buffers before pool destruction.",
                uint32_t(mFreeBufferStack.size()),
                "/",
                mPoolSize);

    // wait up to 40ms in 4ms intervals
    for (uint32_t i = 0u; i < 10u; i++)
    {
      ::usleep(5000u);
      if (mFreeBufferStack.size() >= mPoolSize)
      {
        break;
      }
    }
  }

  if (mFreeBufferStack.size() < mPoolSize)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                " warning: not all buffers returned before pool destruction!",
                uint32_t(mFreeBufferStack.size()), "/", mPoolSize);
  }

  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  while (!mDmaPages.empty())
  {
    Page* page = mDmaPages.back();
    mDmaPages.pop_back();

    AVB_ASSERT( NULL != page  );

    if (NULL == igbDevice)
    {
    }
    else
    {
      igb_dma_free_page( igbDevice, page );
      delete page;
    }
  }

  delete[] mBase;
  mBase = NULL;
}


IasAvbPacket* IasAvbPacketPool::getPacket()
{
  IasAvbPacket* ret = NULL;

  mLock.lock();

  if (NULL != mBase)
  {
    if (!mFreeBufferStack.empty())
    {
      ret = mFreeBufferStack.back();
      mFreeBufferStack.pop_back();

      AVB_ASSERT(NULL != ret);
      ret->flags = 0u;
      ret->dmatime = 0u;
    }
  }

  mLock.unlock();

  return ret;
}


IasAvbProcessingResult IasAvbPacketPool::doReturnPacket(IasAvbPacket* const packet)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  mLock.lock();

  if (NULL == mBase)
  {
    ret = eIasAvbProcNotInitialized;
  }
  else
  {
    AVB_ASSERT( NULL != packet );
    AVB_ASSERT( packet->getHomePool() == this );

    packet->mDummyFlag = false;

    if (mFreeBufferStack.size() < mPoolSize)
    {
      mFreeBufferStack.push_back(packet);

      if (mFreeBufferStack.size() == mPoolSize)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " All buffers returned\n");
      }
    }
    else if (mFreeBufferStack.size() == mPoolSize)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Too many packets returned\n");
      // put it on the stack to avoid further LAT messages - stack is inconsistent anyway
      mFreeBufferStack.push_back(packet);
    }
    else
    {
      // do nothing to avoid further stack growth
    }
  }

  mLock.unlock();

  return ret;
}


IasAvbProcessingResult IasAvbPacketPool::initAllPacketsFromTemplate(const IasAvbPacket* templatePacket)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (NULL == mBase)
  {
    ret = eIasAvbProcNotInitialized;
  }
  else if ((NULL == templatePacket) || (NULL == templatePacket->vaddr) || (0u == templatePacket->len))
  {
    ret = eIasAvbProcInvalidParam;
  }
  else
  {
    for (PacketStack::iterator it = mFreeBufferStack.begin(); it != mFreeBufferStack.end(); it++)
    {
      IasAvbPacket * packet = *it;
      AVB_ASSERT( NULL != packet);

      *packet = *templatePacket;
    }
  }

  return ret;
}


IasAvbProcessingResult IasAvbPacketPool::reset()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  mLock.lock();

  if (NULL == mBase)
  {
    ret = eIasAvbProcNotInitialized;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " clear FreeBufferStack and push back all buffer");
    mFreeBufferStack.clear();
    for (uint32_t packetIdx = 0u; packetIdx < mPoolSize; packetIdx++)
    {
      mFreeBufferStack.push_back( &mBase[packetIdx] );
    }
  }

  mLock.unlock();

  return ret;
}

IasAvbProcessingResult IasAvbPacketPool::returnPacket(IasAvbPacket* packet)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (NULL == packet)
  {
    DltContext &dltCtx = IasAvbStreamHandlerEnvironment::getDltContext("_AAS");
    DLT_LOG_CXX(dltCtx,  DLT_LOG_ERROR, LOG_PREFIX, "packet is NULL!");
    ret = eIasAvbProcInvalidParam;
  }
  else
  {
    // probe if really an IasAvbPacket
    if  (!packet->isValid())
    {
      DltContext &dltCtx = IasAvbStreamHandlerEnvironment::getDltContext("_AAS");
      DLT_LOG_CXX(dltCtx,  DLT_LOG_ERROR, LOG_PREFIX, "invalid packet!");
      ret = eIasAvbProcInvalidParam;
    }
    else
    {
      IasAvbPacketPool * const home = packet->getHomePool();
      AVB_ASSERT(NULL != home);
      ret = home->doReturnPacket(packet);
    }
  }

  return ret;
}

} // namespace IasMediaTransportAvb
