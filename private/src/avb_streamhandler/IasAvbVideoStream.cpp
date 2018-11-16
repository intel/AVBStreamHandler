/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoStream.cpp
 * @brief   This is the implementation of the IasAvbVideoStream class.
 * @date    2014
 */

#include "avb_streamhandler/IasAvbVideoStream.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasLocalVideoBuffer.hpp"
#include "avb_helper/ias_safe.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <sstream>
#include <dlt/dlt_cpp_extension.hpp>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbVideoStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbVideoStream::IasAvbVideoStream()
  : IasAvbStream(IasAvbStreamHandlerEnvironment::getDltContext("_AVS"), eIasAvbVideoStream)
  , mVideoFormat(IasAvbVideoFormat::eIasAvbVideoFormatRtp)
  , mVideoFormatCode(0u)
  , mCompatibility(eCompCurrent)
  , mLocalStream(NULL)
  , mMaxPacketRate(0u)
  , mMaxPacketSize(0u)
  , mLaunchTimeDelta(0u)
  , mPacketLaunchTime(0u)
  , mLock()
  , mSeqNum(0u)
  , mRtpSequNrLast(0u)
  , mRtpSeqHighByte(0u)
  , mSampleIntervalNs(0u)
  , mWaitForData(false)
  , mValidationMode(cValidateOnce)
  , mDebugLogCount(0u)
  , mNumSkippedPackets(0u)
  , mNumPacketsToSkip(0u)
  , mDebugIn(true)
  , mValidationThreshold(0u)
  , mValidationCount(0u)
  , mMsgCount(0u)
  , mMsgCountMax(0u)
  , mLocalTimeLast(0u)
  , mDatablockSeqNum(0u)
{
  // do nothing
}


/*
 *  Destructor.
 */
IasAvbVideoStream::~IasAvbVideoStream()
{
  cleanup(); // calls derivedCleanup()
}


void IasAvbVideoStream::derivedCleanup()
{
  // disconnect
  if (isConnected())
  {
    (void) connectTo(NULL);
  }

  // revert to default values
  mMaxPacketRate = 0u;
  mMaxPacketSize = 0u;
  mLaunchTimeDelta = 0u;
  mMsgCount = 0u;
  mMsgCountMax = 0u;
  mLocalTimeLast = 0u;
}


IasAvbProcessingResult IasAvbVideoStream::initTransmit(IasAvbSrClass srClass, uint16_t maxPacketRate, uint16_t maxPacketSize,
    IasAvbVideoFormat format, const IasAvbStreamId & streamId, uint32_t poolSize, IasAvbClockDomain * clockDomain,
    const IasAvbMacAddress & dmac, bool preconfigured)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    if ((0u == maxPacketRate)
        || (0u == maxPacketSize)
       )
    {
      result = eIasAvbProcInvalidParam;
    }

    IasAvbTSpec tSpec(uint16_t(maxPacketSize + 24u), srClass);
    const uint32_t packetsPerSec = IasAvbTSpec::getPacketsPerSecondByClass(srClass);

    if (eIasAvbProcOK == result)
    {
      if (  ((1500u - cAvtpHeaderSize - cRtppHeaderSize) < maxPacketSize)
          ||(  (IasAvbVideoFormat::eIasAvbVideoFormatRtp != format)
             &&(IasAvbVideoFormat::eIasAvbVideoFormatIec61883 != format) )
         )
      {
        result = eIasAvbProcUnsupportedFormat;
      }
    }

    if (eIasAvbProcOK == result)
    {
     if (0u != packetsPerSec)
     {
       const uint16_t maxIntervalFrames = uint16_t((uint32_t(maxPacketRate) + packetsPerSec -1)/packetsPerSec); // roundup
       tSpec.setMaxIntervalFrames(maxIntervalFrames);
     }

     //tSpec.getRequiredBandwidth()
     result = IasAvbStream::initTransmit(tSpec, streamId, poolSize, clockDomain, dmac, tSpec.getVlanId(), preconfigured);
    }

    if (eIasAvbProcOK == result)
    {
      std::string comp;
      std::stringstream key;

      // if stream specific compatibility is requested honor it, else use configured compatibility
      key << IasRegKeys::cCompatibilityVideo << ".h.264." << std::hex << uint64_t(getStreamId());
      if (!IasAvbStreamHandlerEnvironment::getConfigValue(key.str(), comp))
      {
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cCompatibilityVideo, comp);
      }
      if (comp == "D5_1722a")
      {
        mCompatibility = eComp1722aD5;
      }
      else if (comp == "D9_1722a")
      {
        mCompatibility = eComp1722aD9;
      }
      else
      {
        mCompatibility = eCompCurrent;
      }

      mMaxPacketRate=maxPacketRate;
      mMaxPacketSize=maxPacketSize;
      mVideoFormat = format;
      AVB_ASSERT(0u != packetsPerSec);
      mLaunchTimeDelta = uint32_t((uint64_t(1000000000)/uint64_t(packetsPerSec * tSpec.getMaxIntervalFrames())));

      result = prepareAllPackets();
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error=", int32_t(result));
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbVideoStream::initReceive(IasAvbSrClass srClass, uint16_t maxPacketRate, uint16_t maxPacketSize,
    IasAvbVideoFormat format, const IasAvbStreamId & streamId, const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    if ((0u == maxPacketRate)
        || (0u == maxPacketSize)
       )
    {
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      if (  (1500u < maxPacketSize)
          ||(  (IasAvbVideoFormat::eIasAvbVideoFormatRtp != format)
             &&(IasAvbVideoFormat::eIasAvbVideoFormatIec61883 != format) )
         )
      {
        result = eIasAvbProcUnsupportedFormat;
      }
    }

    if (eIasAvbProcOK == result)
    {
      IasAvbTSpec tSpec(static_cast<uint16_t>(maxPacketSize + 24u), srClass);
      result = IasAvbStream::initReceive(tSpec, streamId, dmac, vid, preconfigured);
    }

    if (eIasAvbProcOK == result)
    {
      std::string comp;
      std::stringstream key;

      // if stream specific compatibility is requested honor it, else use configured compatibility
      key << IasRegKeys::cCompatibilityVideo << ".h.264." << std::hex << uint64_t(getStreamId());
      if (!IasAvbStreamHandlerEnvironment::getConfigValue(key.str(), comp))
      {
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cCompatibilityVideo, comp);
      }
      if (comp == "D5_1722a")
      {
        mCompatibility = eComp1722aD5;
      }
      else if (comp == "D9_1722a")
      {
        mCompatibility = eComp1722aD9;
      }
      else
      {
        mCompatibility = eCompCurrent;
      }

      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxValidationMode, mValidationMode);
      mValidationThreshold = 100u;
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxValidationThreshold, mValidationThreshold);
      mValidationCount = mValidationThreshold;
      mWaitForData = true;
      mNumSkippedPackets = 1u;
      uint32_t skipTime = 10000u; // us

      mMaxPacketRate=maxPacketRate;
      mMaxPacketSize=maxPacketSize;

      mVideoFormat = format;

      mVideoFormatCode = getFormatCode(mVideoFormat);

      if (!IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, " no RX clock update interval configured! Set to",
            skipTime, "us");
      }
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error=", int32_t(result));
    }
  }
  return result;
}


IasAvbProcessingResult IasAvbVideoStream::prepareAllPackets()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(isInitialized());
  AVB_ASSERT(isTransmitStream());

  IasAvbPacket * referencePacket = getPacketPool().getPacket();
  const IasAvbMacAddress * sourceMac = IasAvbStreamHandlerEnvironment::getSourceMac();

  if (NULL == referencePacket)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Failed to get a reference packet from the pool!");
    result = eIasAvbProcInitializationFailed;
  }
  else if (NULL == sourceMac)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " No source MAC address!");
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    using std::memcpy;
    /*
     *  Fill in reference packet. This is still a mess of hard-coded magic numbers and
     *  should be replaced by a more elegant mechanism rather sooner than later.
     */

    uint8_t* const packetStart = static_cast<uint8_t*>(referencePacket->getBasePtr());
    uint8_t* packetData = packetStart;

    // destination MAC
    avb_safe_result copyResult = avb_safe_memcpy(packetData, cIasAvbMacAddressLength, &getDmac(), cIasAvbMacAddressLength);
    AVB_ASSERT(e_avb_safe_result_ok == copyResult);

    packetData += cIasAvbMacAddressLength;

    // source MAC
    copyResult = avb_safe_memcpy(packetData, cIasAvbMacAddressLength, sourceMac, cIasAvbMacAddressLength);
    AVB_ASSERT(e_avb_safe_result_ok == copyResult);
    (void) copyResult;

    packetData += cIasAvbMacAddressLength;

    // VLAN tag
    *(packetData++) = 0x81u;
    *(packetData++) = 0x00u;
    *(packetData++) = uint8_t(getVlanData() >> 8);
    *(packetData++) = uint8_t(getVlanData());

    /* 1722 header update + payload */
    *(packetData++) = 0x22u; // 1722 Ethtype high
    *(packetData++) = 0xF0u; // 1722 Ethtype low
    *(packetData++) = getFormatCode(mVideoFormat); // subtype: 1722a(D8): CVF:Compressed Video Format = 3   or IEC 61883/IIDC Format = 0
    if (3u == getFormatCode(mVideoFormat))
    {
      *(packetData++) = 0x81u; //  SV_VER_MR_RS_GV_TV
    }
    else
    {
      *(packetData++) = 0x80u; // SV_VER_MR_RS_GV_TV - 1722-D4 6.4.4.2 talker shall set TV to 0 for CIP with SPH
    }
    packetData++;           // sequence number, filled in per packet or at connect
    *(packetData++) = 0x00u; // TU;

    // streamId
    getStreamId().copyStreamIdToBuffer(packetData, 8u);
    packetData += 8;

    packetData += 4;        // time stamp, filled in per packet

    if (3u == getFormatCode(mVideoFormat))
    {
      // H.264
      if (eComp1722aD5 == mCompatibility)
      {
        *(packetData++) = getVideoFormatCode(mVideoFormat);   // mVideoFormatCode;  // format RFC Payload type: 2
        *(packetData++) = 0u;   // format_subtype 0: MJPEG 1: H264 2: JPEG2000
        *(packetData++) = 0u;   // reserved
        *(packetData++) = 1u;   // reserved
      }
      else
      {
        // unknown compatibility, use standard
        *(packetData++) = getVideoFormatCode(mVideoFormat);   // mVideoFormatCode;  // format RFC Payload type: 2
        *(packetData++) = 1u;   // format_subtype 0: MJPEG 1: H264 2: JPEG2000
        *(packetData++) = 0u;   // reserved
        *(packetData++) = 0u;   // reserved
      }
    }
    else
    {
      // IEC 61883
      // gateway-info make sure all bytes are zero
      *(packetData++) = 0x00u;
      *(packetData++) = 0x00u;
      *(packetData++) = 0x00u;
      *(packetData++) = 0x00u;
    }

    packetData += 2;        // stream data length, filled in per packet

    if (3u == getFormatCode(mVideoFormat))
    {
      packetData += 1;        // M field/evt, filled in per packet
    *(packetData++) = 0u;   // reserved
    }
    else
    {
      // IEC61883
      *(packetData++) = 0x5F; // tag 0x01 & channel 0x31
      *(packetData++) = 0xA0; // tcode 0x1010 & sy 0x00
    }

    /*
     * We need to state the length here for the init mechanism. Note that the actual packets
     * that will be sent out later might vary in length, so the packet->len and stream_data_length
     * fields should be set every time a packet is generated.
     */
    referencePacket->len = uint32_t(packetData - packetStart);  // currently 18 + 24 = 42 (AVTP header size) - H.264

    if (3u == getFormatCode(mVideoFormat))
    {
      // set payload offset to position following AVTP header
      if ( (eComp1722aD5 == mCompatibility) || (eComp1722aD9 == mCompatibility) )
      {
        referencePacket->setPayloadOffset(cEthHeaderSize + cAvtpHeaderSize);
      }
      else
      {
        // draft 14 - add rtp timestamp
        referencePacket->len += 4u;
        // we adjust the payload offset here to reflect the video
        // payload offset rather than the avtp payload offset so
        // that the rpt-timestamp is not overwritten.
        referencePacket->setPayloadOffset(cEthHeaderSize + cAvtpHeaderSize + 4u);
      }
    }
    else
    {
      // CIP header - 8 bytes
      referencePacket->setPayloadOffset(cEthHeaderSize + cAvtpHeaderSize + cCIPHeaderSize);
    }

    // now copy the template to all other packets in the pool
    result = getPacketPool().initAllPacketsFromTemplate(referencePacket);

    (void) IasAvbPacketPool::returnPacket(referencePacket);
  }

  if (eIasAvbProcOK != result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error=", int32_t(result));
  }

  return result;
}


void IasAvbVideoStream::activationChanged()
{
  /*
   *  acquire the lock to ensure the calling thread waits
   *  until we completely processed packet data in the other tread
   */
  mLock.lock();

  if (isActive())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " activating stream");
    // stream has just been activated
    mWaitForData = true;
    mPacketLaunchTime = 0u;
  }

  if (isConnected())
  {
    AVB_ASSERT(NULL != mLocalStream);
    mLocalStream->setClientActive(isActive());
  }

  mLock.unlock();
}

void IasAvbVideoStream::resetTime(bool hard)
{
  /* After activation of the stream (or upon the end of a buffer underrun condition),
   * the first samples pass through the reference pane, so set launch time and
   * presentation time accordingly. From then on, both times will be maintained
   * incrementally.
   */
  IasLibPtpDaemon* const ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();

  if (NULL != ptp)
  {
    uint64_t localTime = ptp->getRealLocalTime();
    AVB_ASSERT(0u != localTime);
    int64_t launchDelta =  int64_t(localTime-mPacketLaunchTime);

    if (hard || (0u == mRefPaneSampleTime))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " hard reset, old refPlane = ",
          mRefPaneSampleTime,
          "old launch time=", mPacketLaunchTime,
          "now =", localTime);

      mPacketLaunchTime = localTime;
      mRefPaneSampleTime = uint32_t(localTime);
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " soft reset, old refPlane = ",
          mRefPaneSampleTime,
          "old launch time=", mPacketLaunchTime,
          "now =", localTime,
          "launchDelta =", launchDelta);

      // if launch time is already in the future, do nothing
      if (launchDelta > 0L)
      {
        mPacketLaunchTime += launchDelta;
        mRefPaneSampleTime = uint32_t(int64_t(mRefPaneSampleTime) + launchDelta);
      }
    }
    mRefPaneSampleCount = 0u;

  }
}

IasAvbPacket* IasAvbVideoStream::preparePacket(uint64_t nextWindowStart)
{
   (void) nextWindowStart;
   IasAvbPacket* packet = NULL;
   IasLocalVideoBuffer::IasVideoDesc  descPacket;
   uint16_t rtpSequenceNumberCurrent = 0;

   descPacket.avbPacket = NULL;

   if (isInitialized() && isTransmitStream() && isConnected())
   {
     AVB_ASSERT(NULL != mLocalStream);

     mLocalStream->readLocalVideoBuffer(NULL, &descPacket);

     if(NULL != descPacket.avbPacket)
     {
       packet = descPacket.avbPacket;
     }

     if (NULL != packet)
     {
       // rtp sequence number check
       rtpSequenceNumberCurrent = descPacket.rtpSequenceNumber;
       if((descPacket.rtpSequenceNumber != static_cast<uint16_t>(mRtpSequNrLast + 1)) && (mRtpSequNrLast != 0))
       {
         DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " SEQUENCE ERROR rtpSequenceNumber: ",
             rtpSequenceNumberCurrent,
             "rtpSequenceNumberLast: ",
             mRtpSequNrLast);
       }
       mRtpSequNrLast=rtpSequenceNumberCurrent;

       // statistics: msgs/s
       struct timespec tp;
       (void) clock_gettime(CLOCK_MONOTONIC, &tp);
       uint64_t localTime = (uint64_t(tp.tv_sec) * uint64_t(1000000000u)) + tp.tv_nsec;
       if ((localTime-mLocalTimeLast)<=cObservationInterval) //default: 1s
       {
         mMsgCount++;
       }
       else
       {
         mLocalTimeLast=localTime;
         if(mMsgCount > mMsgCountMax)
         {
           mMsgCountMax=mMsgCount;
           DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " msgs/s max: ", mMsgCountMax);
         }
         mMsgCount=0;
       }

       if (!finalizeAvbPacket( &descPacket ))
       {
         // packet preparation failed, dispose packet
         IasAvbPacketPool::returnPacket( packet );
         packet = NULL;
         DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " AVB video packet preparation failed, dispose packet");
       }
     }
     else
     {
       // prepare dummy packet
       packet = getPacketPool().getDummyPacket();
       if (NULL != packet)
       {
         if (!prepareDummyAvbPacket(packet))
          {
            // packet preparation failed, dispose packet
            IasAvbPacketPool::returnPacket( packet );
            packet = NULL;
            DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " prepareDummyAvbPacket failed, dispose packet");
          }
       }
       else
       {
         DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " AVB video dummy packet preparation failed - Packet Pool empty");
       }
     }
   }

   return packet;
}


bool IasAvbVideoStream::writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart)
{
  (void)packet;
  (void)nextWindowStart;

  // Unused dummy method that is required due to being pure virtual in the base class.

  return false;
}


bool IasAvbVideoStream::finalizeAvbPacket(IasLocalVideoBuffer::IasVideoDesc *descPacket)
{
  bool result = true;
  IasAvbPacket* packet = NULL;
  uint8_t datablocksInPac = 0;

  mLock.lock();

  if (NULL == descPacket || !isInitialized() || !isActive() || !isTransmitStream())
  {
    result = false;
  }
  else
  {
    packet = descPacket->avbPacket;
    AVB_ASSERT(NULL != packet);
    AVB_ASSERT(NULL != packet->getBasePtr());
    uint8_t* const avtpBase8 = static_cast<uint8_t*>(packet->getBasePtr()) + ETH_HLEN + 4u; // consider VLAN tag
    uint16_t* const avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);
    uint32_t* const avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase8);

    if (0u == mPacketLaunchTime)
    {
      // stream has just been activated
      resetTime(true);
    }

    if (!descPacket->isIec61883Packet)
    {
      // write time stamp to packet
      if ( (eComp1722aD5 == mCompatibility) || (eComp1722aD9 == mCompatibility) )
      {
        avtpBase32[3] = descPacket->rtpTimestamp;
      }
      else
      {
        // draft 14
        avtpBase32[3] = htonl(mRefPaneSampleTime + getPresentationTimeOffset());
        avtpBase32[6] = descPacket->rtpTimestamp;
      }
    }
    else
    {
      // since TV bit is set to 0 for IEC 61883 this field is not used. Instead the sph header is used.
      avtpBase32[3] = 0u;
    }
    packet->attime = mPacketLaunchTime;

    uint16_t streamDataLength = static_cast<uint16_t>(descPacket->avbPacket->len - descPacket->avbPacket->getPayloadOffset());

    if (!descPacket->isIec61883Packet)
    {
      // sync avtp and rtp sequence numbers (8 bits only)
      avtpBase8[2] = uint8_t(descPacket->rtpSequenceNumber);
    }
    else
    {
      // insert sequence number and advance for next packet
      avtpBase8[2] = mSeqNum;
      mSeqNum++;
      streamDataLength = static_cast<uint16_t>(descPacket->avbPacket->len - descPacket->avbPacket->getPayloadOffset() + cCIPHeaderSize);
    }

    // set packet length and stream_data_length
    *(avtpBase16 + 10) = htons(streamDataLength);

    if (!descPacket->isIec61883Packet)
    {
      // set marker bit

      if (eComp1722aD5 == mCompatibility)
      {
        avtpBase8[22]= (descPacket->mptField & 0x80) ? 0x4u : 0x00u;
      }
      else
      {
        // unknown compatibility, use standard
        avtpBase8[22]= (descPacket->mptField & 0x80) ? 0x10u : 0x00u;
        if (eCompCurrent == mCompatibility)
        {
          avtpBase8[22] |= 0x20;       // set ptv bit
        }
      }
    }

    IasAvbClockDomain * const pClockDomain = getClockDomain();
    AVB_ASSERT(NULL != pClockDomain);

    // set timestamp uncertain bit, depending on lock state
    avtpBase8[3] = (IasAvbClockDomain::eIasAvbLockStateLocked == pClockDomain->getLockState()) ? 0x00u : 0x01u;

    // sph header
    if (descPacket->isIec61883Packet)
    {
      // IEC 61883i-4 CIP header
      avtpBase8[24] = 63u;                // qi_1 and SID
      avtpBase8[25] = 0x06;               // DBS
      avtpBase8[26] = 0xC4;               // FN_QPC_SPH_rsv, FN and QPC
      avtpBase8[27] = mDatablockSeqNum;   // DBC
      avtpBase8[28] = 0xA0;               // qi_2_FMT
      avtpBase8[29] = 0x00;               // FDF
      avtpBase8[30] = 0x00;
      avtpBase8[31] = 0x00;

      // setup sequence number for next packet
      datablocksInPac = static_cast<uint8_t>(streamDataLength/24u); // DBS * 4
      mDatablockSeqNum = static_cast<uint8_t>(mDatablockSeqNum + datablocksInPac);

      // add sph timestamp only if it is not provided in payload
      if (!descPacket->hasSph)
      {
        for (uint32_t i = 32u; i < streamDataLength; i += 192u)
        {
          avtpBase32[i/4] = htonl(mRefPaneSampleTime + getPresentationTimeOffset());
        }
      }
    }

    // advance ref pane for next packet
    const uint32_t deltaTime = mLaunchTimeDelta;

    mRefPaneSampleTime += deltaTime;
    mPacketLaunchTime += deltaTime;
  }

  mLock.unlock();

  return result;
}

bool IasAvbVideoStream::prepareDummyAvbPacket(IasAvbPacket* packet)
{
  bool result = true;

  mLock.lock();

  if (!isInitialized() || !isActive() || !isTransmitStream())
  {
    result = false;
  }
  else
  {
    AVB_ASSERT(NULL != packet);
    AVB_ASSERT(NULL != packet->getBasePtr());
    uint8_t* const avtpBase8 = static_cast<uint8_t*>(packet->getBasePtr()) + ETH_HLEN + 4u; // consider VLAN tag
    uint16_t* const avtpBase16 = reinterpret_cast<uint16_t*>(avtpBase8);

    if ((0u == mPacketLaunchTime))
    {
      // stream has just been activated
      resetTime(true);
    }

    packet->attime = mPacketLaunchTime;

    // set packet length and stream_data_length
    const uint16_t streamDataLength = 0;
    *(avtpBase16 + 10) = htons(streamDataLength);
    packet->len = streamDataLength + IasAvbTSpec::cIasAvbPerFrameOverhead + cAvtpHeaderSize;

    IasAvbClockDomain * const pClockDomain = getClockDomain();
    AVB_ASSERT(NULL != pClockDomain);

    // set timestamp uncertain bit, depending on lock state
    avtpBase8[3] = (IasAvbClockDomain::eIasAvbLockStateLocked == pClockDomain->getLockState()) ? 0x00u : 0x01u;

    // advance ref pane for next packet
    const uint32_t deltaTime = mLaunchTimeDelta;//uint32_t((double(written) * mSampleIntervalNs * pClockDomain->getRateRatio() * mRatioBendCurrent) + 0.5);

    mRefPaneSampleTime += deltaTime;
    mPacketLaunchTime += deltaTime;
  }

  mLock.unlock();

  return result;
}


void IasAvbVideoStream::readFromAvbPacket(const void* const packet, const size_t length)
{
  IasLocalVideoBuffer::IasVideoDesc descPacket;

  mLock.lock();

  if (isInitialized() && isReceiveStream())
  {
    IasAvbStreamState newState = IasAvbStreamState::eIasAvbStreamInvalidData;

    const uint8_t* const avtpBase8 = static_cast<const uint8_t*>(packet);
    const uint16_t* const avtpBase16 = static_cast<const uint16_t*>(packet);
    const uint32_t* const avtpBase32 = static_cast<const uint32_t*>(packet);

    const IasAvbStreamState oldState = mStreamStateInternal;
    size_t payloadLength = 0u;

    if (NULL != packet)
    {
      payloadLength = ntohs(avtpBase16[10]);
    }

    uint32_t validationStage = 0u;

    if (NULL == packet)
    {
      newState = IasAvbStreamState::eIasAvbStreamNoData;
      mWaitForData = true;
    }
    else
    {
      // @@DIAG inc FRAMES_RX
      mDiag.setFramesRx(mDiag.getFramesRx()+1);

      if (cValidateNever == mValidationMode)
      {
        // just assume a healthy packet (should only be used under lab/debugging conditions)
        newState = IasAvbStreamState::eIasAvbStreamValid;
      }
      else
      {
        if ((cValidateAlways == mValidationMode) || (IasAvbStreamState::eIasAvbStreamValid != getStreamState()))
        {

          // validate length
          if (length >= cAvtpHeaderSize)
          {
            validationStage++;
            if (0x00 == avtpBase8[0]) // AVTP IEC61883 video format
            {
              validationStage++;
              descPacket.isIec61883Packet = true;
              // Iec 61883-4 requires cip headers, so the tag field must be set
              // tag = 0 -> IIDC
              // tag = 1 -> IEC61883
              if (0x40 == (avtpBase8[22] & 0x40))
              {
                validationStage++;
                // check if sph is in use
                if (0x4 == (avtpBase8[26] & 0x4))
                {
                  validationStage++;
                  descPacket.hasSph = true;
                }
                else
                {
                  descPacket.hasSph = false;
                }

                // validate stream data length
                // CIP Header is not part of stream data length
                size_t bufferSize = payloadLength - cCIPHeaderSize;

                // ignore potential padding, just make sure packet is long enough
                if ((length - cAvtpHeaderSize) >= bufferSize)
                {
                  validationStage++;
                  newState = IasAvbStreamState::eIasAvbStreamValid;
                }

                // Make sure that payload length is multiple of TSP
                if (descPacket.hasSph)
                {
                  if (0 != (bufferSize % 192))
                  {
                    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " Payload not multiple of 192!!");
                  }
                }
                else
                {
                  if (0 != (bufferSize % 188))
                  {
                    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " Payload not multiple of 188!!");
                  }
                }
              }
              else
              {
                newState = IasAvbStreamState::eIasAvbStreamInvalidData;
                DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, " IEC 61883-4 packet without CIP");
              }
            }
            else
            {
              validationStage++;

              // validate AVTP subtype
              if (0x03 == avtpBase8[0]) // AVTP compressed video format
              {
                validationStage++;
                descPacket.isIec61883Packet = false;
                // validate format
                if (avtpBase8[16] == 0x02u) // RFC Video payload type
                {
                  validationStage++;

                  // validate stream data length
                  // ignore potential padding, just make sure packet is long enough
                  if ((length - cAvtpHeaderSize) >= payloadLength)
                  {
                    validationStage++;
                    newState = IasAvbStreamState::eIasAvbStreamValid;
                  }
                }
              }
            }
          }

          if (newState != IasAvbStreamState::eIasAvbStreamValid)
          {
            // @@DIAG inc UNSUPPORTED FORMAT
            mDiag.setUnsupportedFormat(mDiag.getUnsupportedFormat()+1);
          }
        }

        if (IasAvbStreamState::eIasAvbStreamValid == oldState)
        {
          if (avtpBase8[2] == uint8_t(mSeqNum + 1u))
          {
            newState = IasAvbStreamState::eIasAvbStreamValid;
            if (0x00 == avtpBase8[0])
            {
              descPacket.isIec61883Packet = true;
            }
            else
            {
              descPacket.isIec61883Packet = false;

              // increase rtp sequence number high byte in case of avtp sequence number overflow
              if(avtpBase8[2] < uint8_t(mSeqNum))
              {
                mRtpSeqHighByte++;
              }
            }
          }
          else
          {
            // @DIAG inc SEQ_NUM_MISMATCH
            mDiag.setSeqNumMismatch(mDiag.getSeqNumMismatch()+1);
            DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX,
              "1722 ANNEX E - SEQ_NUM_MISMATCH (", mDiag.getSeqNumMismatch(), ")");

            newState = IasAvbStreamState::eIasAvbStreamInvalidData;
          }
        }
      }

      // generate RTP sequence number
      descPacket.rtpSequenceNumber = uint16_t((uint16_t(mRtpSeqHighByte) << 8) | uint16_t(avtpBase8[2])) ;

      mSeqNum = avtpBase8[2];
    }

    if ((IasAvbStreamState::eIasAvbStreamValid == newState) && (mValidationCount > 0u))
    {
      mValidationCount--;
      // defer setting stream state to "valid" for a given number of subsequent valid packets
      if (0u == mValidationCount)
      {
        setStreamState(newState);
        if (isConnected())
        {
          mLocalStream->setClientActive(true);
        }
      }
    }

    if (newState != oldState)
    {
      mStreamStateInternal = newState;
      if (IasAvbStreamState::eIasAvbStreamValid == oldState)
      {
        uint64_t id = getStreamId();
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " stream data invalid, stage:", int32_t(validationStage),
            "stream:", id);

        mValidationCount = mValidationThreshold;
        setStreamState(newState);
        if (isConnected())
        {
          mLocalStream->setClientActive(false);
        }
      }
      else
      {
        // do nothing, local side already set to inactive
      }
    }

    if (IasAvbStreamState::eIasAvbStreamValid == newState)
    {

      if (isConnected())
      {
        AVB_ASSERT(NULL != mLocalStream);

        if(payloadLength)
        {
          descPacket.dts = 0;
          descPacket.pts = 0;
          if (!descPacket.isIec61883Packet)
          {
            descPacket.buffer.data = const_cast<uint8_t*>(avtpBase8 + 24);

            // 12 bytes of avtp header removed here. 12 more bytes removed when reconstructing the rtp header for the local buffer.
            // Draft 14 comapatible mode will remove the rtp-timestamp as well
            if ( (eComp1722aD5 == mCompatibility) || (eComp1722aD9 == mCompatibility) )
            {
              descPacket.rtpPacketPtr = const_cast<uint8_t*>(avtpBase8 + 12);  // RTP header with 12 byte is used for rtp packet reconstruction
            }
            else
            {
              descPacket.rtpPacketPtr = const_cast<uint8_t*>(avtpBase8 + 16); // +rtp-timestamp len
            }
            descPacket.buffer.size = static_cast<uint32_t>(payloadLength);
          }
          else
          {
            descPacket.buffer.data = const_cast<uint8_t*>(avtpBase8 + 32); // skip CIP header, start data with the first sph
            descPacket.buffer.size = static_cast<uint32_t>(payloadLength - cCIPHeaderSize); // skip CIP header
            descPacket.rtpPacketPtr = const_cast<uint8_t*>(avtpBase8 + 12);  // RTP header is not used for IEC61884-4, but needed for ptr check currently.
          }
          if ( (eComp1722aD5 == mCompatibility) || (eComp1722aD9 == mCompatibility) )
          {
            descPacket.rtpTimestamp = /*ntohl*/(avtpBase32[3]); // no conversion needed for rtpdepay function
          }
          else
          {
            // draft 14 - if ptv is set use rtp time stamp, else assign 0
            if (avtpBase8[22] & 0x20)
            {
              descPacket.rtpTimestamp = /*ntohl*/(avtpBase32[6]);
            }
            else
            {
              descPacket.rtpTimestamp = 0;
            }
          }

          if (eComp1722aD5 == mCompatibility)
          {
            descPacket.mptField = (avtpBase8[22] & 0x04) ? 0xE0u : 0x60u; // hardcoded H264 use-case
          }
          else
          {
            // unknown compatibility, use standard
            descPacket.mptField = (avtpBase8[22] & 0x10) ? 0xE0u : 0x60u; // hardcoded H264 use-case
          }

          mLocalStream->writeLocalVideoBuffer(NULL,&descPacket);
        }
      }
    }

    if (IasAvbStreamState::eIasAvbStreamNoData == newState)
    {
      // Nothing to send, but writting null to localstram notes that we're still alive
      mLocalStream->writeLocalVideoBuffer(NULL, NULL);
    }
  }

  mLock.unlock();
}


IasAvbProcessingResult IasAvbVideoStream::connectTo(IasLocalVideoStream* localStream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  (void)localStream;

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    if (localStream != mLocalStream)
    {
      mLock.lock();

      // first, disconnect from old stream, if any
      if (NULL != mLocalStream)
      {
        mLocalStream->disconnect();
        mLocalStream = NULL;
      }

      if (NULL != localStream)
      {

        // parameter check if streams could be connected
        if ((0u == mMaxPacketRate)
            || (0u == mMaxPacketSize)
            || (mMaxPacketRate != localStream->getMaxPacketRate())
            || (mMaxPacketSize != localStream->getMaxPacketSize())
            || (getDirection() != localStream->getDirection())
           )
        {
          result = eIasAvbProcInvalidParam;
        }

        if (eIasAvbProcOK == result)
        {
          result = localStream->connect(this);
        }

        if (((IasAvbStreamDirection::eIasAvbTransmitToNetwork==localStream->getDirection())) && (eIasAvbProcOK == result))
        {
          result = localStream->setAvbPacketPool(&getPacketPool());
        }

        if (eIasAvbProcOK == result)
        {
          localStream->setClientActive(isTransmitStream() && isActive());
          mLocalStream = localStream;
        }
      }

      mLock.unlock();
    }
  }

  if (eIasAvbProcOK != result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " Error=", int32_t(result));
  }

  return result;
}


bool IasAvbVideoStream::signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples)
{
  bool requestReset = false;
  (void) numSamples;
  (void) event;
  return requestReset;
}


uint8_t IasAvbVideoStream::getFormatCode(const IasAvbVideoFormat format)
{
  uint8_t code = 0u;

  // Related to 1722a(D8)
  // CVF:Compressed Video Format = 3
  // IEC 61883/IIDC Format = 0


  if ((format == IasAvbVideoFormat::eIasAvbVideoFormatRtp)) // H.264
  {
    code = 3u;
  }
  else if (format == IasAvbVideoFormat::eIasAvbVideoFormatIec61883) // MpegTS
  {
    code = 0u;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " AvbVideoFormat wrong!");
  }

  return code;
}


uint8_t IasAvbVideoStream::getVideoFormatCode(const IasAvbVideoFormat format)
{
  uint8_t code = 0u;

  // 1722a(D8): CVF:Compressed Video Format = 3   or IEC 61883/IIDC Format = 0
  if ((format == IasAvbVideoFormat::eIasAvbVideoFormatRtp)) // H.264
  {
    code = 2u; // RFC Payload Type
  }
  else if (format == IasAvbVideoFormat::eIasAvbVideoFormatIec61883) // MpegTS
  {
    code = 0u;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " AvbVideoFormat wrong!");
  }

  return code;
}



} // namespace IasMediaTransportAvb
