/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbPacket.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#undef protected
#undef private


using namespace IasMediaTransportAvb;

class IasTestAvbPacket : public ::testing::Test
{
protected:
    IasTestAvbPacket():
      mAvbPacket(NULL),
      mDltContext(NULL)
  {
  }

  virtual ~IasTestAvbPacket() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    mAvbPacket = new IasAvbPacket();
    mDltContext = new DltContext();
  }

  virtual void TearDown()
  {
    delete mAvbPacket;
    mAvbPacket = NULL;

    delete mDltContext;
    mDltContext = NULL;
  }

  IasAvbPacket* mAvbPacket;
  DltContext * mDltContext;
};

TEST_F(IasTestAvbPacket, CTor_DTor)
{
  ASSERT_TRUE(NULL != mAvbPacket);
}

TEST_F(IasTestAvbPacket, assign_op)
{
  unsigned char source[1024] = { 1 }; // rest will be zeroed
  source[(sizeof source) - 1] = 255;

  unsigned char dest[sizeof source] = {}; // all zero

  ASSERT_TRUE(NULL != mAvbPacket);
  mAvbPacket->vaddr = source;
  mAvbPacket->len = sizeof source;
  std::unique_ptr<IasAvbPacket> packet(new IasAvbPacket());
  ASSERT_TRUE(NULL != packet);
  packet->vaddr = dest;
  *packet = *mAvbPacket;
  ASSERT_EQ(source[0], dest[0]);
  ASSERT_EQ(source[1023], dest[1023]);
  ASSERT_EQ(packet->len, sizeof source);
}

TEST_F(IasTestAvbPacket, getHomePool)
{
  ASSERT_TRUE(NULL != mAvbPacket);
  (void)mAvbPacket->getHomePool();

}

TEST_F(IasTestAvbPacket, setHomePool)
{
  ASSERT_TRUE(NULL != mAvbPacket);
  mAvbPacket->setHomePool(NULL);

  IasAvbPacketPool * homePool = new IasAvbPacketPool(*mDltContext);

  mAvbPacket->setHomePool(homePool);
  mAvbPacket->setHomePool(homePool);

  delete homePool;
}

TEST_F(IasTestAvbPacket, getPayloadOffset)
{
  ASSERT_TRUE(NULL != mAvbPacket);
  (void)mAvbPacket->getPayloadOffset();
}

TEST_F(IasTestAvbPacket, setPayloadOffset)
{
  unsigned char buf[256];
  const size_t offset = (sizeof buf) / 2;

  ASSERT_TRUE(NULL != mAvbPacket);
  mAvbPacket->vaddr = buf;
  mAvbPacket->setPayloadOffset(offset);

  ASSERT_EQ(mAvbPacket->getPayloadOffset(), offset);
  ASSERT_EQ(mAvbPacket->getPayloadPointer(), &buf[offset]);
}

TEST_F(IasTestAvbPacket, isValid)
{
  ASSERT_TRUE(NULL != mAvbPacket);

  mAvbPacket->mMagic = 0u;
  ASSERT_FALSE(mAvbPacket->isValid());

  DltContext dltCtx;
  IasAvbPacketPool * pool = new IasAvbPacketPool(dltCtx);
  mAvbPacket->mHome = pool;
  ASSERT_FALSE(mAvbPacket->isValid());

  delete pool;
}

TEST_F(IasTestAvbPacket, dummyPacket)
{
  ASSERT_TRUE(NULL != mAvbPacket);
  mAvbPacket->mDummyFlag = false;
  ASSERT_FALSE(mAvbPacket->isDummyPacket());
  mAvbPacket->makeDummyPacket();
  ASSERT_TRUE(mAvbPacket->isDummyPacket());
}
