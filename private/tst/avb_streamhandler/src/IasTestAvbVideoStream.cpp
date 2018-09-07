/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestAvbVideoStream.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbVideoStream.hpp"
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef private
#undef protected

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "test_common/IasSpringVilleInfo.hpp"
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"

#include <netinet/in.h>
#include <cstring>

using namespace IasMediaTransportAvb;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb {

class LocalVideoStream: public IasLocalVideoStream
{
  public:
    LocalVideoStream(const IasAvbStreamDirection direction, DltContext context, uint16_t localStreamId = 0u):
      IasLocalVideoStream(
          context
          , direction
          , IasLocalStreamType::eIasTestToneStream
          , localStreamId)
    {
    }

    virtual ~LocalVideoStream()
    {
    }

    virtual void readFromAvbPacket(const void* packet, size_t length) { (void)packet; (void) length; }
    virtual bool writeToAvbPacket(IasAvbPacket* packet) { (void)packet; return false; }
    virtual void derivedCleanup() {}
    virtual IasAvbProcessingResult resetBuffers() { return eIasAvbProcOK; }
};

class IasTestAvbVideoStream : public ::testing::Test
{
public:
  IasTestAvbVideoStream() :
    mAvbVideoStream(NULL)
  , streamHandler(DLT_LOG_INFO)
  , mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbVideoStream()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

//    dlt_enable_local_print();

    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
              "TEST",
              "IasTestAvbVideoStream",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);

    mAvbVideoStream = new (nothrow) IasAvbVideoStream();
  }

  virtual void TearDown()
  {
    delete mAvbVideoStream;
    mAvbVideoStream = NULL;

    if (mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    streamHandler.cleanup();

    DLT_UNREGISTER_CONTEXT(mDltCtx);
    heapSpaceLeft = heapSpaceInitSize;
  }


  virtual void readFromAvbPacket(const void* packet, size_t length) { (void)packet; (void) length; }
  virtual bool writeToAvbPacket(IasAvbPacket* packet) { (void)packet; return false; }
  virtual void derivedCleanup() {}

  IasAvbProcessingResult initStreamHandler()
  {
    // IT HAS TO BE SET TO ZERO - getopt_long - DefaultConfig_passArguments - needs to be reinitialized before use
    optind = 0;

    if (!IasSpringVilleInfo::fetchData())
    {
      return eIasAvbProcErr;
    }

    const char *args[] = {
      "setup",
      "-t", "Fedora",
      "-p", "UnitTests",
      "-n", IasSpringVilleInfo::getInterfaceName()
    };
    int argc = sizeof(args) / sizeof(args[0]);

    IasAvbProcessingResult ret = streamHandler.init(theConfigPlugin, true, argc, (char**)args);
    if (eIasAvbProcOK == ret)
    {
      IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(IasRegKeys::cCompatibilityVideo, "D5_1722a");
    }

    return ret;
  }

  bool createEnvironment()
  {
    if(NULL != mEnvironment)
    {
      return false;
    }

    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    if (mEnvironment)
    {
      mEnvironment->registerDltContexts();
      mEnvironment->setDefaultConfigValues();

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName()))
        {
          if (eIasAvbProcOK == mEnvironment->createIgbDevice())
          {
            if (eIasAvbProcOK == mEnvironment->createPtpProxy())
            {
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  IasAvbResult setConfigValue(const std::string& key, uint64_t value)
  {
    IasAvbResult result;
    IasAvbStreamHandlerEnvironment::mInstance;
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
    result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
    return result;
  }

  IasAvbResult setConfigValue(const std::string& key, const std::string &value)
  {
    IasAvbResult result;
    IasAvbStreamHandlerEnvironment::mInstance;
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
    result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
    return result;
  }

  DltContext mDltCtx;
  IasAvbVideoStream *mAvbVideoStream;
  IasAvbStreamHandler streamHandler;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

} // namespace IasMediaTransportAvb

TEST_F(IasTestAvbVideoStream, CTor_DTor)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbVideoStream, isConnected)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);

  ASSERT_FALSE(mAvbVideoStream->isConnected());
}

TEST_F(IasTestAvbVideoStream, initTransmit_mpegts)
{
  ASSERT_TRUE(createEnvironment());
  ASSERT_TRUE(mAvbVideoStream != NULL);
  uint16_t maxPacketRate = 0u;
  uint16_t maxPacketSize = 0u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  IasAvbStreamId  streamId(123);
  uint32_t poolSize = 1000u;
  IasAvbPtpClockDomain clockDomain;
  IasAvbMacAddress dmac = {};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  bool preconfigured    = true;

  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->initTransmit(srClass,
                                                                   maxPacketRate,
                                                                   maxPacketSize,
                                                                   format,
                                                                   streamId,
                                                                   poolSize,
                                                                   &clockDomain,
                                                                   dmac,
                                                                   preconfigured));

  maxPacketRate = 4000u;

  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->initTransmit(srClass,
                                                                   maxPacketRate,
                                                                   maxPacketSize,
                                                                   format,
                                                                   streamId,
                                                                   poolSize,
                                                                   &clockDomain,
                                                                   dmac,
                                                                   preconfigured));

  maxPacketSize = 1464u;
  format        = (IasMediaTransportAvb::IasAvbVideoFormat)2u;

  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAvbVideoStream->initTransmit(srClass,
                                                                        maxPacketRate,
                                                                        maxPacketSize,
                                                                        format,
                                                                        streamId,
                                                                        poolSize,
                                                                        &clockDomain,
                                                                        dmac,
                                                                        preconfigured));

  format  = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
}

TEST_F(IasTestAvbVideoStream, initTransmit_h264)
{
  ASSERT_TRUE(createEnvironment());
  ASSERT_TRUE(mAvbVideoStream != NULL);
  uint16_t maxPacketRate = 4000u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId = IasAvbStreamId(124);
  uint32_t poolSize = 1000u;
  IasAvbPtpClockDomain clockDomain;
  IasAvbMacAddress dmac = {};
  bool preconfigured = true;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;

  uint16_t maxPacketSize = 1465u;
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAvbVideoStream->initTransmit(srClass,
                                                                        maxPacketRate,
                                                                        maxPacketSize,
                                                                        format,
                                                                        streamId,
                                                                        poolSize,
                                                                        &clockDomain,
                                                                        dmac,
                                                                        preconfigured));

  maxPacketSize = 1464u;
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
  ASSERT_EQ(IasAvbVideoStream::eCompCurrent, mAvbVideoStream->mCompatibility);

  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbVideoStream->initTransmit(srClass,
                                                                           maxPacketRate,
                                                                           maxPacketSize,
                                                                           format,
                                                                           streamId,
                                                                           poolSize,
                                                                           &clockDomain,
                                                                           dmac,
                                                                           preconfigured));
}

TEST_F(IasTestAvbVideoStream, initReceive_mpegts)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  ASSERT_TRUE(createEnvironment());
  uint16_t maxPacketRate = 1u;
  uint16_t maxPacketSize = 0;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  IasAvbStreamId  streamId;
  IasAvbMacAddress dmac = {};
  uint16_t vid = 0;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  bool preconfigured = true;

  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->initReceive(srClass,
                                                                  maxPacketRate,
                                                                  maxPacketSize,
                                                                  format,
                                                                  streamId,
                                                                  dmac,
                                                                  vid,
                                                                  preconfigured));

  maxPacketRate = 0u;

  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->initReceive(srClass,
                                                                  maxPacketRate,
                                                                  maxPacketSize,
                                                                  format,
                                                                  streamId,
                                                                  dmac,
                                                                  vid,
                                                                  preconfigured));

  maxPacketRate = 42u;
  maxPacketSize = 1501u;
  // (1500u < maxPacketSize)                                       (F)
  // || ( (IasAvbVideoFormat::eIasAvbVideoFormatRtp != format)     (T)
  // &&(IasAvbVideoFormat::eIasAvbVideoFormatIec61883 != format) ) (F)
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAvbVideoStream->initReceive(srClass,
                                                                  maxPacketRate,
                                                                  maxPacketSize,
                                                                  format,
                                                                  streamId,
                                                                  dmac,
                                                                  vid,
                                                                  preconfigured));

  maxPacketSize = 1500u;
  format        = (IasMediaTransportAvb::IasAvbVideoFormat)2u;
  srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  // (1500u < maxPacketSize)                                       (F)
  // || ( (IasAvbVideoFormat::eIasAvbVideoFormatRtp != format)     (T)
  // &&(IasAvbVideoFormat::eIasAvbVideoFormatIec61883 != format) ) (T)
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mAvbVideoStream->initReceive(srClass,
                                                                  maxPacketRate,
                                                                  maxPacketSize,
                                                                  format,
                                                                  streamId,
                                                                  dmac,
                                                                  vid,
                                                                  preconfigured));

  format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  srClass = IasAvbSrClass::eIasAvbSrClassLow;
  // (1500u < maxPacketSize)                                 (F)
  // || (IasAvbVideoFormat::eIasAvbVideoFormatRtp != format) (T)
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(srClass,
                                                        maxPacketRate,
                                                        maxPacketSize,
                                                        format,
                                                        streamId,
                                                        dmac,
                                                        vid,
                                                        preconfigured));
}

TEST_F(IasTestAvbVideoStream, initReceive_h264)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  uint16_t maxPacketRate = 4000u;
  uint16_t maxPacketSize = 1464u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId;
  IasAvbMacAddress dmac = {};
  uint16_t vid = 0;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  bool preconfigured = true;

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(srClass,
                                                        maxPacketRate,
                                                        maxPacketSize,
                                                        format,
                                                        streamId,
                                                        dmac,
                                                        vid,
                                                        preconfigured));

  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbVideoStream->initReceive(srClass,
                                                                  maxPacketRate,
                                                                  maxPacketSize,
                                                                  format,
                                                                  streamId,
                                                                  dmac,
                                                                  vid,
                                                                  preconfigured));
}

// TODO: segfault to investigate
//TEST_F(IasTestAvbVideoStream, connectTo)
//{
//  ASSERT_TRUE(mAvbVideoStream != NULL);
//  IasLocalVideoStream* localStream = NULL;

//  ASSERT_EQ(eIasAvbProcNotInitialized, mAvbVideoStream->connectTo(localStream));

//  uint16_t maxPacketRate = 42u;
//  uint16_t maxPacketSize = 1500u;
//  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
//  IasAvbStreamId  streamId;
//  IasAvbMacAddress dmac = {0};
//  uint16_t vid = 0;
//  uint16_t numPackets = 2u;
//  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
//  bool preconfigured = true;
//  bool noInternalBuffers = false;

//  localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbReceiveFromNetwork, mDltCtx);

//  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, noInternalBuffers);

//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(srClass,
//                                                        maxPacketRate,
//                                                        maxPacketSize,
//                                                        format,
//                                                        streamId,
//                                                        dmac,
//                                                        vid,
//                                                        preconfigured));

//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));

//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));

//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
//  delete localStream;
//  localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbTransmitToNetwork, mDltCtx);
//  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, noInternalBuffers);

//  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->connectTo(localStream));

//  localStream->mMaxPacketSize = maxPacketSize - 1;

//  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->connectTo(localStream));

//  localStream->mMaxPacketRate = maxPacketRate - 1;

//  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->connectTo(localStream));

//  mAvbVideoStream->mMaxPacketSize = 0u;

//  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->connectTo(localStream));

//  mAvbVideoStream->mMaxPacketRate = 0u;

//  ASSERT_EQ(eIasAvbProcInvalidParam, mAvbVideoStream->connectTo(localStream));
//  uint32_t poolSize = 2u;
//  IasAvbPtpClockDomain clockDomain;

//  delete mAvbVideoStream;
//  mAvbVideoStream = new (nothrow) IasAvbVideoStream();

//  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

//  maxPacketSize = 1464u;
//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
//                                                         maxPacketRate,
//                                                         maxPacketSize,
//                                                         format,
//                                                         streamId,
//                                                         poolSize,
//                                                         &clockDomain,
//                                                         dmac,
//                                                         preconfigured));
//  mAvbVideoStream->activate();

//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
//  delete localStream;
//  localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbTransmitToNetwork, mDltCtx);
//  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, noInternalBuffers);

//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));
//  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));

//  delete localStream;
//}

TEST_F(IasTestAvbVideoStream, getFormatCode)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;

  ASSERT_EQ((uint8_t)0u, mAvbVideoStream->getFormatCode(format));

  ASSERT_EQ((uint8_t)0u, mAvbVideoStream->getFormatCode((IasAvbVideoFormat)2));
}

TEST_F(IasTestAvbVideoStream, getVideoFormatCode)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;

  ASSERT_EQ((uint8_t)0u, mAvbVideoStream->getVideoFormatCode(format));

  ASSERT_EQ((uint8_t)0u, mAvbVideoStream->getVideoFormatCode((IasAvbVideoFormat)2));
}

TEST_F(IasTestAvbVideoStream, activationChanged)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);

  mAvbVideoStream->activationChanged();
  ASSERT_TRUE(1);

  uint16_t maxPacketRate = 42u;
  uint16_t maxPacketSize = 1500u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId;
  IasAvbMacAddress dmac = {0};
  uint16_t vid = 0;
  uint16_t numPackets = 2u;
  bool preconfigured = true;
  bool noInternalBuffers = false;

  IasLocalVideoStream* localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbReceiveFromNetwork, mDltCtx);

  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, noInternalBuffers);

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(IasAvbSrClass::eIasAvbSrClassLow,
                                                        maxPacketRate,
                                                        maxPacketSize,
                                                        format,
                                                        streamId,
                                                        dmac,
                                                        vid,
                                                        preconfigured));

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));
  // run for connected stream
  mAvbVideoStream->activationChanged();
  ASSERT_EQ(IasLocalVideoStream::ClientState::eIasIdle, mAvbVideoStream->mLocalStream->mClientState);

  mAvbVideoStream->cleanup();
  delete localStream;
  localStream = NULL;
}

TEST_F(IasTestAvbVideoStream, readFromAvbPacket)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  void *nullPacket = NULL;

  mAvbVideoStream->readFromAvbPacket(nullPacket, 0);

  uint16_t maxPacketRate = 4000;
  uint16_t maxPacketSize = 1024;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId;
  IasAvbMacAddress dmac = {0};
  uint16_t vid = 0;
  uint32_t poolSize = 2048;
  IasAvbHwCaptureClockDomain clockDomain;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t numPackets     = 4u;
  bool useInternalBuffers = true;
  bool preconfigured = true;

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));

  mAvbVideoStream->readFromAvbPacket(nullPacket, 0);

  delete mAvbVideoStream;
  mAvbVideoStream = new (nothrow) IasAvbVideoStream();

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(IasAvbSrClass::eIasAvbSrClassLow, maxPacketRate, maxPacketSize, format, streamId, dmac, vid, true));

  mAvbVideoStream->readFromAvbPacket(nullPacket, 0);

  LocalVideoStream * localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbReceiveFromNetwork, mDltCtx);
  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, useInternalBuffers);
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));

  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateNever;
  mAvbVideoStream->mValidationCount = 1u;
  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);
  uint8_t * avtpBase8 = packet;
  uint16_t * avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);
  uint32_t * avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase16);
  avtpBase8[0] = 0x03u;   // AVTP compressed video format
  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  avtpBase8[12] = 0u; // 12 byte RTP header
  avtpBase8[16] = 0x02u;  // RFC Video payload type
  avtpBase16[10] = htons(uint8_t(IasAvbVideoStream::cAvtpHeaderSize)); // payloadLength
  avtpBase32[3] = 24000u; // rtp timestamp
  // cValidateNever == mValidationMode (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  mAvbVideoStream->mValidationCount = 0u;
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateOnce;
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamInactive;
  // cValidateNever == mValidationMode (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  mAvbVideoStream->mValidationCount = 0u;
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateOnce;
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamInactive;
  avtpBase8[22] = 4u;
  // eComp1722aD5 == mCompatibility                               (T)
  // descPacket.mptField = (avtpBase8[22] & 0x04) ? 0xE0u : 0x60u (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamInactive;
  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u);
  // newState = Valid && oldState != newState
  // isConnected (F)
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum);
  // newState = Invalid && oldState != newState
  // isConnected (F)
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));

  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  // newState = Invalid && oldState != newState
  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum);
  // isConnected (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  // eComp1722aD5 == mCompatibility (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  avtpBase8[22] = 16u;
  // eComp1722aD5 == mCompatibility                               (F)
  // descPacket.mptField = (avtpBase8[22] & 0x10) ? 0xE0u : 0x60u (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  avtpBase16[10] = sizeof packet - IasAvbVideoStream::cAvtpHeaderSize - 1;
  // (length - cAvtpHeaderSize) >= payloadLength (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  avtpBase8[16] = 0x03u;
  // avtpBase8[16] == 0x02u (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  avtpBase8[0] = 0x02u;
  // 0x03 == avtpBase8[0] (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  mAvbVideoStream->mVideoFormat    = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  // IasAvbVideoFormat::eIasAvbVideoFormatIec61883 == mVideoFormat (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mCompatibility  = IasAvbVideoStream::Compatibility::eCompCurrent;
  mAvbVideoStream->mVideoFormat    = format;
  // length >= cAvtpHeaderSize (F)
  mAvbVideoStream->readFromAvbPacket(packet, IasAvbVideoStream::cAvtpHeaderSize - 1);

  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum + 1u); // valid stream
  mAvbVideoStream->mStreamState    = IasAvbStreamState::eIasAvbStreamValid;
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateOnce;
  // (cValidateAlways == mValidationMode) || (IasAvbStreamState::eIasAvbStreamValid != oldState) (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[0]  = 0u; // IEC61883 video format
  avtpBase8[22] = 0x40u;
  avtpBase8[26] = 0x4u; // sph
  avtpBase16[10] = htons(sizeof packet - 64); // payloadLength
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  // 0x00 == avtpBase8[0]                        (T)
  // 0x40 == (avtpBase8[22] & 0x40)              (T)
  // 0x4 == (avtpBase8[26] & 0x4)                (T)
  // (length - cAvtpHeaderSize) >= payloadLength (T)
  // descPacket.hasSph                           (T)
  // 0 != (payloadLength % 192)                  (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[0]  = 0u; // IEC61883 video format
  avtpBase8[2] = uint8_t(mAvbVideoStream->mSeqNum - 1u); // set to increase rtp sequence number high byte
  avtpBase8[22] = 0x40u;// tag
  avtpBase8[26] = 0x4u; // sph
  avtpBase16[10] = htons(sizeof packet - IasAvbVideoStream::cAvtpHeaderSize + 1); // payloadLength
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  // (length - cAvtpHeaderSize) >= payloadLength (F)
  // descPacket.hasSph                           (T)
  // 0 != (payloadLength % 192)                  (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[0]  = 0u; // IEC61883 video format
  avtpBase8[22] = 0x40u;// tag
  avtpBase8[26] = 0x30u; // sph
  avtpBase16[10] = htons(sizeof packet - 84); // payloadLength
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  // (length - cAvtpHeaderSize) >= payloadLength (T)
  // descPacket.hasSph                           (F)
  // 0 != (payloadLength % 188)                  (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[0]  = 0u; // IEC61883 video format
  avtpBase8[22] = 0x40u;// tag
  avtpBase8[26] = 0x30u; // sph
  avtpBase16[10] = htons(sizeof packet - IasAvbVideoStream::cAvtpHeaderSize); // payloadLength
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  // (length - cAvtpHeaderSize) >= payloadLength (T)
  // descPacket.hasSph                           (F)
  // 0 != (payloadLength % 188)                  (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[0]  = 0u; // IEC61883 video format
  avtpBase8[22] = 0x30u;// tag
  mAvbVideoStream->mValidationMode = IasAvbVideoStream::cValidateAlways;
  // 0x00 == avtpBase8[0]                        (T)
  // 0x40 == (avtpBase8[22] & 0x40)              (F)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  avtpBase8[0] = 0u; // not IEC61883 video format
  avtpBase8[2] = mAvbVideoStream->mSeqNum + 1u;
  mAvbVideoStream->mValidationMode      = IasAvbVideoStream::cValidateOnce;
  mAvbVideoStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  // IasAvbStreamState::eIasAvbStreamValid == oldState    (T)
  // avtpBase8[2] == uint8_t(mSeqNum + 1u)                  (T)
  // 0x00 == avtpBase8[0]                                 (T)
  mAvbVideoStream->readFromAvbPacket(packet, sizeof packet);

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  delete localStream;
  localStream = NULL;
}

TEST_F(IasTestAvbVideoStream, finalizeAvbPacket)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  uint16_t maxPacketRate = 42u;
  uint16_t maxPacketSize = 42u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId, rxStreamId (0x91E0F000FE000001u);
  IasAvbMacAddress dmac = {0};
  IasAvbPtpClockDomain clockDomain;
  uint16_t poolSize = 2u;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  bool preconfigured = true;

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
  mAvbVideoStream->activate();

  IasLocalVideoBuffer::IasVideoDesc descPacket;
  IasAvbPacket *packet = new (nothrow) IasAvbPacket();
  memset(&packet->vaddr, 0, sizeof packet->vaddr);
  uint32_t _vaddr[48];
  memset(&_vaddr, 0, sizeof _vaddr);
  packet->vaddr = &_vaddr;
  packet->len   = sizeof _vaddr;
  descPacket.avbPacket = packet;
  descPacket.isIec61883Packet = false;

  uint8_t* const avtpBase8 = static_cast<uint8_t*>(packet->vaddr) + 18;
  uint16_t* const avtpBase16 = static_cast<uint16_t*>(packet->vaddr) + 9;
  uint32_t* const avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase8);

  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  // 0u == mPacketLaunchTime                                (T)
  ASSERT_TRUE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  mAvbVideoStream->mPacketLaunchTime = 1u;
  mAvbVideoStream->mCompatibility = IasAvbVideoStream::Compatibility::eCompCurrent;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  // 0u == mPacketLaunchTime                                (F)
  ASSERT_TRUE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  uint32_t presentationTime     = mAvbVideoStream->mRefPaneSampleTime + mAvbVideoStream->getPresentationTimeOffset();
  descPacket.isIec61883Packet = true;
  // !descPacket->isIec61883Packet (F)
  ASSERT_TRUE(mAvbVideoStream->finalizeAvbPacket(&descPacket));
  ASSERT_EQ(presentationTime, ntohl(avtpBase32[8]));
  // validate CIP header
  (void)avtpBase16;
  ASSERT_EQ(63u, avtpBase8[24]);    // qi_1 and SID
  ASSERT_EQ(0x06u, avtpBase8[25]);  // DBS for AVTP
  ASSERT_EQ(0xC4u, avtpBase8[26]);  // FN_QPC_SPH_rsv, FN and QPC
  ASSERT_EQ(0x00u, avtpBase8[27]);  // DBC
  ASSERT_EQ(160u, avtpBase8[28]);   // qi_2_FMT
  ASSERT_EQ(0x00u, avtpBase8[31]);  // DBC

  descPacket.isIec61883Packet = false;
  descPacket.mptField         = 0x80u;
  // avtpBase8[22]= (descPacket->mptField & 0x80) ? 0x10u : 0x00u (T)
  ASSERT_TRUE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  mAvbVideoStream->mPacketLaunchTime = 1u;
  mAvbVideoStream->mCompatibility = IasAvbVideoStream::Compatibility::eComp1722aD5;
  // avtpBase8[22]= (descPacket->mptField & 0x80) ? 0x4u : 0x00u (T)
  ASSERT_TRUE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  mAvbVideoStream->deactivate();
  delete mAvbVideoStream->mTSpec;
  mAvbVideoStream->mTSpec = NULL;

  IasLocalVideoBuffer::IasVideoDesc * nullPacket = NULL;
  // NULL == descPacket || !isInitialized() || !isActive() || !isTransmitStream() (T||T||T||T)
  ASSERT_FALSE(mAvbVideoStream->finalizeAvbPacket(nullPacket));

  delete mAvbVideoStream;
  mAvbVideoStream = new IasAvbVideoStream();

  ASSERT_FALSE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  uint16_t vid = 0;

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(IasAvbSrClass::eIasAvbSrClassHigh,
                                                        maxPacketRate,
                                                        maxPacketSize,
                                                        format,
                                                        rxStreamId,
                                                        dmac,
                                                        vid,
                                                        preconfigured));

  ASSERT_FALSE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  mAvbVideoStream->activate();

  // NULL == descPacket || !isInitialized() || !isActive() || !isTransmitStream() (F||F||F||T)
  ASSERT_FALSE(mAvbVideoStream->finalizeAvbPacket(&descPacket));

  delete packet;
}

TEST_F(IasTestAvbVideoStream, prepareAllPackets)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  uint16_t maxPacketRate = 24u;
  uint16_t maxPacketSize = 24u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId;
  uint32_t poolSize = 2u;
  IasAvbPtpClockDomain clockDomain;
  IasAvbMacAddress dmac = {0};
  bool preconfigured = true;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
  IasAvbPacketPool * mPool = &mAvbVideoStream->getPacketPool();
  mPool->mLock.lock();
  mPool->mFreeBufferStack.clear();
  mPool->mLock.unlock();
  // NULL == referencePacket
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbVideoStream->prepareAllPackets());
}

TEST_F(IasTestAvbVideoStream, resetTime)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  bool hard = false;
  // NULL != ptp (F)
  mAvbVideoStream->resetTime(hard);
  ASSERT_TRUE(1);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  mAvbVideoStream->mPacketLaunchTime = IasAvbStreamHandlerEnvironment::getPtpProxy()->getLocalTime() + 1000000u;
  // launchDelta > 0L (F)
  mAvbVideoStream->resetTime(hard);
  ASSERT_TRUE(1);

  mAvbVideoStream->mPacketLaunchTime = IasAvbStreamHandlerEnvironment::getPtpProxy()->getLocalTime();
  // launchDelta > 0L (T)
  mAvbVideoStream->resetTime(hard);
  ASSERT_TRUE(1);

  hard = true;
  mAvbVideoStream->resetTime(hard);
  ASSERT_TRUE(0u != mAvbVideoStream->mPacketLaunchTime);
}

TEST_F(IasTestAvbVideoStream, signalDiscontinuity)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  IasAvbVideoStream::DiscontinuityEvent event = IasAvbVideoStream::eIasUnspecific;
  uint32_t numSamples = 0;

  ASSERT_FALSE(mAvbVideoStream->signalDiscontinuity(event, numSamples));
}

TEST_F(IasTestAvbVideoStream, writeToAvbPacket)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  IasAvbPacket *packet = NULL;

  ASSERT_FALSE(mAvbVideoStream->writeToAvbPacket(packet, 0u));
}

TEST_F(IasTestAvbVideoStream, preparePacket)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  // isInitialized() && isTransmitStream() && isConnected() (F && F && F)
  ASSERT_TRUE(NULL == mAvbVideoStream->preparePacket(0u));

  uint16_t maxPacketRate = 1000;
  uint16_t maxPacketSize = 512;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId (0x91E0F000FE000001u);
  uint32_t poolSize = 2048;
  IasAvbHwCaptureClockDomain clockDomain;
  IasAvbMacAddress dmac = {0x91, 0xE0, 0xF0, 0x00, 0xFE, 0x01};
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t vid = 0;
  bool preconfigured = true;
  IasAvbStreamId  localStreamId;
  uint16_t numPackets = 4u;
//  bool useInternalBuffers = true;
  IasLocalVideoBuffer::IasVideoDesc  descPacket;
  IasAvbPacket avbPacket;
  descPacket.avbPacket = &avbPacket;
  uint8_t bufferData[2];
  memset(bufferData, 0, sizeof bufferData);
  descPacket.buffer.data = bufferData;
  descPacket.buffer.size = sizeof bufferData;
  descPacket.rtpSequenceNumber = mAvbVideoStream->mRtpSequNrLast + 1u;

  bool noInternalBuffers = false;
  LocalVideoStream * localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbTransmitToNetwork, mDltCtx);
  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, noInternalBuffers);
  localStream->setClientActive(true);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(srClass,
                                                        maxPacketRate,
                                                        maxPacketSize,
                                                        format,
                                                        streamId,
                                                        dmac,
                                                        vid,
                                                        preconfigured));
  // isInitialized() && isTransmitStream() && isConnected() (T && F && F)
  ASSERT_TRUE(NULL == mAvbVideoStream->preparePacket(0u));

  delete mAvbVideoStream;
  mAvbVideoStream = new (nothrow) IasAvbVideoStream();

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
  // isInitialized() && isTransmitStream() && isConnected() (T && T && F)
  ASSERT_TRUE(NULL == mAvbVideoStream->preparePacket(0u));

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));
  // NULL != descPacket.avbPacket (F)
  // NULL != packet               (F)
  ASSERT_FALSE(NULL != mAvbVideoStream->preparePacket(0u));

  ASSERT_EQ(2, localStream->getLocalVideoBuffer()->writeH264(&descPacket));
  // prepare fake packet
  ASSERT_FALSE(NULL != mAvbVideoStream->preparePacket(0u));

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  ASSERT_EQ(eIasAvbProcOK, localStream->disconnect());
  delete localStream;
  localStream = NULL;
}

TEST_F(IasTestAvbVideoStream, preparePacket_msgCount)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);

  uint16_t maxPacketRate = 1000;
  uint16_t maxPacketSize = 512;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId (0x91E0F000FE000001u);
  uint32_t poolSize = 2048;
  IasAvbHwCaptureClockDomain clockDomain;
  IasAvbMacAddress dmac = {0x91, 0xE0, 0xF0, 0x00, 0xFE, 0x01};
  bool preconfigured    = true;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  IasAvbStreamId  localStreamId;
  uint16_t numPackets = 4u;
  bool useInternalBuffers = true;
  IasLocalVideoBuffer::IasVideoDesc  descPacket;
  IasAvbPacket avbPacket;
  descPacket.avbPacket = &avbPacket;
  uint8_t bufferData[104];
  memset(bufferData, 0, sizeof bufferData);
  descPacket.buffer.data = bufferData;
  descPacket.buffer.size = sizeof bufferData;
  descPacket.rtpSequenceNumber = mAvbVideoStream->mRtpSequNrLast + 1u;

  LocalVideoStream * localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbTransmitToNetwork, mDltCtx);
  localStream->init(format, numPackets, maxPacketRate, maxPacketSize, useInternalBuffers);
  localStream->setClientActive(true);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));

  mAvbVideoStream->mMsgCount = mAvbVideoStream->mMsgCountMax + 1u;
  descPacket.rtpSequenceNumber = mAvbVideoStream->mRtpSequNrLast + 2u;
  ASSERT_EQ(0, localStream->getLocalVideoBuffer()->writeH264(&descPacket));
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));
  // (descPacket.rtpSequenceNumber != static_cast<uint16_t>(mRtpSequNrLast + 1)) (T)
  // && (mRtpSequNrLast != 0)                                                  (F)
  // mMsgCount > mMsgCountMax (T)
  ASSERT_FALSE(NULL != mAvbVideoStream->preparePacket(0u));

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  delete localStream;
  localStream = NULL;
}

TEST_F(IasTestAvbVideoStream, preparePacket_seq_err)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);

  uint16_t maxPacketRate = 1000;
  uint16_t maxPacketSize = 512;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId (0x91E0F000FE000001u);
  uint32_t poolSize = 2048;
  IasAvbHwCaptureClockDomain clockDomain;
  IasAvbMacAddress dmac = {0x91, 0xE0, 0xF0, 0x00, 0xFE, 0x01};
  bool preconfigured = true;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  IasAvbStreamId  localStreamId;
  uint16_t numPackets = 4u;
  bool useInternalBuffers = true;
  IasLocalVideoBuffer::IasVideoDesc  descPacket;
  IasAvbPacket avbPacket;
  descPacket.avbPacket = &avbPacket;
  uint8_t bufferData[2];
  memset(bufferData, 0, sizeof bufferData);
  descPacket.buffer.data = bufferData;
  descPacket.buffer.size = sizeof bufferData;
  descPacket.rtpSequenceNumber = mAvbVideoStream->mRtpSequNrLast + 1u;

  LocalVideoStream * localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbTransmitToNetwork, mDltCtx);
  ASSERT_EQ(eIasAvbProcOK, localStream->init(format, numPackets, maxPacketRate, maxPacketSize, useInternalBuffers));
  localStream->setClientActive(true);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));

  struct timespec tp;
  (void) clock_gettime(CLOCK_MONOTONIC, &tp);
  uint64_t localTime = (uint64_t(tp.tv_sec + 1) * uint64_t(1000000000u)) + tp.tv_nsec;
  mAvbVideoStream->mLocalTimeLast = localTime - IasAvbVideoStream::cObservationInterval;

  descPacket.rtpSequenceNumber = mAvbVideoStream->mRtpSequNrLast + 1u;
  mAvbVideoStream->mRtpSequNrLast = 1u;
  ASSERT_EQ(0, localStream->getLocalVideoBuffer()->writeH264(&descPacket));
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));
  // (descPacket.rtpSequenceNumber != static_cast<uint16_t>(mRtpSequNrLast + 1)) (T)
  // && (mRtpSequNrLast != 0)                                                  (T)
  ASSERT_FALSE(NULL != mAvbVideoStream->preparePacket(0u));

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  delete localStream;
  localStream = NULL;
}

TEST_F(IasTestAvbVideoStream, prepareDummyAvbPacket)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  uint16_t maxPacketRate = 42u;
  uint16_t maxPacketSize = 42u;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId, rxStreamId (0x91E0F000FE000001u);
  IasAvbMacAddress dmac = {0};
  IasAvbPtpClockDomain clockDomain;
  uint16_t poolSize = 2u;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassHigh;
  bool preconfigured = true;

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
  IasAvbPacket *packet = new IasAvbPacket();
  mAvbVideoStream->activate();

  memset(&packet->vaddr, 0, sizeof packet->vaddr);
  uint8_t _vaddr[40];
  memset(&_vaddr, 0, sizeof _vaddr);
  packet->vaddr = &_vaddr;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  // 0u == mPacketLaunchTime                                (T)
  ASSERT_TRUE(mAvbVideoStream->prepareDummyAvbPacket(packet));

  mAvbVideoStream->mPacketLaunchTime = 1u;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  // 0u == mPacketLaunchTime                                (F)
  ASSERT_TRUE(mAvbVideoStream->prepareDummyAvbPacket(packet));

  delete mAvbVideoStream;
  mAvbVideoStream = new (nothrow) IasAvbVideoStream();

  IasAvbMacAddress rxDmac = {0x91, 0xE0, 0xF0, 0x00, 0xFE, 0x01};
  uint16_t vid = 0;

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initReceive(srClass,
                                                        maxPacketRate,
                                                        maxPacketSize,
                                                        format,
                                                        rxStreamId,
                                                        rxDmac,
                                                        vid,
                                                        preconfigured));
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || T)
  ASSERT_FALSE(mAvbVideoStream->prepareDummyAvbPacket(packet));

  delete packet;
  packet = NULL;
  mAvbVideoStream->deactivate();
  // !isInitialized() || !isActive() || !isTransmitStream() (F || T || T)
  ASSERT_FALSE(mAvbVideoStream->prepareDummyAvbPacket(packet));

  delete mAvbVideoStream->mTSpec;
  mAvbVideoStream->mTSpec = NULL;
  // !isInitialized() || !isActive() || !isTransmitStream() (T || T || T)
  ASSERT_FALSE(mAvbVideoStream->prepareDummyAvbPacket(packet));
}

TEST_F(IasTestAvbVideoStream, getLocalStreamId)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);

  uint16_t maxPacketRate = 1000;
  uint16_t maxPacketSize = 512;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  IasAvbStreamId  streamId (0x91E0F000FE000001u);
  uint32_t poolSize = 2048;
  IasAvbHwCaptureClockDomain clockDomain;
  IasAvbMacAddress dmac = {0x91, 0xE0, 0xF0, 0x00, 0xFE, 0x01};
  bool preconfigured = true;
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  uint16_t  localStreamId = 512u;
  uint16_t numPackets = 4u;
  bool useInternalBuffers = true;

  LocalVideoStream * localStream = new LocalVideoStream(IasAvbStreamDirection::eIasAvbTransmitToNetwork, mDltCtx, localStreamId);
  ASSERT_EQ(eIasAvbProcOK, localStream->init(format, numPackets, maxPacketRate, maxPacketSize, useInternalBuffers));
  localStream->setClientActive(true);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->initTransmit(srClass,
                                                         maxPacketRate,
                                                         maxPacketSize,
                                                         format,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac,
                                                         preconfigured));
  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(localStream));

  ASSERT_EQ(localStreamId, mAvbVideoStream->getLocalStreamId());

  ASSERT_EQ(eIasAvbProcOK, mAvbVideoStream->connectTo(NULL));
  localStream->cleanup();
  delete localStream;
}

TEST_F(IasTestAvbVideoStream, compareAttributes)
{
  ASSERT_TRUE(mAvbVideoStream != NULL);

  uint64_t streamId = 0u;
  IasAvbStreamDirection direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  IasAvbVideoFormat format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  uint32_t clockId = 0u;
  uint64_t dmac = {};
  uint64_t avbMacAddr = {};
  IasAvbStreamState txStatus = IasAvbStreamState::eIasAvbStreamInactive;
  IasAvbStreamState rxStatus = IasAvbStreamState::eIasAvbStreamInactive;
  bool preconfigured = false;
  IasAvbStreamDiagnostics diagnostics = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  IasAvbVideoStreamAttributes first_att;
  first_att.setStreamId(streamId);
  first_att.setDirection(direction);
  first_att.setFormat(format);
  first_att.setClockId(clockId);
  first_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
  first_att.setStreamId(streamId);
  first_att.setDmac(dmac);
  first_att.setSourceMac(avbMacAddr);
  first_att.setTxActive(txStatus);
  first_att.setRxStatus(rxStatus);
  first_att.setLocalStreamId(streamId);
  first_att.setPreconfigured(preconfigured);
  first_att.setDiagnostics(diagnostics);

  first_att = first_att;
  ASSERT_EQ(first_att, first_att);

  IasAvbVideoStreamAttributes second_att;
  second_att.setStreamId(streamId);
  second_att.setDirection(direction);
  second_att.setFormat(format);
  second_att.setClockId(clockId);
  second_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
  second_att.setStreamId(streamId);
  second_att.setDmac(dmac);
  second_att.setSourceMac(avbMacAddr);
  second_att.setTxActive(txStatus);
  second_att.setRxStatus(rxStatus);
  second_att.setLocalStreamId(streamId);
  second_att.setPreconfigured(preconfigured);
  second_att.setDiagnostics(diagnostics);

  bool result = false;
  if(first_att == second_att)
  {
      result = true;
  }
  ASSERT_TRUE(result);


  streamId = 1u;
  second_att.setStreamId(streamId);
  ASSERT_EQ(streamId, second_att.getStreamId());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  streamId = 0u;
  second_att.setStreamId(streamId);


  direction = IasAvbStreamDirection::eIasAvbReceiveFromNetwork;
  second_att.setDirection(direction);
  ASSERT_EQ(direction, second_att.getDirection());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  direction = IasAvbStreamDirection::eIasAvbTransmitToNetwork;
  second_att.setDirection(direction);

  format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  second_att.setFormat(format);
  ASSERT_EQ(format, second_att.getFormat());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  format = IasAvbVideoFormat::eIasAvbVideoFormatRtp;
  second_att.setFormat(format);


  clockId = 1u;
  second_att.setClockId(clockId);
  ASSERT_EQ(clockId, second_att.getClockId());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  clockId = 0u;
  second_att.setClockId(clockId);

  second_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll);
  ASSERT_EQ(IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll, second_att.getAssignMode());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  second_att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);


  dmac = {1};
  second_att.setDmac(dmac);
  ASSERT_EQ(dmac, second_att.getDmac());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  dmac = {};
  second_att.setDmac(dmac);


  avbMacAddr = {1};
  second_att.setSourceMac(avbMacAddr);
  ASSERT_EQ(avbMacAddr, second_att.getSourceMac());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  avbMacAddr = {};
  second_att.setSourceMac(avbMacAddr);


  txStatus = IasAvbStreamState::eIasAvbStreamValid;
  second_att.setTxActive(txStatus);
  ASSERT_EQ(true, second_att.getTxActive());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  txStatus = IasAvbStreamState::eIasAvbStreamInactive;
  second_att.setTxActive(txStatus);


  rxStatus = IasAvbStreamState::eIasAvbStreamValid;
  second_att.setRxStatus(rxStatus);
  ASSERT_EQ(rxStatus, second_att.getRxStatus());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  txStatus = IasAvbStreamState::eIasAvbStreamInactive;
  second_att.setRxStatus(rxStatus);


  streamId = 1u;
  second_att.setLocalStreamId(streamId);
  ASSERT_EQ(streamId, second_att.getLocalStreamId());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  streamId = 0u;
  second_att.setLocalStreamId(streamId);


  preconfigured = true;
  second_att.setPreconfigured(preconfigured);
  ASSERT_EQ(preconfigured, second_att.getPreconfigured());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  preconfigured = false;
  second_att.setPreconfigured(preconfigured);


  diagnostics = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  second_att.setDiagnostics(diagnostics);
  ASSERT_EQ(diagnostics, second_att.getDiagnostics());
  result = true;
  if(first_att == second_att)
  {
      result = false;
  }
  ASSERT_TRUE(result);
  diagnostics = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  second_att.setDiagnostics(diagnostics);

  result = false;
  if(first_att != second_att)
  {
      result = true;
  }
  ASSERT_TRUE(result);

  IasAvbVideoStreamAttributes third_att;
  third_att = first_att;
  ASSERT_EQ(first_att, third_att);

  uint16_t maxPacketRate = 0u;
  uint16_t maxPacketSize = 0u;
  IasAvbVideoStreamAttributes fourth_att(direction, maxPacketRate, maxPacketSize, format, clockId,
                                         IasAvbIdAssignMode::eIasAvbIdAssignModeDynamicAll, streamId, dmac, avbMacAddr,
                                         txStatus, rxStatus, streamId, preconfigured, diagnostics);
  ASSERT_EQ(maxPacketRate, fourth_att.getMaxPacketRate());
  ASSERT_EQ(maxPacketSize, fourth_att.getMaxPacketSize());

}
