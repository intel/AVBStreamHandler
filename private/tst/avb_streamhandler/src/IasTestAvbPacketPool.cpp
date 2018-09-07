/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbPacketPool.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb
{

class IasTestAvbPacketPool : public ::testing::Test
{
protected:
    IasTestAvbPacketPool():
      mEnvironment(NULL),
      mAvbPacketPool(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbPacketPool()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

    dlt_enable_local_print();
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->registerDltContexts();

    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
              "TEST",
              "IasTestAvbPacketPool",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);

    mAvbPacketPool = new IasAvbPacketPool(mDltCtx);
  }

  virtual void TearDown()
  {
    delete mAvbPacketPool;
    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    heapSpaceLeft = heapSpaceInitSize;

    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }

  bool LocalSetup()
  {
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();

      if (IasSpringVilleInfo::fetchData())
      {
        mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());
        IasSpringVilleInfo::printDebugInfo();
      }
      else
      {
        mEnvironment->setConfigValue(IasRegKeys::cNwIfName, "p1p2");
      }

      if (eIasAvbProcOK == mEnvironment->createIgbDevice())
      {
        if (IasAvbStreamHandlerEnvironment::getIgbDevice())
        {
          if (eIasAvbProcOK == mEnvironment->createPtpProxy())
          {
            return true;
          }
        }
      }
    }
    return false;
  }

  IasAvbStreamHandlerEnvironment * mEnvironment;
  IasAvbPacketPool* mAvbPacketPool;
  DltContext mDltCtx;
};

TEST_F(IasTestAvbPacketPool, CTor_DTor)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
}

TEST_F(IasTestAvbPacketPool, init_fail)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  size_t packetSize = 0;
  uint32_t poolSize = 0;
  IasAvbProcessingResult result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbPacketPool, init_good)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  size_t packetSize = 256;
  uint32_t poolSize = 1024;
  IasAvbProcessingResult result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbPacketPool, getPacket)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  IasAvbPacket *packet = mAvbPacketPool->getPacket();
  ASSERT_TRUE( NULL == packet );

  ASSERT_TRUE(LocalSetup());
  size_t packetSize = 256;
  uint32_t poolSize = 1024;
  ASSERT_EQ(eIasAvbProcOK, mAvbPacketPool->init(packetSize, poolSize));

  // already initialized
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbPacketPool->init(packetSize, poolSize));
  mAvbPacketPool->mFreeBufferStack.clear();

  ASSERT_TRUE(NULL == mAvbPacketPool->getPacket());
}

TEST_F(IasTestAvbPacketPool, initAllPacketsFromTemplate)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  IasAvbPacket packet;
  IasAvbProcessingResult result = mAvbPacketPool->initAllPacketsFromTemplate(&packet);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  ASSERT_TRUE(LocalSetup());
  size_t packetSize = 256;
  uint32_t poolSize = 1024;
  ASSERT_EQ(eIasAvbProcOK, mAvbPacketPool->init(packetSize, poolSize));
  packet.vaddr = &poolSize;
  packet.len = 0u;
  // (NULL == templatePacket)           (F)
  // || (NULL == templatePacket->vaddr) (F)
  // || (0u == templatePacket->len)     (T)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbPacketPool->initAllPacketsFromTemplate(&packet));

  packet.vaddr = NULL;
  packet.len = 1u;
  // (NULL == templatePacket)           (F)
  // || (NULL == templatePacket->vaddr) (T)
  // || (0u == templatePacket->len)     (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbPacketPool->initAllPacketsFromTemplate(&packet));
}

TEST_F(IasTestAvbPacketPool, returnPacket_NULL)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  igb_packet * const nullpkt = NULL;
  IasAvbProcessingResult result = mAvbPacketPool->returnPacket(nullpkt);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbPacketPool, getDummypacket)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  IasAvbPacket *packet = mAvbPacketPool->getDummyPacket();
  ASSERT_TRUE(NULL == packet);

  ASSERT_TRUE(LocalSetup());

  size_t packetSize = 256;
  uint32_t poolSize = 1024;

  ASSERT_EQ(eIasAvbProcOK, mAvbPacketPool->init(packetSize, poolSize));
  packet = mAvbPacketPool->getDummyPacket();
  ASSERT_TRUE(NULL != packet);
}

TEST_F(IasTestAvbPacketPool, getPacketSize)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  size_t size = mAvbPacketPool->getPacketSize();
  ASSERT_EQ(0, size);
}

TEST_F(IasTestAvbPacketPool, getPoolSize)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);
  uint32_t size = mAvbPacketPool->getPoolSize();
  ASSERT_EQ(0, size);
}

TEST_F(IasTestAvbPacketPool, BRANCH_Init)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);

  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mAvbPacketPool->init(0, 2);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mAvbPacketPool->init(32, 2);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbPacketPool->init(33, 3);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  result = mAvbPacketPool->init(34, 4);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbPacketPool, HEAP_Failed_Init)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);

  ASSERT_TRUE(LocalSetup());

  IasAvbProcessingResult result = eIasAvbProcErr;
  size_t poolSize = 32;
  uint32_t packetSize = 2;

  heapSpaceLeft = 0;

  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  poolSize = 1;
  heapSpaceLeft = sizeof(IasAvbPacket) * poolSize;

  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  packetSize = 64;
  poolSize = 2 * packetSize;
  heapSpaceLeft = sizeof(IasAvbPacket) * poolSize + sizeof(igb_dma_alloc);

  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  heapSpaceLeft = sizeof(IasAvbPacket) * poolSize + sizeof(igb_dma_alloc) * 2;

  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);
}

TEST_F(IasTestAvbPacketPool, DoReturnPacket)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);

  IasAvbProcessingResult result = eIasAvbProcErr;

  IasAvbPacket packet;
  packet.setHomePool(NULL);
  packet.setHomePool(mAvbPacketPool);

  igb_packet * const nullpkt = NULL;
  result = mAvbPacketPool->returnPacket(nullpkt);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mAvbPacketPool->returnPacket(&packet);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  ASSERT_TRUE(LocalSetup());

  size_t packetSize = 1024;
  uint32_t poolSize = 32;
  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasAvbPacket *p = mAvbPacketPool->getPacket();
  ASSERT_TRUE(NULL != p);

  // healthy return
  result = mAvbPacketPool->returnPacket(&packet);
  ASSERT_EQ(eIasAvbProcOK, result);

  // return once too many - warning message issued
  result = mAvbPacketPool->returnPacket(&packet);
  ASSERT_EQ(eIasAvbProcOK, result);

  // return twice too many - ignored silently
  result = mAvbPacketPool->returnPacket(&packet);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbPacketPool, InitAllPacketsFromTemplate)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);

  IasAvbProcessingResult result = eIasAvbProcErr;

  ASSERT_TRUE(LocalSetup());

  result = mAvbPacketPool->initAllPacketsFromTemplate(NULL);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  size_t packetSize = 1024;
  uint32_t poolSize = 32;
  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbPacketPool->initAllPacketsFromTemplate(NULL);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  IasAvbPacket *packet = mAvbPacketPool->getPacket();
  ASSERT_TRUE(NULL != packet);
  void* vaddr = packet->vaddr;
  packet->vaddr = NULL;
  result = mAvbPacketPool->initAllPacketsFromTemplate(packet);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
  packet->vaddr = vaddr;
  packet->len = 0u;
  result = mAvbPacketPool->initAllPacketsFromTemplate(packet);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
  packet->len = 18u;
  result = mAvbPacketPool->initAllPacketsFromTemplate(packet);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbPacketPool, reset)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);

  IasAvbProcessingResult result = eIasAvbProcErr;

  ASSERT_TRUE(LocalSetup());

  result = mAvbPacketPool->reset();
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  size_t packetSize = 1024;
  uint32_t poolSize = 32;
  result = mAvbPacketPool->init(packetSize, poolSize);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbPacketPool->reset();
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbPacketPool, Reset)
{
  ASSERT_TRUE(NULL != mAvbPacketPool);

  ASSERT_EQ(eIasAvbProcNotInitialized, mAvbPacketPool->reset());
}

} /* IasMediaTransportAvb */
