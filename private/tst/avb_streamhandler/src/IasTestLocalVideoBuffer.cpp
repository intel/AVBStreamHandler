/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestLocalVideoBuffer.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasLocalVideoBuffer.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "test_common/IasSpringVilleInfo.hpp"
#define protected protected
#define private private

#include <iostream>

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;

namespace IasMediaTransportAvb
{

class IasTestLocalVideoBuffer : public ::testing::Test
{
protected:
    IasTestLocalVideoBuffer()
    : mLocalVideoBuffer(NULL)
    , mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestLocalVideoBuffer()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

    mEnvironment = new (nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->registerDltContexts();

    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
              "_TST",
              "IasTestLocalVideoBuffer",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_ON);

    mLocalVideoBuffer = new IasLocalVideoBuffer();
  }

  virtual void TearDown()
  {
    delete mLocalVideoBuffer;
    mLocalVideoBuffer = NULL;

    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->unregisterDltContexts();

    delete mEnvironment;
    mEnvironment = NULL;

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

  IasLocalVideoBuffer* mLocalVideoBuffer;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  DltContext mDltCtx;
};


} // namespace IasMediaTransportAvb



TEST_F(IasTestLocalVideoBuffer, CTor_DTor)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);
}

TEST_F(IasTestLocalVideoBuffer, init)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 0;
  uint16_t maxPacketSize = 0;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));
  numPackets = 1;
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));
  numPackets = 2;
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));
  numPackets = 0;
  maxPacketSize = 1500;
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));
  numPackets = 2;
  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  heapSpaceLeft = 0;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  heapSpaceLeft = heapSpaceInitSize;
  internalBuffers = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));
}

TEST_F(IasTestLocalVideoBuffer, reset)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 2;
  uint16_t maxPacketSize = 1500;
  bool internalBuffers = false;
  uint32_t optimalFillLevel = 0;

  IasAvbProcessingResult result = mLocalVideoBuffer->reset(optimalFillLevel);
  ASSERT_EQ(eIasAvbProcNotInitialized, result);

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  result = mLocalVideoBuffer->reset(optimalFillLevel);
  ASSERT_EQ(eIasAvbProcOK, result);

  IasLocalVideoBuffer::IasVideoDesc * videoDesc = new IasLocalVideoBuffer::IasVideoDesc();
  IasAvbPacket * avbPacket = new IasAvbPacket();
  videoDesc->avbPacket = avbPacket;
  mLocalVideoBuffer->mRing[0] = *videoDesc;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->reset(optimalFillLevel));

  delete videoDesc;
  delete avbPacket;
}

TEST_F(IasTestLocalVideoBuffer, write)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  /* NULL packet */
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(NULL));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(NULL));

  /* NULL packet.buffer.data */
  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof (packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));

  /* packet.buffer.size of 0 */
  uint8_t oneByte;
  packet.buffer.data = &oneByte;
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));

  /* Fail due to no buffer initialised */
  packet.buffer.size = sizeof (oneByte);
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));

  uint16_t numPackets = 2u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;
  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  std::unique_ptr<IasLocalVideoBuffer::IasVideoDesc> oneBytePacket(new IasLocalVideoBuffer::IasVideoDesc());
  oneBytePacket->buffer.data = &oneByte;
  oneBytePacket->buffer.size = sizeof (oneByte);

  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(oneBytePacket.get()));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(oneBytePacket.get()));

  std::unique_ptr<IasAvbPacketPool> pool(new IasAvbPacketPool(mDltCtx));
  mLocalVideoBuffer->setAvbPacketPool(pool.get());

  mLocalVideoBuffer->mWriteIndex = 0u;
  mLocalVideoBuffer->mReadIndex = -numPackets;

  int result_h264 = mLocalVideoBuffer->writeH264(oneBytePacket.get());
  int result_mpeg_ts = mLocalVideoBuffer->writeMpegTS(oneBytePacket.get());

  // getFillLevel() == (mNumPacketsTotal-1) (F)
  ASSERT_EQ(0, result_h264);
  ASSERT_EQ(0, result_mpeg_ts);
}

TEST_F(IasTestLocalVideoBuffer, write_pool)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 5u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));
  IasAvbPacketPool * pool = new IasAvbPacketPool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(pool);

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof (packet));
  uint8_t data[maxPacketSize];
  memset(data, 0, sizeof data);
  packet.buffer.data = data;
  packet.buffer.size = sizeof(data);
  packet.isIec61883Packet = true;
  IasAvbPacket avbPacket;
  packet.avbPacket = &avbPacket;
  packet.tspsInAvbPacket = 6u;
  packet.avbPacket->setHomePool(pool);
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));

  mLocalVideoBuffer->mRing[0] = packet;
  mLocalVideoBuffer->mRing[1] = packet;
  mLocalVideoBuffer->mRing[2] = packet;
  mLocalVideoBuffer->mWriteIndex = 3u;

  ASSERT_EQ(0u, mLocalVideoBuffer->writeMpegTS(&packet));

  mLocalVideoBuffer->mReadIndex = 1u;

  ASSERT_EQ(0u, mLocalVideoBuffer->writeMpegTS(&packet));

  mLocalVideoBuffer->mRing[mLocalVideoBuffer->mWriteIndex - 1].tspsInAvbPacket = (maxPacketSize / (192u)) + 1u;

  ASSERT_EQ(0u, mLocalVideoBuffer->writeMpegTS(&packet));

  mLocalVideoBuffer->mRing[mLocalVideoBuffer->mWriteIndex - 1].isIec61883Packet = false;

  ASSERT_EQ(0u, mLocalVideoBuffer->writeH264(&packet));

  mLocalVideoBuffer->mNumPacketsTotal = mLocalVideoBuffer->mWriteIndex - 1;

  ASSERT_EQ(0u, mLocalVideoBuffer->writeH264(&packet));

  delete pool;
}

TEST_F(IasTestLocalVideoBuffer, read)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, NULL));
}

TEST_F(IasTestLocalVideoBuffer, getFillLevel)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);
  uint32_t fillLevel = mLocalVideoBuffer->getFillLevel();
  (void) fillLevel;
}

TEST_F(IasTestLocalVideoBuffer, getTotalSize)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);
  uint32_t totalSize = mLocalVideoBuffer->getTotalSize();
  (void) totalSize;
}

TEST_F(IasTestLocalVideoBuffer, setAvbPacketPool)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);
  mLocalVideoBuffer->setAvbPacketPool(NULL);
}

TEST_F(IasTestLocalVideoBuffer, IasVideoDesc_CTOR)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);
  IasLocalVideoBuffer::IasVideoDesc desc;
  (void) desc;
}

TEST_F(IasTestLocalVideoBuffer, new_zero_free)
{
  unsigned char *buffer = new (nothrow) unsigned char[0];
  ASSERT_TRUE(NULL != buffer);
  delete[] buffer;
}

TEST_F(IasTestLocalVideoBuffer, init_write_read_local)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 2;
  uint16_t maxPacketSize = 1500;
  // Internal buffering is nolonger supported so write routines attempting to use them will return 0 bytes written.
  // The original code is left in placec just in case we need to get foot out of mouth,
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);

  /* Write one packet */
  //ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));

  /* Try and Write another packet, but the ring buffer is full */
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));

  IasLocalVideoBuffer::IasVideoDesc outPacket;
  for (int i = 0; i < 16000; ++i)
  {
    ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, &outPacket));
    // since internal buffering is now unsupported
    // ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));
    ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));

    if (!(i % 8000))
    {
      usleep(100);
    }
  }

  ASSERT_EQ(0, mLocalVideoBuffer->read(&packetData, &outPacket));
}

TEST_F(IasTestLocalVideoBuffer,init_write_local_iec)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 2u;
  uint16_t maxPacketSize = 10 * (188u + 4u);
  bool internalBuffers = true;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  mLocalVideoBuffer->mBuffer = new (nothrow) IasLocalVideoBuffer::VideoData[numPackets * maxPacketSize];

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[10 * 188] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);
  packet.isIec61883Packet = true;

  /* Write one packet */
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));
}

TEST_F(IasTestLocalVideoBuffer, init_write_read_packet_pool)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 2;
  uint16_t maxPacketSize = 1500;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);

  /* Write one packet */
  ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));

  /* Write second packet, but buffer is full */
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));

  /* Read off one packet */
  IasLocalVideoBuffer::IasVideoDesc outPacket;
  ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, &outPacket));

  /* Store this packet reference otherwise we lose it */
  IasAvbPacket *packetRef = outPacket.avbPacket;

  /* Write second packet, and read it */
  packet.rtpSequenceNumber++;
  ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, &outPacket));

  /* Write third packet, pool is now out of memory */
  packet.rtpSequenceNumber++;
  ASSERT_EQ(0, mLocalVideoBuffer->writeH264(&packet));

  /* Return the packets to the pool */
  pool.returnPacket(outPacket.avbPacket);
  pool.returnPacket(packetRef);
  packetRef = NULL;

  packet.rtpSequenceNumber--;
  ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, &outPacket));
  pool.returnPacket(outPacket.avbPacket);
  packet.rtpSequenceNumber+=2;

  for (int i = 0; i < 16000; i++)
  {
    ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));
    ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, &outPacket));
    pool.returnPacket(outPacket.avbPacket);
    packet.rtpSequenceNumber++;

    if (!(i % 8000))
    {
      usleep(100);
    }
  }

  outPacket.avbPacket = NULL;
  /* Read when empty */
  ASSERT_EQ(0, mLocalVideoBuffer->read(NULL, &outPacket));
  ASSERT_TRUE(NULL == outPacket.avbPacket);

  /* Valid read buffer but no local buffer */
  ASSERT_EQ(sizeof(packetData), mLocalVideoBuffer->writeH264(&packet));
  packet.rtpSequenceNumber++;
  uint8_t tmp;
  ASSERT_EQ(0, mLocalVideoBuffer->read(&tmp, &outPacket));
  ASSERT_TRUE(NULL != outPacket.avbPacket);
  pool.returnPacket(outPacket.avbPacket);

  pool.cleanup();
}

TEST_F(IasTestLocalVideoBuffer, init_write_packet_pool)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 2u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  IasAvbPacket avbPacket;
  uint8_t avbPacketVaddr[1500] = {};
  avbPacket.vaddr = avbPacketVaddr;
  avbPacket.len = 1u;
  avbPacket.setPayloadOffset(1);
  ASSERT_EQ(eIasAvbProcOK, pool.initAllPacketsFromTemplate(&avbPacket));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);
  packet.isIec61883Packet = true;
  // uint32_t expectedBytesWritten = packet.buffer.size - (packet.buffer.size % 192u);

  /* Write one packet */
  //ASSERT_EQ(expectedBytesWritten, mLocalVideoBuffer->writeMpegTS(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));
}

TEST_F(IasTestLocalVideoBuffer, init_write_packet_pool_headroom)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 6u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  IasAvbPacket avbPacket;
  uint8_t avbPacketVaddr[1500] = {};
  avbPacket.vaddr = avbPacketVaddr;
  avbPacket.len = 1u;
  ASSERT_EQ(eIasAvbProcOK, pool.initAllPacketsFromTemplate(&avbPacket));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);
  packet.isIec61883Packet = true;
  packet.hasSph = true;
  uint32_t expectedBytesWritten = packet.buffer.size - (packet.buffer.size % 192u);
  mLocalVideoBuffer->mWriteIndex = 3u;
  mLocalVideoBuffer->mRing[2].isIec61883Packet = true;
  IasAvbPacket ringPacket;
  uint8_t ringPacketVaddr[1500] = {};
  ringPacket.vaddr = ringPacketVaddr;
  ringPacket.len = 1;
  ringPacket = avbPacket;
  ringPacket.setHomePool(&pool);
  ringPacket.setPayloadOffset(0u);

  mLocalVideoBuffer->mRing[2].avbPacket = &ringPacket;

  /* Write one packet */
  ASSERT_EQ(expectedBytesWritten, mLocalVideoBuffer->writeMpegTS(&packet));
}

TEST_F(IasTestLocalVideoBuffer, init_write_packet_pool_headroom_no_remaining)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 6u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  IasAvbPacket avbPacket;
  uint8_t avbPacketVaddr[1500] = {};
  avbPacket.vaddr = avbPacketVaddr;
  avbPacket.len = 1u;
  ASSERT_EQ(eIasAvbProcOK, pool.initAllPacketsFromTemplate(&avbPacket));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);
  packet.isIec61883Packet = true;
  packet.hasSph = true;
  mLocalVideoBuffer->mWriteIndex = 5u;
  mLocalVideoBuffer->mRing[2].isIec61883Packet = true;
  IasAvbPacket ringPacket;
  uint8_t ringPacketVaddr[1500] = {};
  ringPacket.vaddr = ringPacketVaddr;
  ringPacket.len = 1;
  ringPacket = avbPacket;
  ringPacket.setHomePool(&pool);
  ringPacket.setPayloadOffset(0u);

  mLocalVideoBuffer->mRing[2].avbPacket = &ringPacket;

  /* Write one packet */
  ASSERT_EQ(0u, mLocalVideoBuffer->writeMpegTS(&packet));
}

TEST_F(IasTestLocalVideoBuffer, init_write_packet_pool_headroom_tsps)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 6u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  IasAvbPacket avbPacket;
  uint8_t avbPacketVaddr[1500] = {};
  avbPacket.vaddr = avbPacketVaddr;
  avbPacket.len = 1u;
  ASSERT_EQ(eIasAvbProcOK, pool.initAllPacketsFromTemplate(&avbPacket));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);
  packet.isIec61883Packet = true;
  packet.hasSph = true;
  uint32_t expectedBytesWritten = packet.buffer.size - (packet.buffer.size % 192u);
  mLocalVideoBuffer->mWriteIndex = 3u;
  mLocalVideoBuffer->mRing[2].isIec61883Packet = true;
  IasAvbPacket ringPacket;
  uint8_t ringPacketVaddr[1500] = {};
  ringPacket.vaddr = ringPacketVaddr;
  ringPacket.len = 1;
  ringPacket = avbPacket;
  ringPacket.setHomePool(&pool);
  ringPacket.setPayloadOffset(0u);

  mLocalVideoBuffer->mRing[2].avbPacket = &ringPacket;
  mLocalVideoBuffer->mRing[2].tspsInAvbPacket = 1u;

  /* Write one packet */
  ASSERT_EQ(expectedBytesWritten, mLocalVideoBuffer->writeMpegTS(&packet));
}

TEST_F(IasTestLocalVideoBuffer, init_write_packet_pool_no_headroom)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);

  uint16_t numPackets = 2u;
  uint16_t maxPacketSize = 1500u;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers));

  ASSERT_TRUE(LocalSetup());

  IasAvbPacketPool pool(mDltCtx);
  mLocalVideoBuffer->setAvbPacketPool(&pool);
  ASSERT_EQ(eIasAvbProcOK, pool.init(maxPacketSize, numPackets));

  IasAvbPacket avbPacket;
  uint8_t avbPacketBuffer[1500] = {};
  avbPacket.vaddr = avbPacketBuffer;
  avbPacket.len = 1u;
  avbPacket.setPayloadOffset(1);
  ASSERT_EQ(eIasAvbProcOK, pool.initAllPacketsFromTemplate(&avbPacket));

  IasLocalVideoBuffer::IasVideoDesc packet;
  memset(&packet, 0, sizeof packet);
  uint8_t packetData[1500] = {};
  packet.buffer.data = &packetData;
  packet.buffer.size = sizeof (packetData);
  packet.isIec61883Packet = true;
  // uint32_t expectedBytesWritten = packet.buffer.size - (packet.buffer.size % 192u);

  /* Write one packet */
  //ASSERT_EQ(expectedBytesWritten, mLocalVideoBuffer->writeMpegTS(&packet));
  ASSERT_EQ(0, mLocalVideoBuffer->writeMpegTS(&packet));
}

TEST_F(IasTestLocalVideoBuffer, getInternalBuffers)
{
  ASSERT_TRUE(NULL != mLocalVideoBuffer);
  ASSERT_TRUE(mLocalVideoBuffer->getInternalBuffers());
}
