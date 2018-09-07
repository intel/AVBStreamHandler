/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestAvbStream.cpp
 * @date 2018
 */

#include "gtest/gtest.h"


#define private public
#define protected public
#include "avb_streamhandler/IasAvbStream.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"

#include <arpa/inet.h>

using namespace IasMediaTransportAvb;

namespace IasMediaTransportAvb {

  class IasAvbStreamMock: public IasAvbStream
  {
    public:

      IasAvbStreamMock(DltContext &dltContext, const IasAvbStreamType streamType):
        IasAvbStream(dltContext, streamType)
      {}
      ~IasAvbStreamMock() {}


      virtual void readFromAvbPacket(const void* packet, size_t length) { (void)packet; (void) length; }
      virtual bool writeToAvbPacket(IasAvbPacket* packet, uint64_t n) { (void)packet; (void)n; return false; }
      virtual void derivedCleanup() {}
  };

class IasTestAvbStream : public ::testing::Test
{
protected:
  IasTestAvbStream()
  : mStream()
  , mStreamHandler(DLT_LOG_INFO)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbStream()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
                  "TEST",
                  "IasTestAvbStream",
                  DLT_LOG_INFO,
                  DLT_TRACE_STATUS_OFF);
    mStream = new IasAvbStreamMock(mDltCtx, eIasAvbAudioStream);
//    dlt_enable_local_print();
  }

  virtual void TearDown()
  {
    mStreamHandler.cleanup();
    delete mStream;
    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }

  bool initStreamHandler()
  {
    // IT HAS TO BE SET TO ZERO - getopt_long - DefaultConfig_passArguments - needs to be reinitialized before use
    optind = 0;

    if (!IasSpringVilleInfo::fetchData())
    {
      return false;
    }

    const char *args[] = {
      "setup",
      "-t", "Fedora",
      "-p", "UnitTests",
      "-n", IasSpringVilleInfo::getInterfaceName()
    };
    int argc = sizeof(args) / sizeof(args[0]);

    IasAvbProcessingResult result = mStreamHandler.init(theConfigPlugin, true, argc, (char**)args);

    return (result == eIasAvbProcOK);
  }

  IasAvbStreamMock * mStream;
  DltContext mDltCtx;
  IasAvbStreamHandler mStreamHandler;
};
} // namespace IasMediaTransportAvb

TEST_F(IasTestAvbStream, Cleanup)
{
  ASSERT_TRUE(mStream != NULL);

  mStream->cleanup();
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbStream, GetStreamType)
{
  ASSERT_TRUE(mStream != NULL);

  IasAvbStreamType result = eIasAvbAudioStream;
  result = mStream->getStreamType();

  ASSERT_EQ(eIasAvbAudioStream, result);
}

TEST_F(IasTestAvbStream, IsInitialized)
{
  ASSERT_TRUE(mStream != NULL);

  bool result = mStream->isInitialized();
  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStream, IsTransmitStream)
{
  ASSERT_TRUE(mStream != NULL);

  bool result = mStream->isTransmitStream();
  ASSERT_TRUE(result);
}

TEST_F(IasTestAvbStream, IsReceiveStream)
{
  ASSERT_TRUE(mStream != NULL);

  bool result = mStream->isReceiveStream();
  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStream, IsActive)
{
  ASSERT_TRUE(mStream != NULL);

  bool result = mStream->isActive();
  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStream, PreparePacket)
{
  ASSERT_TRUE(mStream != NULL);

  IasAvbPacket* result = mStream->preparePacket(0u);
  ASSERT_TRUE(result == NULL);

  IasAvbTSpec tSpec(1u, IasAvbSrClass::eIasAvbSrClassLow);
  mStream->mTSpec = &tSpec;
  IasAvbPacketPool* pool = new IasAvbPacketPool(mDltCtx);
  pool->mBase = NULL;
  mStream->mPacketPool = pool;

  ASSERT_EQ(NULL, mStream->preparePacket(0u));

  delete pool;
  mStream->mPacketPool = NULL;
  mStream->mTSpec = NULL;
}

TEST_F(IasTestAvbStream, ActiveDeactive)
{
  ASSERT_TRUE(mStream != NULL);

  mStream->activate(true);
  mStream->activate();
  mStream->deactivate(true);
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbStream, HookClockDomain)
{
  ASSERT_TRUE(mStream != NULL);

  IasAvbClockDomain* clockDomain = 0;
  IasAvbProcessingResult result = mStream->hookClockDomain(clockDomain);

  ASSERT_EQ(eIasAvbProcNotInitialized, result);
}

TEST_F(IasTestAvbStream, InitTransmit_Receive_Common)
{
  ASSERT_TRUE(mStream != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbStreamId avbStreamIdObj;
  IasAvbClockDomain* avbClockDomainObj = 0;
  IasAvbMacAddress avbMacAddr = {0};
  IasAvbTSpec tSpec(0, IasAvbSrClass::eIasAvbSrClassHigh);

  result = mStream->initTransmit(tSpec, avbStreamIdObj, 0, avbClockDomainObj, avbMacAddr, 0, true);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mStream->initReceive(tSpec, avbStreamIdObj, avbMacAddr, 0, true);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbStream, ActivationChanged)
{
  ASSERT_TRUE(mStream != NULL);

  mStream->activationChanged();
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbStream, GetVlanData)
{
  ASSERT_TRUE(mStream != NULL);

  uint16_t result = mStream->getVlanData();
  ASSERT_EQ(result, 0);
}

TEST_F(IasTestAvbStream, GetPresentationTimeOffset)
{
  ASSERT_TRUE(mStream != NULL);

  uint32_t result = mStream->getPresentationTimeOffset();
  ASSERT_EQ(result, 0);
}

TEST_F(IasTestAvbStream, BRANCH_Receiver_Transmitter)
{
  ASSERT_TRUE(mStream != NULL);

  ASSERT_TRUE(initStreamHandler());

  IasAvbTSpec tspec(1, IasAvbSrClass::eIasAvbSrClassHigh);
  IasAvbStreamId streamID((uint64_t)1);
  IasAvbMacAddress macAddr = {0xff};
  IasAvbHwCaptureClockDomain clockDamain;

  IasAvbProcessingResult result = eIasAvbProcErr;

  result = mStream->initReceive(tspec, streamID, macAddr, 1, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  (void)mStream->getTSpec();
  (void)mStream->getStreamState();

  IasAvbPacket* resultPtr = mStream->preparePacket(0u);
  ASSERT_TRUE(NULL == resultPtr);

  IasAvbClockDomain* clockDomain = 0;
  result = mStream->hookClockDomain(clockDomain);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mStream->initReceive(tspec, streamID, macAddr, 1, true);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  clockDomain = 0;
  result = mStream->hookClockDomain(clockDomain);
  ASSERT_EQ(eIasAvbProcOK, result);

  mStream->cleanup();

  result = mStream->initTransmit(tspec, streamID, 1, &clockDamain, macAddr, 1, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  resultPtr = mStream->preparePacket(0u);
  /*ASSERT_EQ(NULL, resultPtr);*/

  result = mStream->resetPacketPool();
  ASSERT_EQ(result, eIasAvbProcOK);

  result = mStream->initTransmit(tspec, streamID, 1, &clockDamain, macAddr, 1, true);
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  mStream->cleanup();

}

TEST_F(IasTestAvbStream, BRANCH_HookClockDomain)
{
  ASSERT_TRUE(mStream != NULL);

  ASSERT_TRUE(initStreamHandler());

  IasAvbProcessingResult result = eIasAvbProcErr;

  IasAvbTSpec tspec(1, IasAvbSrClass::eIasAvbSrClassHigh);
  IasAvbStreamId streamID((uint64_t)1);
  IasAvbMacAddress macAddr = {0xff};
  IasAvbHwCaptureClockDomain clockDamain;

  result = mStream->initTransmit(tspec, streamID, 1, &clockDamain, macAddr, 1, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mStream->hookClockDomain(&clockDamain);
  ASSERT_EQ(eIasAvbProcErr, result);

  mStream->cleanup();

  result = mStream->initReceive(tspec, streamID, macAddr, 1, true);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mStream->hookClockDomain(&clockDamain);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mStream->hookClockDomain(&clockDamain);
  ASSERT_EQ(eIasAvbProcAlreadyInUse, result);
}

TEST_F(IasTestAvbStream, initTransmit)
{
  ASSERT_TRUE(mStream != NULL);

  IasAvbStreamId avbStreamIdObj;
  IasAvbClockDomain* avbClockDomainObj = new IasAvbHwCaptureClockDomain();
  IasAvbMacAddress avbMacAddr = {0};
  IasAvbTSpec tSpec(1u, IasAvbSrClass::eIasAvbSrClassHigh);
  uint32_t poolSize = 0u;
  uint16_t vid = 0u;
  // (NULL == clockDomain)                   (F)
  // || (0u == tSpec.getMaxFrameSize())      (F)
  // || (0u == tSpec.getMaxIntervalFrames()) (F)
  // || (0u == poolSize)                     (T)
  ASSERT_EQ(eIasAvbProcInvalidParam, mStream->initTransmit(tSpec, avbStreamIdObj, poolSize,
                                                        avbClockDomainObj,avbMacAddr, vid, true));

  mStream->cleanup();

  IasAvbTSpec zeroFrameSizeTSpec(0u, IasAvbSrClass::eIasAvbSrClassHigh);
  poolSize = 1u;
  // (NULL == clockDomain)                   (F)
  // || (0u == tSpec.getMaxFrameSize())      (T)
  // || (0u == tSpec.getMaxIntervalFrames()) (F)
  // || (0u == poolSize)                     (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mStream->initTransmit(zeroFrameSizeTSpec, avbStreamIdObj, poolSize,
                                                        avbClockDomainObj,avbMacAddr, vid, true));

  mStream->cleanup();

  IasAvbTSpec zeroIntervalFramesTSpec(1u, IasAvbSrClass::eIasAvbSrClassHigh, 0u);
  poolSize = 1u;
  // (NULL == clockDomain)                   (F)
  // || (0u == tSpec.getMaxFrameSize())      (F)
  // || (0u == tSpec.getMaxIntervalFrames()) (T)
  // || (0u == poolSize)                     (F)
  ASSERT_EQ(eIasAvbProcInvalidParam, mStream->initTransmit(zeroIntervalFramesTSpec, avbStreamIdObj, poolSize,
                                                        avbClockDomainObj,avbMacAddr, vid, true));

  delete avbClockDomainObj;
  avbClockDomainObj = nullptr;
}

TEST_F(IasTestAvbStream, dispatchPacket)
{
  ASSERT_TRUE(mStream != NULL);
  ASSERT_TRUE(initStreamHandler());

  IasAvbTSpec tspecHigh(1, IasAvbSrClass::eIasAvbSrClassHigh);
  IasAvbTSpec tspecLow(1, IasAvbSrClass::eIasAvbSrClassLow);
  IasAvbStreamId streamID((uint64_t)1u);
  IasAvbMacAddress macAddr = {0xff};
  uint16_t vid = 1u;

  ASSERT_EQ(eIasAvbProcOK, mStream->initReceive(tspecHigh, streamID, macAddr, vid, true));

  IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
  ASSERT_TRUE(NULL != ptp);
  uint64_t now = ptp->getLocalTime();

  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);

  uint8_t* const avtpBase8 = packet;
  uint16_t* const avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);
  uint32_t* const avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase16);
  // isInitialized() && isReceiveStream() - all variants
  mStream->mDirection = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  mStream->dispatchPacket(packet, sizeof packet, now);
  mStream->mDirection = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  IasAvbTSpec *tempTSpec = mStream->mTSpec;
  mStream->mTSpec = NULL;
  mStream->dispatchPacket(packet, sizeof packet, now);
  mStream->mTSpec = tempTSpec;

  *(avtpBase8 + 1) = 0u; // timestamp invalid
  mStream->dispatchPacket(packet, sizeof packet, now);

  *(avtpBase8 + 1) = 1u; // timestamp valid bit
  *(avtpBase32 + 3) = htonl((uint32_t)(now)); // packet is late
  // delta = 0
  mStream->dispatchPacket(packet, sizeof packet, now);

  *(avtpBase32 + 3) = htonl((uint32_t)(now - 1000)); // packet is late
  // delta < 0
  mStream->dispatchPacket(packet, sizeof packet, now);

  *(avtpBase32 + 3) = htonl((uint32_t)(now + 3000000)); // Time stamp further than maxTransitTime
  // delta > maxTransitTime
  mStream->dispatchPacket(packet, sizeof packet, now);

  mStream->cleanup();

  ASSERT_EQ(eIasAvbProcOK, mStream->initReceive(tspecLow, streamID, macAddr, vid, true));
  *(avtpBase32 + 3) = htonl((uint32_t)(now - 1000)); // packet is late
  // delta < 0
  mStream->dispatchPacket(packet, sizeof packet, now);

  *(avtpBase32 + 3) = htonl((uint32_t)(now + 1500000000)); // Time stamp further than maxTransitTime
  // delta > maxTransitTime
  mStream->dispatchPacket(packet, sizeof packet, now);
}

TEST_F(IasTestAvbStream, changeStreamId)
{
  ASSERT_TRUE(mStream != NULL);
  IasAvbTSpec tspecHigh(1, IasAvbSrClass::eIasAvbSrClassHigh);
  IasAvbStreamId streamID((uint64_t)1u);
  IasAvbMacAddress macAddr = {0xff};
  uint16_t vid = 1u;

  ASSERT_EQ(eIasAvbProcOK, mStream->initReceive(tspecHigh, streamID, macAddr, vid, true));

  uint64_t streamId = uint64_t(mStream->mAvbStreamId);
  IasAvbStreamId changedStreamId(streamId + 1);

  mStream->changeStreamId(changedStreamId);

  ASSERT_EQ(changedStreamId, uint64_t(*mStream->mAvbStreamId));

  IasAvbStreamId origAvbStreamId(streamId);
  mStream->changeStreamId(origAvbStreamId);

  ASSERT_EQ(streamId, uint64_t(*mStream->mAvbStreamId));
}

TEST_F(IasTestAvbStream, adjustPresentationTimeOffset)
{
  ASSERT_TRUE(mStream != NULL);

  ASSERT_EQ(0u, mStream->adjustPresentationTimeOffset(0u));

  ASSERT_EQ(0u, mStream->adjustPresentationTimeOffset(1u));
}

TEST_F(IasTestAvbStream, incFramesTx)
{
  ASSERT_TRUE(mStream != NULL);

  ASSERT_EQ(1u, mStream->incFramesTx());
}
