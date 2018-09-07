/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockReferenceStream.cpp
 * @brief   This is the implementation of the IasAvbClockReferenceStream class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_helper/ias_safe.h"


#include <arpa/inet.h>
#include <cstdlib>
#include <linux/if_ether.h>
#include <cstring>
#include <dlt/dlt_cpp_extension.hpp>

// ntohll not defined in in.h, so we have to create something ourself
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cond_swap64(x) __bswap_64(x)
#else
#define cond_swap64(x) (x)
#endif


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbClockReferenceStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbClockReferenceStream::IasAvbClockReferenceStream()
  : IasAvbStream(IasAvbStreamHandlerEnvironment::getDltContext("_ACS"), eIasAvbClockReferenceStream)
  , mType(IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio)
  , mBaseFrequency(0u)
  , mPull(IasAvbClockMultiplier::eIasAvbCrsMultFlat)
  , mTimeStampInterval(0u)
  , mTimeStampsPerPdu(0u)
  , mMediaClockRestartToggle(0u)
  , mRefPlaneEventCount(0u)
  , mRefPlaneEventTime(0u)
  , mMasterCount(0u)
  , mLastMasterCount(0u)
  , mMasterTime(0u)
  , mLastMasterTime(0u)
  , mMasterTimeout(0u)
  , mRefPlaneEventOffset(0)
  , mPacketLaunchTime(0u)
  , mLock()
  , mSeqNum(0u)
  , mClockValid(true)
  , mHoldoffTime(0u)
  , mValidationMode(cValidateOnce)
  , mValidationThreshold(0u)
  , mValidationCount(0u)
  , mFirstRun(true)
  , mBTMEnable(false)
  , mMasterTimeUpdateMinInterval(0u)
{
  // do nothing
}


/*
 *  Destructor.
 */
IasAvbClockReferenceStream::~IasAvbClockReferenceStream()
{
  cleanup(); // calls derivedCleanup()
}


void IasAvbClockReferenceStream::derivedCleanup()
{
  // revert to default values
  mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
  mBaseFrequency = 0u;
  mPull = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
  mTimeStampInterval = 0u;
  mTimeStampsPerPdu = 0u;
  mMediaClockRestartToggle = 0u;
  mRefPlaneEventCount = 0u;
  mRefPlaneEventTime = 0u;
  mPacketLaunchTime = 0u;
  mMasterCount = 0u;
  mLastMasterCount = 0u;
  mMasterTime = 0u;
  mLastMasterTime = 0u;
  mMasterTimeout = 0u;
  mRefPlaneEventOffset = 0;
}


IasAvbProcessingResult IasAvbClockReferenceStream::initTransmit(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type,
    uint16_t crfStampsPerPdu, uint16_t crfStampInterval, uint32_t baseFreq, IasAvbClockMultiplier pull,
    const IasAvbStreamId & streamId, uint32_t poolSize, IasAvbClockDomain * clockDomain, const IasAvbMacAddress & dmac)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBootTimeMeasurement, mBTMEnable);

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    initFormat();

    if ((0u == crfStampsPerPdu)
        || (0u == crfStampInterval)
        || (0u == baseFreq)
        || (0x1FFFFFFFu < baseFreq)
        || (NULL == clockDomain)
       )
    {
      result = eIasAvbProcInvalidParam;
    }
    else if ((IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio != type)
      || (IasAvbClockMultiplier::eIasAvbCrsMultFlat != pull)
      )
    {
      result = eIasAvbProcUnsupportedFormat;
    }
    else
    {
      result = eIasAvbProcOK;
    }

    if (eIasAvbProcOK == result)
    {
      uint32_t packetsPerSecond = baseFreq / crfStampsPerPdu / crfStampInterval;
      uint16_t packetSize = uint16_t(mCrfHeaderSize + mPayloadHeaderSize + (cCrfTimeStampSize * crfStampsPerPdu));

#if DEBUG_LAUNCHTIME
      packetSize += 8u;
#endif

      if (packetSize >= ETH_DATA_LEN)
      {
        result = eIasAvbProcInvalidParam;
      }
      else
      {
        const uint32_t packetsPerSecondClass = IasAvbTSpec::getPacketsPerSecondByClass(srClass);
        if (0u == packetsPerSecondClass)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Illegal SR class configuration (packetsPerSec = 0, class code =",
              uint32_t(srClass));
          result = eIasAvbProcInvalidParam;
        }
        else
        {
          uint16_t maxIntervalFrames = uint16_t((packetsPerSecond + packetsPerSecondClass - 1u) / packetsPerSecondClass);
          IasAvbTSpec tSpec(packetSize, srClass, maxIntervalFrames);
          result = IasAvbStream::initTransmit(tSpec, streamId, poolSize, clockDomain, dmac, tSpec.getVlanId(), true);
        }
      }
    }

    if (eIasAvbProcOK == result)
    {
      mMasterTimeout = 2000000000; // default is 2 seconds
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioClockTimeout, mMasterTimeout);

      mBaseFrequency = baseFreq;
      mPull = pull;
      mTimeStampInterval = crfStampInterval;    // usually 64 (x 750 x 1 = 48000 Hz) or 160 (x 50 x 6 = 48000 Hz)
      mTimeStampsPerPdu = crfStampsPerPdu;      // usually 1 (750 PDUs per sec) or 6 (50 PDUs per sec)

      mRefPlaneEventCount = 0u;
      mRefPlaneEventTime = 0u;

      mClockValid = (IasAvbClockDomain::eIasAvbLockStateLocked == clockDomain->getLockState());

      const double sampleIntervalNs = 1.0e9 / double(baseFreq);
      const uint32_t ptOffsetOrig = getPresentationTimeOffset();
      const uint32_t steps = adjustPresentationTimeOffset((uint32_t)sampleIntervalNs);

      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "adjusted presentation time offset:",
                  "new =", getPresentationTimeOffset(), "orig =", ptOffsetOrig,
                  "stepWidth =", uint32_t(sampleIntervalNs), "steps =", steps);

      result = prepareAllPackets();

      if (IasAvbClockDomainType::eIasAvbClockDomainRaw == clockDomain->getType())
      {
        uint64_t val = 0u;
        IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRawXTimestamp, val);
        if (2u == val) // rev.2
        {
          mMasterTimeUpdateMinInterval = uint64_t(1e6);

          if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitClkUpdateInterval, val))
          {
            mMasterTimeUpdateMinInterval = val;
          }

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "TX clock update interval =", mMasterTimeUpdateMinInterval, "ns");
        }
      }
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error=", int32_t(result));
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbClockReferenceStream::initReceive(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type,
    uint16_t maxCrfStampsPerPdu, const IasAvbStreamId & streamId, const IasAvbMacAddress & dmac)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    initFormat();

    if (0u == maxCrfStampsPerPdu)
    {
      result = eIasAvbProcInvalidParam;
    }
    else if (IasAvbClockReferenceStreamType::eIasAvbCrsTypeUser == type)
    {
      result = eIasAvbProcUnsupportedFormat;
    }

    if (eIasAvbProcOK == result)
    {
      IasAvbTSpec tSpec(uint16_t(mCrfHeaderSize + mPayloadHeaderSize + maxCrfStampsPerPdu * cCrfTimeStampSize), srClass);

      result = IasAvbStream::initReceive(tSpec, streamId, dmac, tSpec.getVlanId(), true);

      if (eIasAvbProcOK == result)
      {
        mType = type;
        mTimeStampsPerPdu = maxCrfStampsPerPdu;
        mRefPlaneEventCount = 0u;
        mRefPlaneEventTime = 0u;
        mHoldoffTime = 15000000u; // 50 PDUs/sec, plus some margin for the sender being too fast
        if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cCrfRxHoldoff, mHoldoffTime))
        {
          mHoldoffTime *= 1000000u; // ms to ns
        }
        mClockValid = false;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxValidationMode, mValidationMode);
        mValidationThreshold = 100u;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxValidationThreshold, mValidationThreshold);
        mValidationCount = mValidationThreshold;
      }
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error=", int32_t(result));
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbClockReferenceStream::prepareAllPackets()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(isInitialized());
  AVB_ASSERT(isTransmitStream());

  IasAvbPacket * referencePacket = getPacketPool().getPacket();
  const IasAvbMacAddress * sourceMac = IasAvbStreamHandlerEnvironment::getSourceMac();

  if (NULL == referencePacket)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to get a reference packet from the pool!");
    result = eIasAvbProcInitializationFailed;
  }
  else if (NULL == sourceMac)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "No source MAC address!");
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    using std::memcpy;
    avb_safe_result copyresult;
    (void) copyresult;

    /*
     *  Fill in reference packet. This is still a mess of hard-coded magic numbers and
     *  should be replaced by a more elegant mechanism rather sooner than later.
     */

    uint8_t* const packetStart = static_cast<uint8_t*>(referencePacket->getBasePtr());
    uint8_t* packetData = packetStart;

    // destination MAC
    copyresult = avb_safe_memcpy(packetData, cIasAvbMacAddressLength, &getDmac(), sizeof (IasAvbMacAddress));
    AVB_ASSERT(e_avb_safe_result_ok == copyresult);
    packetData += cIasAvbMacAddressLength;

    // source MAC
    copyresult = avb_safe_memcpy(packetData, cIasAvbMacAddressLength, sourceMac, sizeof *sourceMac);
    AVB_ASSERT(e_avb_safe_result_ok == copyresult);
    packetData += cIasAvbMacAddressLength;

    // VLAN tag
    *(packetData++) = 0x81u;
    *(packetData++) = 0x00u;
    *(packetData++) = uint8_t(getVlanData() >> 8);
    *(packetData++) = uint8_t(getVlanData());

    /* 1722 header */
    *(packetData++) = 0x22u; // 1722 Ethtype high
    *(packetData++) = 0xF0u; // 1722 Ethtype low

    if (eIasAvbCompD6 == mCompatibilityMode)
    {
      *(packetData++) = 5;     // cd field 0 and subtype of CRS
      *(packetData++) = 0x80u; // sv=1, version, mr, r, cs, tu all 0
      packetData++;            // sequence number, filled in per packet
      *(packetData++) = 0;     // reserved

      // streamId
      getStreamId().copyStreamIdToBuffer(packetData, 8u);
      packetData += 8;

      // AVTP Time - reserved
      *reinterpret_cast<uint32_t*>(packetData) = 0;
      packetData += 4;

      // Format Info - reserved
      *reinterpret_cast<uint32_t*>(packetData) = 0;
      packetData += 4;

      // packet info: stream_data_length and type
      *reinterpret_cast<uint16_t*>(packetData) = htons((uint16_t) (mPayloadHeaderSize + (mTimeStampsPerPdu * cCrfTimeStampSize)));
      packetData += 2;
      *reinterpret_cast<uint16_t*>(packetData) = htons(mType);
      packetData += 2;

      // type header
      *(packetData++) = 7; 	// nominal frequency 7 (48 kHz)
      *(packetData++) = 0; 	// reserved
      *reinterpret_cast<uint16_t*>(packetData) = htons(mTimeStampInterval);
      packetData += 2;
    }
    // latest 1722 spec
    else
    {
      *(packetData++) = 4;     // subtype CRF
      *(packetData++) = 0x80u; // sv=1, version, mr, r, cs, tu all 0
      packetData++;            // sequence number, filled in per packet
      *(packetData++) = uint8_t(mType);

      // streamId
      getStreamId().copyStreamIdToBuffer(packetData, 8u);
      packetData += 8;

      // packet info, part 1: pull and base_frequency
      *reinterpret_cast<uint32_t*>(packetData) = htonl((mPull << 29) + mBaseFrequency);
      packetData += 4;

      // packet info, part 2: crf_data_length and timestamp_interval
      *reinterpret_cast<uint16_t*>(packetData) = htons((uint16_t) (mTimeStampsPerPdu * cCrfTimeStampSize));
      packetData += 2;
      *reinterpret_cast<uint16_t*>(packetData) = htons(mTimeStampInterval);
      packetData += 2;
    }

    /*
     * We need to state the length here for the init mechanism. Since all CRF packets are
     * supposed to have the same length, we can leave this untouched when filling in the payload
     * later on.
     */
    referencePacket->len = uint32_t(packetData - packetStart) + (mTimeStampsPerPdu * cCrfTimeStampSize);

    // now copy the template to all other packets in the pool
    result = getPacketPool().initAllPacketsFromTemplate(referencePacket);

    (void) IasAvbPacketPool::returnPacket(referencePacket);
  }

  if (eIasAvbProcOK != result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error=", int32_t(result));
  }

  return result;
}


void IasAvbClockReferenceStream::activationChanged()
{
  /*
   *  acquire the lock to ensure the calling thread waits
   *  until we completely processed packet data in the other tread
   */
  mLock.lock();

  if (isActive())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "activating stream");
    // stream has just been activated
    mRefPlaneEventTime = 0u;
    mRefPlaneEventCount = 0u;
  }

  mLock.unlock();
}

IasAvbCompatibility IasAvbClockReferenceStream::getCompatibilityMode()
{
  std::string compModeStr;
  IasAvbCompatibility compMode;

  IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cCompatibilityAudio, compModeStr);
  if ("SAF" == compModeStr)
  {
    compMode = eIasAvbCompSaf;
  }
  else if ("d6_1722a" == compModeStr)
  {
    compMode = eIasAvbCompD6;
  }
  else
  {
    compMode = eIasAvbCompLatest;
  }

  return compMode;
}


bool IasAvbClockReferenceStream::resetTime(uint64_t nextWindowStart)
{
  bool ret = false;
  IasAvbClockDomain * const clock = getClockDomain();
  AVB_ASSERT(NULL != clock);

  mMasterCount = clock->getEventCount(mMasterTime);
  const uint32_t eventRate = clock->getEventRate();
  if (0u == eventRate)
  {
    // clock info invalid
    mMasterTime = 0u;
  }
  else
  {
    mMasterCount = mMasterCount * mBaseFrequency / eventRate;
  }

  if (0u == mMasterTime)
  {
    // no ref clock available
    mRefPlaneEventCount = 0u;
    mRefPlaneEventTime = 0u;
    mPacketLaunchTime = nextWindowStart;

    // we abuse the offset to memorize that the warning has already been shown
    if (0u == mRefPlaneEventOffset)
    {
      mRefPlaneEventOffset = 1u;
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "no master clock available! clockId =",
          clock->getClockDomainId());
    }
  }
  else
  {
    // store offset between own sample count and master count
    mRefPlaneEventOffset = int64_t(mRefPlaneEventCount - mMasterCount);

    // adjust ref plane
    mRefPlaneEventTime = mMasterTime;

    /* adapt launch time
     * without syntonized mode, time needs to be converted from PTP time (ref plane) to I210 time
     * NOTE: right now, TX delay is applied in TX sequencer, so launch time can be equal ref plane time
     */
    mPacketLaunchTime = mRefPlaneEventTime;

    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "clockId", clock->getClockDomainId(), "start", nextWindowStart);
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX,
        "mT", mMasterTime,
        "mC", mMasterCount,
        "rT", mRefPlaneEventTime,
        "rC", mRefPlaneEventCount,
        "o", mRefPlaneEventOffset,
        "l", mPacketLaunchTime
    );

    ret = true;
  }

  mLastMasterCount = 0u;
  mLastMasterTime = 0u;

  return ret;
}


bool IasAvbClockReferenceStream::writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart)
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
    uint32_t* const avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase8);
    uint64_t* const crfStampBase64 = reinterpret_cast<uint64_t*>(avtpBase32 + mPayloadOffset32);

    if ((0u == mRefPlaneEventCount) && (0u == mRefPlaneEventTime))
    {
      if (!resetTime(nextWindowStart))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "resetTime failed due to missing reference clock");

        /* reset failed due to missing reference clock
         * prepare dummy packet instead. this will retrigger the sequencer to ask for another packet
         */
        packet->makeDummyPacket();
        packet->attime = mPacketLaunchTime + 10000000u; // wait 10ms before trying again

        return true;
      }
    }

    // insert sequence number and advance for next packet
    avtpBase8[2] = mSeqNum;
    mSeqNum++;

    if (mFirstRun && mBTMEnable)
    {
      mFirstRun = false;
      // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb prepared first crf packet",NULL);
    }

    IasAvbClockDomain * const pClockDomain = getClockDomain();
    AVB_ASSERT(NULL != pClockDomain);

    // advance ref plane for next packet
    AVB_ASSERT(0u != mMasterTime); // otherwise, we would have created a dummy packet above
    {
      uint64_t newMasterTime = 0u;
      uint64_t newMasterCount = pClockDomain->getEventCount(newMasterTime);
      newMasterCount = newMasterCount * mBaseFrequency / pClockDomain->getEventRate();

      if (newMasterTime != mMasterTime)
      {
        // this will also catch negative changes of master time
        if (((newMasterTime - mMasterTime) > mMasterTimeout)
            || !(newMasterCount > mMasterCount))
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "mater time out of expected interval, resetting.");
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "old time:", mMasterTime,
              "new time:", newMasterTime,
              "old count:", mMasterCount,
              "new count:", newMasterCount
              );

          // this will trigger resetTime for the next packet
          mRefPlaneEventTime = 0u;
          mRefPlaneEventCount = 0u;

          // this will skip the time calculation below
          mMasterTime = 0u;
        }
        else
        {
          bool doUpdate = true;
          if ((0u != mMasterTimeUpdateMinInterval) &&
              ((newMasterTime - mMasterTime) < mMasterTimeUpdateMinInterval))
          {
            doUpdate = false;
          }

          if (doUpdate)
          {
            // skip master time update in a short measurement interval to avoid accidental error of timestamp calculation
            mLastMasterTime = mMasterTime;
            mMasterTime = newMasterTime;
            mLastMasterCount = mMasterCount;
            mMasterCount = newMasterCount;
          }
        }
      }
    }

    if (0u != mMasterTime)
    {
      double eventDuration = 0.0;
      if (0u == mLastMasterTime)
      {
        // first cycle after reset. use rateRatio instead
        AVB_ASSERT(0.0 != mBaseFrequency);
        eventDuration = 1e9 * pClockDomain->getRateRatio() / double(mBaseFrequency);
      }
      else
      {
        AVB_ASSERT(mMasterCount - mLastMasterCount);
        eventDuration = double(mMasterTime - mLastMasterTime) / double(mMasterCount - mLastMasterCount);
      }

      for (uint32_t i = 0u; i < mTimeStampsPerPdu; i++)
      {
        mRefPlaneEventTime = mMasterTime + uint64_t(int64_t(eventDuration * double(int64_t(mRefPlaneEventCount - (mMasterCount + mRefPlaneEventOffset)))));
        mRefPlaneEventCount += mTimeStampInterval;
        crfStampBase64[i] = cond_swap64(mRefPlaneEventTime + getPresentationTimeOffset());

        if (0 == i) // use timestamp as launchtime for the packet
        {
          mPacketLaunchTime = mRefPlaneEventTime;
        }
      }


#if HURGHBLURB
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX,
          "mT", mMasterTime,
          "mC", mMasterCount,
          "rT", mRefPlaneEventTime,
          "rC", mRefPlaneEventCount,
          "l", mPacketLaunchTime
          "r", eventDuration
          );

#endif
    }


#if DEBUG_LAUNCHTIME
    (void) memcpy(avtpBase8 + mCrfHeaderSize + mPayloadHeaderSize + mTimeStampsPerPdu * cCrfTimeStampSize, &mPacketLaunchTime, 8);
    packet->len += 8;
#endif

    bool clockValid = (IasAvbClockDomain::eIasAvbLockStateLocked == pClockDomain->getLockState());

    if (!mClockValid && clockValid)
    {
      // just acquired lock on clock domain, toggle mr bit
      // ensure that a toggle does not occur more often then every 8 PDUs
      mMediaClockRestartToggle = (mMediaClockRestartToggle ^ 4u) & 0xF7;
    }
    mClockValid = clockValid;
    avtpBase8[1] = uint8_t((avtpBase8[1] & 0xF7) | mMediaClockRestartToggle);

    packet->attime = mPacketLaunchTime;
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "launch time", mPacketLaunchTime);
  }

  mLock.unlock();
  return result;
}


void IasAvbClockReferenceStream::readFromAvbPacket(const void* const packet, const size_t length)
{
  mLock.lock();

  if (isInitialized() && isReceiveStream())
  {
    IasAvbStreamState newState = IasAvbStreamState::eIasAvbStreamInvalidData;
    bool backwardCompatibility = false;
    uint16_t type = 0u;
    uint32_t baseFreq = 0u;
    uint16_t eventsPerStamp = 0u;
    const uint8_t* const avtpBase8 = static_cast<const uint8_t*>(packet);
    const uint16_t* const avtpBase16 = static_cast<const uint16_t*>(packet);
    const uint32_t* const avtpBase32 = static_cast<const uint32_t*>(packet);

    const IasAvbStreamState oldState = mStreamStateInternal;
    size_t payloadLength = 0u;

    uint32_t validationStage = 0u;

    if (NULL == packet)
    {
      newState = IasAvbStreamState::eIasAvbStreamNoData;
      // trigger clock domain reset
      mRefPlaneEventTime = 0u;
    }
    else
    {
      // @@DIAG inc FRAMES_RX
      mDiag.setFramesRx(mDiag.getFramesRx()+1);

      if (cValidateNever == mValidationMode)
      {
        // just assume a healthy packet (should only be used under lab/debugging conditions)
        payloadLength = ntohs(avtpBase16[mPayloadLenOffset16]) - mPayloadHeaderSize;
        newState = IasAvbStreamState::eIasAvbStreamValid;
      }
      else
      {
        if ((cValidateAlways == mValidationMode) || (IasAvbStreamState::eIasAvbStreamValid != getStreamState()))
        {
          // validate length
          if (length >= (mCrfHeaderSize + mPayloadHeaderSize))
          {
            validationStage++;
            if (eIasAvbCompD6 == mCompatibilityMode)
            {
              const uint16_t* pType = reinterpret_cast<const uint16_t*>(avtpBase8 + mTypeoffset8);
              backwardCompatibility = true;
              type = ntohs(*pType);
              baseFreq = decodeNominalFreq(avtpBase8[24]);
              eventsPerStamp = ntohs(avtpBase16[13]);
            }
            else
            {
              type = avtpBase8[mTypeoffset8];
              baseFreq = ntohl(avtpBase32[3]) & 0x1FFFFFFFu;
              eventsPerStamp = ntohs(avtpBase16[9]);
            }

            // validate AVTP subtype
            if (mSubtype == avtpBase8[0])
            {
              validationStage++;
              // validate clock reference stream type
              if (uint8_t(mType) == type)
              {
                validationStage++;
                // validate sv, version and tu fields
                // Note: we don't support "timestamp uncertain" mode at this time
                if ((avtpBase8[1] & 0xF1u) == 0x80u)
                {
                  validationStage++;
                  // validate base frequency
                  if (0u != baseFreq)
                  {
                    validationStage++;
                    // validate pull field - ignore for backward compatiblity mode
                    // Note: we don't support any other pull mode than "flat" right now
                    if (backwardCompatibility || (((avtpBase8[12] & 0xE0u) >> 5) == uint8_t(IasAvbClockMultiplier::eIasAvbCrsMultFlat)))
                    {
                      validationStage++;
                      // validate stream data length
                      payloadLength = ntohs(avtpBase16[mPayloadLenOffset16]) - mPayloadHeaderSize;

                      // ignore potential padding, just make sure packet is long enough
                      // and contains at least one CRF Timestamp and all stamps are complete
                      if (((length - (mCrfHeaderSize + mPayloadHeaderSize)) >= payloadLength)
                          && (payloadLength > 0u)
                          && (0u == (payloadLength % cCrfTimeStampSize))
                          )
                      {
                        newState = IasAvbStreamState::eIasAvbStreamValid;
                      }
                    }
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
          payloadLength = ntohs(avtpBase16[mPayloadLenOffset16]) - mPayloadHeaderSize;
          if (eIasAvbCompD6 == mCompatibilityMode)
          {
            baseFreq = decodeNominalFreq(avtpBase8[24]);
            eventsPerStamp = ntohs(avtpBase16[13]);
          }
          else
          {
            baseFreq = ntohl(avtpBase32[3]) & 0x1FFFFFFFu;
            eventsPerStamp = ntohs(avtpBase16[9]);
          }

          // always validate base frequency, since a 0 value could crash us
          if ((0u != baseFreq)
               && (avtpBase8[2] == uint8_t(mSeqNum + 1u))
             )
          {
            newState = IasAvbStreamState::eIasAvbStreamValid;
          }
          else
          {
            if (0 == baseFreq)
            {
              // @@DIAG inc UNSUPPORTED FORMAT
              mDiag.setUnsupportedFormat(mDiag.getUnsupportedFormat()+1);
            }
            else
            {
              // @DIAG inc SEQ_NUM_MISMATCH
              mDiag.setSeqNumMismatch(mDiag.getSeqNumMismatch()+1);
              DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX,
                "1722 ANNEX E - SEQ_NUM_MISMATCH (", mDiag.getSeqNumMismatch(), ")");
            }

            newState = IasAvbStreamState::eIasAvbStreamInvalidData;
          }
        }
      }

      mSeqNum = avtpBase8[2];
    }

    if ((IasAvbStreamState::eIasAvbStreamValid == newState) && (mValidationCount > 0u))
    {
      mValidationCount--;
      // defer setting stream state to "valid" for a given number of subsequent valid packets
      if (0u == mValidationCount)
      {
        setStreamState(newState);
      }
    }

    if (newState != oldState)
    {
      mStreamStateInternal = newState;
      if (IasAvbStreamState::eIasAvbStreamValid == newState)
      {
        // trigger clock domain reset
        mRefPlaneEventTime = 0u;
      }
      else
      {
        uint64_t id = getStreamId();
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "stream data invalid, stage:", validationStage,
            "stream:", id);

        mValidationCount = mValidationThreshold;
        setStreamState(newState);
      }
    }

    // properly validated packets always carry at least one time stamp
    uint16_t numStamps = uint16_t(payloadLength / cCrfTimeStampSize);
    if ((IasAvbStreamState::eIasAvbStreamValid == newState) && (numStamps > 0u))
    {
      IasAvbClockDomain* const clockDomain = getClockDomain();
      if ((NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx))
      {
        /* we could add sanity checks/diagnostics for numStamps not exceeding mTimeStampsPerPdu
         * and other params being constant from packet to packet. However, we can process even
         * packets that don't fulfill these conditions, so no real pressure here.
         */

        IasAvbRxStreamClockDomain* const rxClockDomain = static_cast<IasAvbRxStreamClockDomain*>(clockDomain);

        AVB_ASSERT(0u != baseFreq); // ensured by validation above

        const uint8_t mrField = (avtpBase8[1] & 0x40u);

        mTimeStampsPerPdu  = numStamps;
        mBaseFrequency     = baseFreq;
        mTimeStampInterval = eventsPerStamp;
        switch (avtpBase8[12] & 0xE0u)
        {
          case IasAvbClockMultiplier::eIasAvbCrsMultFlat:
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
            break;
          case IasAvbClockMultiplier::eIasAvbCrsMultMinus1000ppm:
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultMinus1000ppm;
            break;
          case IasAvbClockMultiplier::eIasAvbCrsMultPlus1000ppm:
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultPlus1000ppm;
            break;
          case IasAvbClockMultiplier::eIasAvbCrsMultTvToMovie:
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultTvToMovie;
            break;
          case IasAvbClockMultiplier::eIasAvbCrsMultMovieToTv:
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultMovieToTv;
            break;
          case IasAvbClockMultiplier::eIasAvbCrsMultOneEighth:
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultOneEighth;
            break;
          default:
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "invalid IasAvbClockMultiplier");
            mPull = IasAvbClockMultiplier::eIasAvbCrsMultFlat;
            break;
        }
        switch (avtpBase8[3])
        {
          case IasAvbClockReferenceStreamType::eIasAvbCrsTypeUser:
            mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeUser;
            break;
          case IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio:
            mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio;
            break;
          case IasAvbClockReferenceStreamType::eIasAvbCrsTypeVideoFrame:
            mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeVideoFrame;
            break;
          case IasAvbClockReferenceStreamType::eIasAvbCrsTypeVideoLine:
            mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeVideoLine;
            break;
          case IasAvbClockReferenceStreamType::eIasAvbCrsTypeMachineCycle:
            mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeMachineCycle;
            break;
          default:
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "invalid IasAvbClockReferenceStreamType");
            mType = IasAvbClockReferenceStreamType::eIasAvbCrsTypeUser;
            break;
        }

        const uint64_t* const stampBase = reinterpret_cast<const uint64_t*>(avtpBase8 + mCrfHeaderSize + mPayloadHeaderSize);
        uint64_t timestamp = cond_swap64(stampBase[0]);

        uint32_t i = 0u;

        if ((mrField != mMediaClockRestartToggle)
            || (rxClockDomain->getResetRequest())
            || (0u == mRefPlaneEventTime)
            )
        {
          if (mrField != mMediaClockRestartToggle)
          {
            mMediaClockRestartToggle = mrField;
          }

          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Resetting rxClockDomain, timestamp=",
              timestamp);
          rxClockDomain->reset(getTSpec().getClass(), timestamp, baseFreq);
          mClockValid = false;
          mRefPlaneEventTime = timestamp;
          mRefPlaneEventCount = 0u;
          // start update loop with second time stamp
          i = 1;
        }

        /* mRefPlaneEventCount is set to 0 every time the clock domain is updated.
         * It accumulates events that do not lead to an immediate clock domain update.
         * This way, we can skip time stamps if they come in too frequently.
         */
        if (mClockValid && (i < numStamps))
        {
          // skip to the most recent time stamp, save processing effort
          mRefPlaneEventCount += ((numStamps - i) - 1u) * eventsPerStamp;
          i = numStamps - 1u;
        }

        for (; i < numStamps; i++)
        {
          uint64_t timestamp = cond_swap64(stampBase[i]);
          mRefPlaneEventCount += eventsPerStamp;
          if (!mClockValid || ((timestamp - mRefPlaneEventTime) > mHoldoffTime))
          {
            rxClockDomain->update(mRefPlaneEventCount, timestamp,
                uint32_t(((uint64_t(1e9) * mRefPlaneEventCount) + (baseFreq / 2u)) / baseFreq),
                timestamp - mRefPlaneEventTime);
            mRefPlaneEventCount = 0u;
            mRefPlaneEventTime = timestamp;
          }
        }

        mClockValid = (IasAvbClockDomain::eIasAvbLockStateLocked == rxClockDomain->getLockState());
      }
    }
  }

  mLock.unlock();
}

void IasAvbClockReferenceStream::initFormat()
{
  mCompatibilityMode = getCompatibilityMode();

  if (eIasAvbCompD6 == mCompatibilityMode){
    mCrfHeaderSize = 24;
    mPayloadHeaderSize = 4;
    mSubtype = 5;
    mPayloadOffset32 = 7;
    mPayloadLenOffset16 = 10;
    mTypeoffset8 = 11*2;
  }
  else {
    mCrfHeaderSize = 20;
    mPayloadHeaderSize = 0;
    mSubtype = 4;
    mPayloadOffset32 = 5;
    mPayloadLenOffset16 = 8;
    mTypeoffset8 = 3;
  }
}

uint32_t IasAvbClockReferenceStream::decodeNominalFreq(uint8_t nominalField)
{
  uint32_t nominalFreq = 0;
  switch(nominalField)
  {
    case 1:
      nominalFreq = 8000;
      break;
    case 2:
      nominalFreq = 16000;
      break;
    case 3:
      nominalFreq = 32000;
      break;
    case 4:
      nominalFreq = 44100;
      break;
    case 5:
      nominalFreq = 88200;
      break;
    case 6:
      nominalFreq = 176400;
      break;
    case 7:
      nominalFreq = 48000;
      break;
    case 8:
      nominalFreq = 96000;
      break;
    case 9:
      nominalFreq = 192000;
      break;
  }

  return nominalFreq;
}

} // namespace IasMediaTransportAvb
