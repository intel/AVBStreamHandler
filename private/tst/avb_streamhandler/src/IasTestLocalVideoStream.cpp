/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestAlsaWorkerThread.cpp
 * @brief   The implementation of the IasTestLocalVideoStream test class.
 * @date    2018
 */

#include "gtest/gtest.h"


#define private public
#define protected public
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "test_common/IasSpringVilleInfo.hpp"
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#define protected protected
#define private private

using namespace IasMediaTransportAvb;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;


namespace IasMediaTransportAvb {

class IasTestLocalVideoStream : public IasLocalVideoStream, public ::testing::Test
{
public:
  class TestClient : public IasLocalVideoStreamClientInterface
  {
    public:
      TestClient() {}
      virtual ~TestClient() {}

      virtual bool signalDiscontinuity( DiscontinuityEvent event, uint32_t numSamples )
      {
        (void) event;
        (void) numSamples;

        return false;
      }
  };

protected:
  IasTestLocalVideoStream()
  : IasLocalVideoStream(mDltCtx, IasAvbStreamDirection::eIasAvbTransmitToNetwork, eIasTestToneStream, 0)
  , mEnvironment(NULL)
  , streamHandler(DLT_LOG_INFO)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestLocalVideoStream()
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
              "IasTestLocalVideoStream",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);
  }

  virtual void TearDown()
  {
    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    streamHandler.cleanup();

    heapSpaceLeft = heapSpaceInitSize;

    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }


  virtual void readFromAvbPacket(const void* packet, size_t length) { (void)packet; (void) length; }
  virtual bool writeToAvbPacket(IasAvbPacket* packet) { (void)packet; return false; }
  virtual void derivedCleanup() {}
  virtual IasAvbProcessingResult resetBuffers() { return eIasAvbProcOK; }

  DltContext mDltCtx;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  IasAvbStreamHandler streamHandler;
};
} // namespace IasMediaTransportAvb

TEST_F(IasTestLocalVideoStream, CTor_DTor)
{
  ASSERT_TRUE(this != NULL);
  ASSERT_TRUE(1);
}

TEST_F(IasTestLocalVideoStream, getClient)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_TRUE(NULL == this->getClient());
}

TEST_F(IasTestLocalVideoStream, getClientState)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_EQ(eIasNotConnected, this->getClientState());
}

TEST_F(IasTestLocalVideoStream, getDirection)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_EQ(IasAvbStreamDirection::eIasAvbTransmitToNetwork, this->getDirection());
}

TEST_F(IasTestLocalVideoStream, getLocalVideoBuffer)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_TRUE(NULL == this->getLocalVideoBuffer());
}

TEST_F(IasTestLocalVideoStream, getType)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_TRUE(eIasTestToneStream == this->getType());
}

TEST_F(IasTestLocalVideoStream, isInitialized)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_FALSE(this->isInitialized());
}

TEST_F(IasTestLocalVideoStream, isConnected)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_FALSE(this->isConnected());
}

TEST_F(IasTestLocalVideoStream, getMaxPacketRate)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_EQ(0, this->getMaxPacketRate());
}

TEST_F(IasTestLocalVideoStream, getMaxPacketSize)
{
  ASSERT_TRUE(this != NULL);

  ASSERT_EQ(0, this->getMaxPacketSize());
}

TEST_F(IasTestLocalVideoStream, init)
{
  ASSERT_TRUE(this != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 0;
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 0;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcInitializationFailed, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));
}

TEST_F(IasTestLocalVideoStream, attributes)
{
  ASSERT_TRUE(this != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 2u;
  uint16_t maxPacketRate = 4000u;
  uint16_t maxPacketSize = 100u;
  bool internalBuffers = true;

  ASSERT_EQ(eIasAvbProcOK, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));

  IasLocalVideoStreamAttributes attrs;
  attrs.setDirection(this->getDirection());
  attrs.setType(this->getType());
  attrs.setStreamId(this->getStreamId());
  attrs.setFormat(this->getFormat());
  attrs.setMaxPacketRate(this->getMaxPacketRate());
  attrs.setMaxPacketSize(this->getMaxPacketSize());
  attrs.setInternalBuffers(this->getLocalVideoBuffer()->getInternalBuffers());
}

TEST_F(IasTestLocalVideoStream, initInvalidMemorySize)
{
  ASSERT_TRUE(this != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 2;
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 1300;
  bool internalBuffers = true;
  heapSpaceLeft = 0;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));
}


TEST_F(IasTestLocalVideoStream, initInvalidPktSize)
{
  ASSERT_TRUE(this != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 0;
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 1600;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcInvalidParam, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));
}

TEST_F(IasTestLocalVideoStream, initInvalidNumPackets)
{
  ASSERT_TRUE(this != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 0;
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 1500;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));
}


TEST_F(IasTestLocalVideoStream, readLocalVideoBuffer)
{
  ASSERT_TRUE(this != NULL);
  IasLocalVideoBuffer *buffer = NULL;
  IasLocalVideoBuffer::IasVideoDesc *descPacket = NULL;

  ASSERT_EQ(eIasAvbProcNotInitialized, this->readLocalVideoBuffer(buffer, descPacket));

  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 2;
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 1500;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));

  IasLocalVideoBuffer * tempBuffer = this->mLocalVideoBuffer;
  descPacket = new IasLocalVideoBuffer::IasVideoDesc();
  this->mLocalVideoBuffer = NULL;

  IasAvbProcessingResult result = this->readLocalVideoBuffer(tempBuffer, descPacket);

  delete descPacket;

  // NULL != getLocalVideoBuffer() (F)
  ASSERT_EQ(eIasAvbProcNullPointerAccess, result);
}

TEST_F(IasTestLocalVideoStream, setClientActive)
{
  ASSERT_TRUE(this != NULL);

  bool active = false;
  this->setClientActive(active);
}

TEST_F(IasTestLocalVideoStream, connect)
{
  ASSERT_TRUE(this != NULL);
  IasLocalVideoStreamClientInterface *client = NULL;

  ASSERT_EQ(eIasAvbProcInvalidParam, this->connect(client));
}

TEST_F(IasTestLocalVideoStream, setAvbPacketPool)
{
  ASSERT_TRUE(this != NULL);

  IasAvbPacketPool *avbPacketPool = NULL;
  ASSERT_EQ(eIasAvbProcNullPointerAccess, this->setAvbPacketPool(avbPacketPool));
}

TEST_F(IasTestLocalVideoStream, disconnect)
{
  ASSERT_TRUE(this != NULL);
  ASSERT_EQ(eIasAvbProcOK, this->disconnect());
}

TEST_F(IasTestLocalVideoStream, writeLocalVideoBuffer)
{
  ASSERT_TRUE(this != NULL);
  IasLocalVideoBuffer *buffer = NULL;
  IasLocalVideoBuffer::IasVideoDesc* descPacket = NULL;

  ASSERT_EQ(eIasAvbProcOK, this->writeLocalVideoBuffer(buffer, descPacket));
}

TEST_F(IasTestLocalVideoStream, init_connect_disconnect)
{
  ASSERT_TRUE(this != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t numPackets = 2;
  uint16_t maxPacketRate = 0;
  uint16_t maxPacketSize = 1500;
  bool internalBuffers = false;

  ASSERT_EQ(eIasAvbProcOK, this->init(format, numPackets, maxPacketRate, maxPacketSize, internalBuffers));

  TestClient client;
  ASSERT_EQ(eIasAvbProcOK, this->connect(&client));

  ASSERT_EQ(eIasAvbProcAlreadyInUse, this->connect(&client));

  IasAvbPacketPool pool(mDltCtx);

  ASSERT_EQ(eIasAvbProcOK, this->setAvbPacketPool(&pool));

  this->setClientActive(false);
  this->setClientActive(true);
  this->setClientActive(true);

  IasLocalVideoBuffer buffer;
  IasLocalVideoBuffer::IasVideoDesc descPacket;

  ASSERT_EQ(eIasAvbProcOK, this->readLocalVideoBuffer(NULL, NULL));
  ASSERT_EQ(eIasAvbProcOK, this->readLocalVideoBuffer(NULL, &descPacket));
  ASSERT_EQ(eIasAvbProcOK, this->readLocalVideoBuffer(&buffer, NULL));
  ASSERT_EQ(eIasAvbProcOK, this->readLocalVideoBuffer(&buffer, &descPacket));
}


TEST_F(IasTestLocalVideoStream, getStreamId)
{
  ASSERT_TRUE(this != NULL);
  ASSERT_EQ(0, this->getStreamId());
}


TEST_F(IasTestLocalVideoStream, attrs)
{
  IasLocalVideoStreamAttributes attrs;

  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  IasLocalStreamType type = IasLocalStreamType::eIasLocalVideoInStream;
  uint16_t streamId = 0u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  uint16_t maxPacketRate = 1u;
  uint16_t maxPacketSize = 2u;
  bool internalBuffers = true;
  IasLocalVideoStreamAttributes otherAttrs(direction, type, streamId, format, maxPacketRate, maxPacketSize, internalBuffers);

  ASSERT_FALSE(attrs == otherAttrs);
  attrs = otherAttrs;
  attrs.setDirection(otherAttrs.getDirection());
  attrs.setFormat(otherAttrs.getFormat());
  attrs.setInternalBuffers(otherAttrs.getInternalBuffers());
  attrs.setMaxPacketRate(otherAttrs.getMaxPacketRate());
  attrs.setMaxPacketSize(otherAttrs.getMaxPacketSize());
  attrs.setType(otherAttrs.getType());
  attrs.setStreamId(otherAttrs.getStreamId());

  IasLocalVideoStreamAttributes copyAttrs(otherAttrs);
  copyAttrs.setStreamId(100u);
  ASSERT_TRUE(attrs != copyAttrs);
}
