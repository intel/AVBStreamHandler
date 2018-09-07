/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestAvbClockReferenceStream.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasAvbStreamId.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "test_common/IasSpringVilleInfo.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#undef protected
#undef private

// For heap fail size calculations
extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

#include <arpa/inet.h>
#include <sstream>

namespace IasMediaTransportAvb {

class IasTestAvbClockReferenceStream : public ::testing::Test
{
public:
  IasTestAvbClockReferenceStream():
    mClockRefStream(NULL),
#if VERBOSE_TEST_PRINTOUT
    streamHandler(DLT_LOG_VERBOSE),
#else
    streamHandler(DLT_LOG_INFO),
#endif
    mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbClockReferenceStream()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;
    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
                  "TEST",
                  "IasTestAvbClockReferenceStream",
                  DLT_LOG_DEBUG,
                  DLT_TRACE_STATUS_OFF);
    dlt_enable_local_print();

    mClockRefStream = new (std::nothrow) IasAvbClockReferenceStream();
  }

  virtual void TearDown()
  {
    if (NULL != mClockRefStream)
    {
      delete mClockRefStream;
      mClockRefStream = NULL;
    }

    if (mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    streamHandler.cleanup();

    heapSpaceLeft = heapSpaceInitSize;
    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }


protected:

  IasAvbProcessingResult initStreamHandler()
  {
    // IT HAS TO BE SET TO ZERO - getopt_long - DefaultConfig_passArguments - needs to be reinitialized before use
    optind = 0;

    IasSpringVilleInfo::fetchData();

    const char *args[] = {
      "setup",
      "-t", "Fedora",
      "-p", "UnitTests",
      "-n", IasSpringVilleInfo::getInterfaceName()
    };

    int argc = sizeof(args) / sizeof(args[0]);

    if (NULL != mEnvironment)
    {
      // init of streamhandler will create it's own environment
      return eIasAvbProcErr;
    }

    return streamHandler.init(theConfigPlugin, true, argc, (char**)args);
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
    AVB_ASSERT(IasAvbStreamHandlerEnvironment::mInstance);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
    result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
    return result;
  }

  IasAvbResult setConfigValue(const std::string& key, const std::string& value)
  {
    IasAvbResult result;
    AVB_ASSERT(IasAvbStreamHandlerEnvironment::mInstance);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = false;
    result =  IasAvbStreamHandlerEnvironment::mInstance->setConfigValue(key, value);
    IasAvbStreamHandlerEnvironment::mInstance->mRegistryLocked = true;
    return result;
  }

  DltContext mDltCtx;
  IasAvbClockReferenceStream *mClockRefStream;
  IasAvbStreamHandler streamHandler;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

TEST_F(IasTestAvbClockReferenceStream, Ctor_Dtor)
{
  ASSERT_TRUE(mClockRefStream != NULL);
}

TEST_F(IasTestAvbClockReferenceStream, initTransmit)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  ASSERT_TRUE(createEnvironment());
  IasAvbSrClass srClass = IasAvbSrClass::eIasAvbSrClassLow;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  // mCrfHeaderSize + (cCrfTimeStampSize * crfStampsPerPdu) = 1500
  uint16_t crfStampsPerPdu          = 1u;// 20 + 8 * 185
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 1u;
  IasAvbClockMultiplier pull      = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  const IasAvbStreamId streamId(uint64_t(0u));
  uint32_t poolSize                 = 2u;
  IasAvbClockDomain * nullDomain  = NULL;
  IasAvbMacAddress dmac           = {0};
  // (0u == crfStampsPerPdu) || (0u == crfStampInterval) (T || T)
  // || (0u == baseFreq) || (0x1FFFFFFFu < baseFreq)     (T || F)
  // || (NULL == clockDomain)                            (T)
  ASSERT_EQ(eIasAvbProcInvalidParam, mClockRefStream->initTransmit(srClass,
                                                                   type,
                                                                   crfStampsPerPdu,
                                                                   crfStampInterval,
                                                                   baseFreq,
                                                                   pull,
                                                                   streamId,
                                                                   poolSize,
                                                                   nullDomain,
                                                                   dmac));
}

TEST_F(IasTestAvbClockReferenceStream, decodeNominalFreq)
{
  ASSERT_TRUE(mClockRefStream != NULL);

  ASSERT_EQ(8000, mClockRefStream->decodeNominalFreq(1));
  ASSERT_EQ(16000, mClockRefStream->decodeNominalFreq(2));
  ASSERT_EQ(32000, mClockRefStream->decodeNominalFreq(3));
  ASSERT_EQ(44100, mClockRefStream->decodeNominalFreq(4));
  ASSERT_EQ(88200, mClockRefStream->decodeNominalFreq(5));
  ASSERT_EQ(176400, mClockRefStream->decodeNominalFreq(6));
  ASSERT_EQ(48000, mClockRefStream->decodeNominalFreq(7));
  ASSERT_EQ(96000, mClockRefStream->decodeNominalFreq(8));
  ASSERT_EQ(192000, mClockRefStream->decodeNominalFreq(9));

  // few random values
  uint32_t nominalFreq = 0;
  ASSERT_EQ(nominalFreq, mClockRefStream->decodeNominalFreq(0));
  ASSERT_EQ(nominalFreq, mClockRefStream->decodeNominalFreq(10));
  ASSERT_EQ(nominalFreq, mClockRefStream->decodeNominalFreq(15));
}

TEST_F(IasTestAvbClockReferenceStream, initFormat)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  ASSERT_TRUE(createEnvironment());

  mEnvironment->setConfigValue(IasRegKeys::cCompatibilityAudio, "d6_1722a");
  mClockRefStream->initFormat();
  ASSERT_EQ(24, mClockRefStream->mCrfHeaderSize);
  ASSERT_EQ(4, mClockRefStream->mPayloadHeaderSize);
  ASSERT_EQ(5, mClockRefStream->mSubtype);
  ASSERT_EQ(7, mClockRefStream->mPayloadOffset32);
  ASSERT_EQ(10, mClockRefStream->mPayloadLenOffset16);
  ASSERT_EQ(11*2, mClockRefStream->mTypeoffset8);

  mEnvironment->setConfigValue(IasRegKeys::cCompatibilityAudio, "latest");
  mClockRefStream->initFormat();
  ASSERT_EQ(20, mClockRefStream->mCrfHeaderSize);
  ASSERT_EQ(0, mClockRefStream->mPayloadHeaderSize);
  ASSERT_EQ(4, mClockRefStream->mSubtype);
  ASSERT_EQ(5, mClockRefStream->mPayloadOffset32);
  ASSERT_EQ(8, mClockRefStream->mPayloadLenOffset16);
  ASSERT_EQ(3, mClockRefStream->mTypeoffset8);
}

TEST_F(IasTestAvbClockReferenceStream, getCompatibilityMode)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  ASSERT_TRUE(createEnvironment());

  mEnvironment->setConfigValue(IasRegKeys::cCompatibilityAudio, "d6_1722a");
  ASSERT_EQ(eIasAvbCompD6, mClockRefStream->getCompatibilityMode());

  mEnvironment->setConfigValue(IasRegKeys::cCompatibilityAudio, "latest");
  ASSERT_EQ(eIasAvbCompLatest, mClockRefStream->getCompatibilityMode());
}

TEST_F(IasTestAvbClockReferenceStream, activationChanged)
{
  ASSERT_TRUE(mClockRefStream != NULL);

  mClockRefStream->activationChanged();
  ASSERT_EQ(0u, mClockRefStream->mRefPlaneEventTime);

  mClockRefStream->mRefPlaneEventCount = 0x00FF;
  mClockRefStream->mActive = true;
  mClockRefStream->activationChanged();
  ASSERT_EQ(0u, mClockRefStream->mRefPlaneEventCount);
}

TEST_F(IasTestAvbClockReferenceStream, resetTime)
{
  ASSERT_TRUE(mClockRefStream != NULL);

  ASSERT_TRUE(createEnvironment());
  uint64_t next = 1u;
  IasAvbRxStreamClockDomain clockDomain;
  mClockRefStream->mAvbClockDomain = &clockDomain;

  // eventRate = 0
  mClockRefStream->mPacketLaunchTime = 0u;
  ASSERT_FALSE(mClockRefStream->resetTime(next));
  ASSERT_TRUE(next == mClockRefStream->mPacketLaunchTime);

  // eventRate != 0
  // masterTime = 0
  clockDomain.reset(IasAvbSrClass::eIasAvbSrClassHigh, uint64_t(0u), 48000u);
  mClockRefStream->mPacketLaunchTime = 0u;
  ASSERT_FALSE(mClockRefStream->resetTime(next));
  ASSERT_TRUE(next == mClockRefStream->mPacketLaunchTime);

  // eventRate != 0
  // masterTime != 0
  // masterTime < next
  clockDomain.update(6u, 125000u, 125000u, 125000u);
  next = 7u;
  mClockRefStream->mPacketLaunchTime = 0u;
  ASSERT_TRUE(mClockRefStream->resetTime(next));
  ASSERT_TRUE(0u != mClockRefStream->mPacketLaunchTime);

  // eventRate != 0
  // masterTime != 0
  // masterTime >= next
  clockDomain.update(6u, 125000u, 125000u, 125000u);
  mClockRefStream->mPacketLaunchTime = 0u;
  ASSERT_TRUE(mClockRefStream->resetTime(next));
  ASSERT_TRUE(0u != mClockRefStream->mPacketLaunchTime);

  uint64_t masterTime;
  clockDomain.getEventCount(masterTime);
  ASSERT_TRUE(mClockRefStream->resetTime(masterTime + 1u));
}

TEST_F(IasTestAvbClockReferenceStream, readFromAvbPacket)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  uint8_t * nullPacket = NULL;

  IasAvbSrClass srClass           = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu       = 1u;
  IasAvbStreamId rxStreamId(uint64_t(1u));
  IasAvbMacAddress dmac           = {0};
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initReceive(srClass, type, maxCrfStampsPerPdu, rxStreamId, dmac));
  // NULL == packet (T)
  mClockRefStream->readFromAvbPacket(nullPacket, 0);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamNoData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);
  uint8_t * avtpBase8 = packet;
  uint16_t * avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);
  uint32_t * avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase16);
  uint64_t * stampBase = reinterpret_cast<uint64_t*>(avtpBase8 + 20);
  avtpBase16[8] = htons(23u); // 0u == (payloadLength % cCrfTimeStampSize
  mClockRefStream->mValidationCount = 1u;
  // NULL == packet (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(0u, avtpBase8[2]);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mValidationCount = 0u;
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateNever;
  avtpBase16[8] = htons(24u); // payloadLength
  // cValidateNever == mValidationMode
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(0u, avtpBase8[2]);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateAlways;
  avtpBase8[0] = 4u;   // subtype
  avtpBase8[1] = 128u; // media clock restart
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[3] = uint8_t(type);
  avtpBase8[12] = 0u; // pull mode "flat"
  avtpBase16[8] = htons(24u); // payloadLength
  avtpBase16[9] = htons(1u); // eventsPerStamp
  avtpBase32[3] = htonl(24000u); // baseFreq
  stampBase[0]  = uint64_t(1e9);
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  IasAvbRxStreamClockDomain rxClockDomain;
  mClockRefStream->mAvbClockDomain = &rxClockDomain;
  // (NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx) (T && T)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(1u, avtpBase8[2]);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateAlways;
  avtpBase8[0] = 4u;   // subtype
  avtpBase8[1] = 192u; // media clock restart
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[3] = uint8_t(type);
  avtpBase8[12] = 0u; // pull mode "flat"
  avtpBase16[8] = htons(24u); // payloadLength
  avtpBase16[9] = htons(1u); // eventsPerStamp
  avtpBase32[3] = htonl(24000u); // baseFreq
  stampBase[0]  = uint64_t(1e9);
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  mClockRefStream->mAvbClockDomain = &rxClockDomain;
  // deepest path possible
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(2u, avtpBase8[2]);

  avtpBase8[1] = 192u; // media clock restart
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  mClockRefStream->mClockValid = true;
  mClockRefStream->mRefPlaneEventTime = 1u;
  mClockRefStream->mMediaClockRestartToggle = avtpBase8[1] & 0x40u;
  // (mrField != mMediaClockRestartToggle) (F)
  // || (rxClockDomain->getResetRequest()) (F)
  // || (0u == mRefPlaneEventTime)         (T)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(3u, avtpBase8[2]);

  avtpBase8[1] = 192u; // media clock restart
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  mClockRefStream->mClockValid = true;
  mClockRefStream->mRefPlaneEventTime = 1u;
  mClockRefStream->mMediaClockRestartToggle = avtpBase8[1] & 0x40u;
  IasAvbPtpClockDomain clockDomain;
  mClockRefStream->mAvbClockDomain = &clockDomain;
  // (NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx) (T && F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(4u, avtpBase8[2]);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateNever;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mMediaClockRestartToggle = 0u;
  // cValidateNever == mValidationMode (T)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(5u, avtpBase8[2]);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateNever;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mMediaClockRestartToggle = 0u;
  mClockRefStream->mValidationCount         = 1u;
  // 0u == mValidationCount (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(6u, avtpBase8[2]);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateAlways;
  avtpBase8[1] = 128u; // media clock restart
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[12] = 0xE0u; // pull mode "flat"
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mMediaClockRestartToggle = 0u;
  // ((avtpBase8[12] & 0xE0u) >> 5) == uint8_t(IasAvbClockMultiplier::eIasAvbCrsMultFlat) (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(7u, avtpBase8[2]);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateAlways;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[3] = uint8_t(IasAvbClockReferenceStreamType::eIasAvbCrsTypeUser);
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mMediaClockRestartToggle = 0u;
  // uint8_t(mType) == avtpBase8[3] (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(8u, avtpBase8[2]);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateOnce;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mStreamState = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mMediaClockRestartToggle = 0u;
  size_t length = mClockRefStream->mCrfHeaderSize - 1u;
  // length >= mCrfHeaderSize (F)
  mClockRefStream->readFromAvbPacket(packet, length);
  ASSERT_EQ(9u, avtpBase8[2]);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateOnce;
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mStreamState = IasAvbStreamState::eIasAvbStreamInactive;
  length = mClockRefStream->mCrfHeaderSize;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[3] = uint8_t(type);
  avtpBase8[12] = 0u; // pull mode "flat"
  avtpBase16[8] = htons(1u); // payloadLength
  mClockRefStream->mMediaClockRestartToggle = 0u;
  // ((length - mCrfHeaderSize) >= payloadLength)   (T)
  // && (payloadLength > 0u)                        (T)
  // && (0u == (payloadLength % cCrfTimeStampSize)) (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(10u, avtpBase8[2]);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateOnce;
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mStreamState = IasAvbStreamState::eIasAvbStreamInactive;
  length = mClockRefStream->mCrfHeaderSize;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[3] = uint8_t(type);
  avtpBase8[12] = 0u; // pull mode "flat"
  avtpBase16[8] = htons(0u); // payloadLength
  mClockRefStream->mMediaClockRestartToggle = 0u;
  // ((length - mCrfHeaderSize) >= payloadLength)   (T)
  // && (payloadLength > 0u)                        (F)
  // && (0u == (payloadLength % cCrfTimeStampSize)) (T)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(11u, avtpBase8[2]);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);

  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateOnce;
  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamInactive;
  mClockRefStream->mStreamState = IasAvbStreamState::eIasAvbStreamInactive;
  length = mClockRefStream->mCrfHeaderSize;
  avtpBase8[2] = uint8_t(mClockRefStream->mSeqNum + 1u); // valid stream
  avtpBase8[3] = uint8_t(type);
  avtpBase8[12] = 0u; // pull mode "flat"
  avtpBase16[8] = htons((sizeof packet) - mClockRefStream->mCrfHeaderSize + 1u); // payloadLength
  mClockRefStream->mMediaClockRestartToggle = 0u;
  // ((length - mCrfHeaderSize) >= payloadLength)   (F)
  // && (payloadLength > 0u)                        (T)
  // && (0u == (payloadLength % cCrfTimeStampSize)) (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
  ASSERT_EQ(IasAvbStreamState::eIasAvbStreamInvalidData, mClockRefStream->mStreamStateInternal);
}

TEST_F(IasTestAvbClockReferenceStream, readFromAvbPacket_tx)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  uint8_t * nullPacket = NULL;
  // isInitialized() && isReceiveStream() (F && F)
  mClockRefStream->readFromAvbPacket(nullPacket, 0);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());
  IasAvbSrClass srClass           = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t crfStampsPerPdu          = 1u;
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 24000u;
  IasAvbClockMultiplier pull      = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  IasAvbStreamId streamId(uint64_t(0u));
  uint32_t poolSize                 = 2u;
  IasAvbPtpClockDomain clockDomain;
  IasAvbMacAddress dmac           = {0};
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initTransmit(srClass,
                                                         type,
                                                         crfStampsPerPdu,
                                                         crfStampInterval,
                                                         baseFreq,
                                                         pull,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac));
  // isInitialized() && isReceiveStream() (T && F)
  mClockRefStream->readFromAvbPacket(nullPacket, 0);
}

TEST_F(IasTestAvbClockReferenceStream, readFromAvbPacket_validate)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbSrClass srClass           = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu       = 1u;
  IasAvbStreamId rxStreamId(uint64_t(1u));
  IasAvbMacAddress dmac           = {0};
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initReceive(srClass, type, maxCrfStampsPerPdu, rxStreamId, dmac));

  mClockRefStream->mStreamStateInternal = IasAvbStreamState::eIasAvbStreamValid;
  mClockRefStream->mValidationMode = IasAvbClockReferenceStream::cValidateOnce;
  uint8_t packet[1024];
  memset(packet, 0, sizeof packet);
  // NULL == packet (F)
  mClockRefStream->readFromAvbPacket(packet, sizeof packet);
}

TEST_F(IasTestAvbClockReferenceStream, writeToAvbPacket)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  IasAvbPacket * nullPacket = NULL;
  // !isInitialized() || !isActive() || !isTransmitStream() (T || T || ?)
  ASSERT_FALSE(mClockRefStream->writeToAvbPacket(nullPacket, 0u));

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbSrClass srClass           = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu       = 1u;
  IasAvbStreamId rxStreamId(uint64_t(1u));
  IasAvbMacAddress dmac           = {0};
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initReceive(srClass, type, maxCrfStampsPerPdu, rxStreamId, dmac));
  mClockRefStream->mActive = true;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || T)
  ASSERT_FALSE(mClockRefStream->writeToAvbPacket(nullPacket, 0u));

  delete mClockRefStream;
  mClockRefStream = new (std::nothrow) IasAvbClockReferenceStream();

  uint16_t crfStampsPerPdu          = 1u;
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 24000u;
  IasAvbClockMultiplier pull      = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  IasAvbStreamId streamId(uint64_t(0u));
  uint32_t poolSize                 = 2u;
  IasAvbPtpClockDomain clockDomain;
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initTransmit(srClass,
                                                         type,
                                                         crfStampsPerPdu,
                                                         crfStampInterval,
                                                         baseFreq,
                                                         pull,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac));

  IasAvbPacketPool pool(*mClockRefStream->mLog);
  pool.init(1024, 1);
  IasAvbPacket * packet = pool.getPacket();
  memset(packet->getBasePtr(), 0, 1024);
  mClockRefStream->mActive = true;
  mClockRefStream->mRefPlaneEventTime = 0u;
  mClockRefStream->mRefPlaneEventCount = 0u;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  ASSERT_TRUE(mClockRefStream->writeToAvbPacket(packet, 0u));

  mClockRefStream->mClockValid         = false;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  ASSERT_TRUE(mClockRefStream->writeToAvbPacket(packet, 0u));
}

TEST_F(IasTestAvbClockReferenceStream, writeToAvbPacket_raw)
{
  ASSERT_TRUE(mClockRefStream != NULL);
  IasAvbPacket * nullPacket = NULL;
  // !isInitialized() || !isActive() || !isTransmitStream() (T || T || ?)
  ASSERT_FALSE(mClockRefStream->writeToAvbPacket(nullPacket, 0u));

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbSrClass srClass           = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbClockReferenceStreamType type = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  uint16_t maxCrfStampsPerPdu       = 1u;
  IasAvbStreamId rxStreamId(uint64_t(1u));
  IasAvbMacAddress dmac           = {0};
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initReceive(srClass, type, maxCrfStampsPerPdu, rxStreamId, dmac));
  mClockRefStream->mActive = true;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || T)
  ASSERT_FALSE(mClockRefStream->writeToAvbPacket(nullPacket, 0u));

  delete mClockRefStream;
  mClockRefStream = new (std::nothrow) IasAvbClockReferenceStream();

  uint16_t crfStampsPerPdu          = 1u;
  uint16_t crfStampInterval         = 1u;
  uint32_t baseFreq                 = 24000u;
  IasAvbClockMultiplier pull      = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  IasAvbStreamId streamId(uint64_t(0u));
  uint32_t poolSize                 = 2u;
  IasAvbRxStreamClockDomain clockDomain;
  ASSERT_EQ(eIasAvbProcOK, mClockRefStream->initTransmit(srClass,
                                                         type,
                                                         crfStampsPerPdu,
                                                         crfStampInterval,
                                                         baseFreq,
                                                         pull,
                                                         streamId,
                                                         poolSize,
                                                         &clockDomain,
                                                         dmac));

  clockDomain.reset(srClass, uint64_t(0u), 48000u);
  clockDomain.update(6u, 125000u, 125000u, 125000u);

  IasAvbPacketPool pool(*mClockRefStream->mLog);
  pool.init(1024, 1);
  IasAvbPacket * packet = pool.getPacket();
  memset(packet->getBasePtr(), 0, 1024);
  mClockRefStream->mActive = true;
  mClockRefStream->mRefPlaneEventCount = 0u;
  mClockRefStream->mRefPlaneEventTime = 0u;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  ASSERT_TRUE(mClockRefStream->writeToAvbPacket(packet, 0u));

  mClockRefStream->mClockValid         = false;
  // !isInitialized() || !isActive() || !isTransmitStream() (F || F || F)
  ASSERT_TRUE(mClockRefStream->writeToAvbPacket(packet, 0u));
}

}//namespace IasMediaTransportAvb
