/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbAudioStream.cpp
 * @brief   This is the implementation of the IasAvbAudioStream class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbAudioStream.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_helper/ias_safe.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <math.h>
#include <iomanip>
// TO BE REPLACED #include "core_libraries/btm/ias_dlt_btm.h"
#include <dlt/dlt_cpp_extension.hpp>

#ifdef __SSE__
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

using std::ofstream;


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbAudioStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

#if defined(DEBUG_LISTENER_UNCERTAINTY)
static uint64_t gDebugRxDelayWorst = 0u;
#endif

uint32_t IasAvbAudioStream::sampleRateTable[] =
{
  0u,
  8000u,
  16000u,
  32000u,
  44100u,
  48000u,
  88200u,
  96000u,
  176400u,
  192000u,
  24000u
};

uint8_t IasAvbAudioStream::getSampleFrequencyCode(const uint32_t sampleFrequency)
{
  uint8_t code = 0u;

  std::string comp;
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cCompatibilityAudio, comp))
  {
    if (comp == "SAF")
    {
      if (48000u == sampleFrequency)
      {
        code = 9;
      }
    }
    else if (comp == "d6_1722a")
    {
      if (48000u == sampleFrequency)
      {
        code = 5;
      }
    }
    else
    {
      // unknown compatibility, use standard
      comp.clear();
    }
  }

  if (comp.empty())
  {
    for (code = static_cast<uint8_t>((sizeof(sampleRateTable)/sizeof(sampleRateTable[0])) - 1u); code > 0u; code--)
    {
      if (sampleFrequency == sampleRateTable[code])
      {
        break;
      }
    }
  }

  return code;
}

/*
 *  Constructor.
 */
IasAvbAudioStream::IasAvbAudioStream()
  : IasAvbStream(IasAvbStreamHandlerEnvironment::getDltContext("_AAS"), eIasAvbAudioStream)
  , mCompatibilityModeAudio(eIasAvbCompLatest)
  , mAudioFormat(IasAvbAudioFormat::eIasAvbAudioFormatSaf16)
  , mAudioFormatCode(0u)
  , mMaxNumChannels(0u)
  , mLocalStream(NULL)
  , mSampleFrequency(0u)
  , mSampleFrequencyCode(0u)
  , mRefPlaneSampleCount(0u)
  , mRefPlaneSampleOffset(0)
  , mRefPlaneSampleTime(0u)
  , mMasterCount(0u)
  , mLastMasterCount(0u)
  , mMasterTime(0u)
  , mLastMasterTime(0u)
  , mDummySamplesSent(0u)
  , mDumpCount(0u)
  , mPacketLaunchTime(0u)
  , mLock()
  , mSamplesPerChannelPerPacket(0u)
  , mStride(0u)
  , mSeqNum(0u)
  , mTempBuffer(NULL)
  , mConversionGain(AudioData(0x7FFF))
  , mUseSaturation(true)
  , mSampleIntervalNs(0.0)
  , mWaitForData(false)
  , mRatioBendRate(0.0)
  , mRatioBendLimit(0)
  , mAccumulatedFillLevel(0)
  , mFillLevelIndex(0)
  , mFillLevelFifo(NULL)
  , mValidationMode(cValidateOnce)
  , mDebugLogCount(0u)
  , mNumSkippedPackets(0u)
  , mNumPacketsToSkip(0u)
  , mDebugIn(true)
  , mValidationThreshold(0u)
  , mValidationCount(0u)
  , mExcessSamples(0u)
  , mDebugFile()
  , mLocalStreamReadSampleCount(0u)
  , mLocalStreamSampleOffset(0u)
  , mLastRefPlaneSampleTime(0u)
  , mFirstRun(true)
  , mBTMEnable(false)
  , mMasterTimeUpdateMinInterval(0u)
{
  // do nothing
}


/*
 *  Destructor.
 */
IasAvbAudioStream::~IasAvbAudioStream()
{
  cleanup(); // calls derivedCleanup()
}


void IasAvbAudioStream::derivedCleanup()
{
  // disconnect
  if (isConnected())
  {
    (void) connectTo(NULL);
  }

  // revert to default values
  mMaxNumChannels = 0u;
  mSampleFrequency = 48000u;
  mSampleFrequencyCode = getSampleFrequencyCode(mSampleFrequency);
  mAudioFormat = IasAvbAudioFormat::eIasAvbAudioFormatSaf16;
  mAudioFormatCode = getFormatCode(mAudioFormat);
  mCompatibilityModeAudio = eIasAvbCompLatest;

  delete[] mTempBuffer;
  mTempBuffer = NULL;

  delete[] mFillLevelFifo;
  mFillLevelFifo = NULL;

  mDebugFile.close();
}


IasAvbCompatibility IasAvbAudioStream::getCompatibilityModeAudio()
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


IasAvbProcessingResult IasAvbAudioStream::initTransmit(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
    IasAvbAudioFormat format, const IasAvbStreamId & streamId, uint32_t poolSize, IasAvbClockDomain * clockDomain,
    const IasAvbMacAddress & dmac, bool preconfigured)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBootTimeMeasurement, mBTMEnable);

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    if ((0u == maxNumberChannels)
        || (0u == sampleFreq)
        || (NULL == clockDomain)
       )
    {
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      if ( !(48000u == sampleFreq || 24000 == sampleFreq) ||
            (IasAvbAudioFormat::eIasAvbAudioFormatSaf16 != format)
         )
      {
          result = eIasAvbProcUnsupportedFormat;
      }
    }

    if (eIasAvbProcOK == result)
    {
      // update compatibility mode
      mCompatibilityModeAudio = getCompatibilityModeAudio();

      const uint32_t packetsPerSec = IasAvbTSpec::getPacketsPerSecondByClass(srClass);

      if (0u == packetsPerSec)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Illegal SR class configuration (packetsPerSec = 0, class code =",
            uint32_t(srClass));
        result = eIasAvbProcInvalidParam;
      }
      else
      {
        mSamplesPerChannelPerPacket = uint16_t((sampleFreq + (packetsPerSec - 1u)) / packetsPerSec);
#if DEBUG_LAUNCHTIME
        IasAvbTSpec tSpec(getPacketSize(format, uint16_t(maxNumberChannels * mSamplesPerChannelPerPacket)) + 8,
            IasAvbTSpec::eIasAvbClassA);
#else
        IasAvbTSpec tSpec(getPacketSize(format, uint16_t(maxNumberChannels * mSamplesPerChannelPerPacket)),
            srClass);
#endif

        result = IasAvbStream::initTransmit(tSpec, streamId, poolSize, clockDomain, dmac, tSpec.getVlanId(), preconfigured);
      }

      if (eIasAvbProcOK == result)
      {
        mTempBuffer = new (nothrow) AudioData[mSamplesPerChannelPerPacket];

        if (NULL == mTempBuffer)
        {
          /**
           * @log Not enough memory to allocate AudioData[mSamplesPerChannelPerPacket]
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
                  "Not enough memory to allocate AudioData[mSamplesPerChannelPerPacket]! mSamplesPerChannelPerPacket=",
                  mSamplesPerChannelPerPacket);
          result = eIasAvbProcNotEnoughMemory;
        }
      }
    }

    if (eIasAvbProcOK == result)
    {
      mCompatibilityModeAudio = getCompatibilityModeAudio();
      uint64_t bend = 0u;
      if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBendCtrlStream, bend))
      {
        uint64_t streamId = uint64_t(getStreamId());
        if (streamId == bend)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Using stream", streamId,
              "as clock bend reference");

          std::string fname;
          if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cDebugBufFName, fname))
          {
            mDebugFile.open(fname.c_str());
          }
          bend = 200; // do not exceed 999, or the controller will become unstable!
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioBendRate, bend);
          mRatioBendRate = double(bend) * 1e-3;
          bend = 20; // maximum bend in ppm
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioMaxBend, bend);
          mRatioBendLimit = int32_t(bend);

          mFillLevelFifo = new (nothrow) int32_t[cFillLevelFifoSize];
          if (NULL == mFillLevelFifo)
          {
            /**
             * @log Not enough memory to allocate Int32[fillLevel]
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to allocate fill level FIFO");
            result = eIasAvbProcNotEnoughMemory;
          }
        }
      }
    }

    if (eIasAvbProcOK == result)
    {
      uint32_t val = 0x7FFFu; // format specific
      if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioFloatGain, val))
      {
        mConversionGain = AudioData(int32_t(val));
      }
      if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioSaturate, val))
      {
        mUseSaturation = (val != 0u);
      }
      mMasterTimeout = 2000000000; // default is 2 seconds
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioClockTimeout, mMasterTimeout);

      mMaxNumChannels = maxNumberChannels;
      mSampleFrequency = sampleFreq;
      mSampleFrequencyCode = getSampleFrequencyCode(sampleFreq);
      mAudioFormat = format;
      mAudioFormatCode = getFormatCode(mAudioFormat);
      mSampleIntervalNs = 1.0e9 / double(sampleFreq);

      const uint32_t ptOffsetOrig = getPresentationTimeOffset();
      const uint32_t steps = adjustPresentationTimeOffset((uint32_t)mSampleIntervalNs);

      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "adjusted presentation time offset:",
                  "new =", getPresentationTimeOffset(), "orig =", ptOffsetOrig,
                  "stepWidth =", uint32_t(mSampleIntervalNs), "steps =", steps);

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


IasAvbProcessingResult IasAvbAudioStream::initReceive(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
    IasAvbAudioFormat format, const IasAvbStreamId & streamId, const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
    if ((0u == maxNumberChannels)
        || (0u == sampleFreq)
       )
    {
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      if ( !(48000u == sampleFreq || 24000 == sampleFreq) ||
            (IasAvbAudioFormat::eIasAvbAudioFormatSaf16 != format)
         )
      {
        result = eIasAvbProcUnsupportedFormat;
      }
    }

    const uint32_t packetsPerSec = IasAvbTSpec::getPacketsPerSecondByClass(srClass);
    if (0u == packetsPerSec)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Illegal SR class configuration (packetsPerSec = 0, class code =",
          uint32_t(srClass));
      result = eIasAvbProcInvalidParam;
    }

    if (eIasAvbProcOK == result)
    {
      mSamplesPerChannelPerPacket = uint16_t((sampleFreq + (packetsPerSec - 1u)) / packetsPerSec);
      IasAvbTSpec tSpec(getPacketSize(format, uint16_t(maxNumberChannels * mSamplesPerChannelPerPacket)), srClass);

      result = IasAvbStream::initReceive(tSpec, streamId, dmac, vid, preconfigured);

      if (eIasAvbProcOK == result)
      {
        mExcessSamples = 1u;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxExcessPayload, mExcessSamples);

        mTempBuffer = new (nothrow) AudioData[mSamplesPerChannelPerPacket + mExcessSamples];

        if (NULL == mTempBuffer)
        {
          result = eIasAvbProcNotEnoughMemory;
        }
      }
    }

    if (eIasAvbProcOK == result)
    {
      mCompatibilityModeAudio = getCompatibilityModeAudio();
      mMaxNumChannels = maxNumberChannels;
      mSampleFrequency = sampleFreq;
      mSampleFrequencyCode = getSampleFrequencyCode(sampleFreq);
      mAudioFormat = format;
      mAudioFormatCode = getFormatCode(mAudioFormat);
      mSampleIntervalNs = 1.0e9 / double(sampleFreq);
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxValidationMode, mValidationMode);
      mValidationThreshold = 100u;
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxValidationThreshold, mValidationThreshold);
      mValidationCount = mValidationThreshold;
      mWaitForData = true;
      mNumSkippedPackets = 1u;
      uint32_t skipTime = 10000u; // us
      if (!IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime))
      {
        /**
         * @log There is no config entry for Rx Clock Update Interval.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "no RX clock update interval configured! Set to",
            skipTime, "us");
      }
      mNumPacketsToSkip = (skipTime * packetsPerSec) / 1000000u;
    }

    if (eIasAvbProcOK != result)
    {
      cleanup();
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error=", int32_t(result));
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbAudioStream::prepareAllPackets()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(isInitialized());
  AVB_ASSERT(isTransmitStream());

  IasAvbPacket * referencePacket = getPacketPool().getPacket();
  const IasAvbMacAddress * sourceMac = IasAvbStreamHandlerEnvironment::getSourceMac();

  if (NULL == referencePacket)
  {
    /**
     * @log Init failed: Requested variable referencePacket == NULL
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to get a reference packet from the pool!");
    result = eIasAvbProcInitializationFailed;
  }
  else if (NULL == sourceMac)
  {
    /**
     * @log Init failed: Requested variable sourceMac == NULL
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "No source MAC address!");
    result = eIasAvbProcInitializationFailed;
  }
  else
  {
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
    *(packetData++) = (mAudioFormat == IasAvbAudioFormat::eIasAvbAudioFormatIec61883) ? 0x00 : 0x02; // cd = 0, sub type = 0 or 2
    *(packetData++) = 0x81u; // SV_VER_MR_RS_GV_TV
    packetData++;           // sequence number, filled in per packet or at connect
    *(packetData++) = 0x00u; // reserved|TU

    // streamId
    getStreamId().copyStreamIdToBuffer(packetData, 8u);
    packetData += 8;

    packetData += 4;        // time stamp, filled in per packet

    *(packetData++) = mAudioFormatCode;

    if (eIasAvbCompLatest == mCompatibilityModeAudio)
    {
      packetData++;           // nsr + rsv + part of ch per frame, filled in per packet
      // ToDo: Check if reserved is set to 0 if not, set here
      packetData++;           // rest of ch per frame, filled in per packet
    }
    else
    {
      packetData++;           // channel layout, filled in per packet
      *(packetData++) = mSampleFrequencyCode;
    }
    *(packetData++) = 16u;   // number of valid MSBs in each sample (BitDepth)

    packetData += 2; // skip stream_data_length

    // Set the sparse time stamp bit, set reserved field and evt field to a constant zero
    // the rest of Packet_info will be filled in per packet
    bool isSparse = false;
    if (eIasAvbCompLatest == mCompatibilityModeAudio)
    {
      if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioSparseTS, isSparse) && isSparse)
      {
        *(packetData++) = 0x10; // rsv|sp=1|evt
      }
      else
      {
        *(packetData++) = 0x00; // rsv|sp=0|evt
      }
    }
    else // Draft 6
    {
      *(packetData++) = 0x00; // M3,M2,M1,M0,evt, first part of channels_per_frame
    }
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Sparse Time Stamping is",
            (isSparse ? "ENABLED" : "DISABLED"));

    if (eIasAvbCompLatest == mCompatibilityModeAudio)
    {
      *(packetData++) = 0x00u; // reserved filed
    }

    /*
     * We need to state the length here for the init mechanism. Note that the actual packets
     * that will be sent out later might vary in length, so the packet->len and stream_data_length
     * fields should be set every time a packet is generated.
     */
    referencePacket->len = uint32_t(packetData - packetStart);

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


void IasAvbAudioStream::activationChanged()
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
    mWaitForData = true;
    mRefPlaneSampleTime = 0u;
    mRefPlaneSampleCount = 0u;
    mRefPlaneSampleOffset = 0u;
    mDummySamplesSent = 0u;
    mDumpCount = 0u;
    mAccumulatedFillLevel = 0;
    mFillLevelIndex = 0;
    if (NULL != mFillLevelFifo)
    {
      for (uint32_t i = 0u; i < cFillLevelFifoSize; i++)
      {
        mFillLevelFifo[i] = 0;
      }
    }
    mLocalStreamReadSampleCount = 0u;
    mLocalStreamSampleOffset = 0u;
  }

  if (isConnected())
  {
    AVB_ASSERT(NULL != mLocalStream);
    mLocalStream->setClientActive(isActive());
  }

  mLock.unlock();
}


bool IasAvbAudioStream::resetTime(uint64_t nextWindowStart)
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
    mMasterCount = mMasterCount * mSampleFrequency / eventRate;
  }

  if (0u == mMasterTime)
  {
    // no ref clock available
    mRefPlaneSampleCount = 0u;
    mRefPlaneSampleTime = 0u;
    mPacketLaunchTime = nextWindowStart;

    // we abuse the offset to memorize that the warning has already been shown
    if (0u == mRefPlaneSampleOffset)
    {
      mRefPlaneSampleOffset = 1u;
      /**
       * @log No reference clock available
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "no reference clock available! clockId =",
          clock->getClockDomainId());
    }
  }
  else
  {
    // store offset between own sample count and master count
    mRefPlaneSampleOffset = int64_t(mRefPlaneSampleCount - mMasterCount);

    uint32_t samplesToSkip = 0u;
    if (nextWindowStart > mMasterTime)
    {
      // calculate first point in time where samples will cross the ref plane during the next TX window
      samplesToSkip = uint32_t(ceil(double(nextWindowStart - mMasterTime) / mSampleIntervalNs));
    }
    else
    {
      // master time already enough in the future, skip 0 samples
    }

    if (mLastRefPlaneSampleTime > mMasterTime)
    {
      uint32_t samplesAlreadySent = 0u; // samples already sent before reset happened due to underrun

      // calculate first point in time where samples will cross the timestamp of the last sent packet
      samplesAlreadySent = uint32_t(ceil(double(mLastRefPlaneSampleTime - mMasterTime) / mSampleIntervalNs));

      if (samplesToSkip < samplesAlreadySent)
      {
        samplesToSkip = samplesAlreadySent;
      }
    }

    // adjust ref plane
    mRefPlaneSampleCount += samplesToSkip;
    mRefPlaneSampleTime = mMasterTime + uint64_t(double(samplesToSkip) * mSampleIntervalNs * clock->getRateRatio());

    /* adapt launch time
     * without syntonized mode, time needs to be converted from PTP time (ref plane) to I210 time
     * NOTE: right now, TX delay is applied in TX sequencer, so launch time can be equal ref plane time
     */
    mPacketLaunchTime = mRefPlaneSampleTime;

    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX,
        "clockId", clock->getClockDomainId(),
        "start", nextWindowStart
        );
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX,
        "mT", mMasterTime,
        "mC", mMasterCount,
        "rT", mRefPlaneSampleTime,
        "rC", mRefPlaneSampleCount,
        "o", mRefPlaneSampleOffset,
        "l", mPacketLaunchTime
		);

    ret = true;
  }

  mLastMasterCount = 0u;
  mLastMasterTime = 0u;
  mLastRefPlaneSampleTime = 0u;

  return ret;
}


bool IasAvbAudioStream::writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart)
{
  bool result = true;
  std::lock_guard<std::mutex> lock(mLock);

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
    uint32_t* const avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase8);
    uint16_t numChannels = 0u;
    uint16_t written = 0u;
    bool   isReadReady = false;

    if ((0u == mRefPlaneSampleCount) && (0u == mRefPlaneSampleTime))
    {
      // stream has just been activated
      if (!resetTime(nextWindowStart))
      {
        /* reset failed due to missing reference clock
         * prepare dummy packet instead. this will retrigger the sequencer to ask for another packet
         */

        packet->makeDummyPacket();
        packet->attime = mPacketLaunchTime + 10000000u; // wait 10ms before trying again

        return true;
      }
    }

    // time-stamping hard-coded for SAF
    // write time stamp to packet
    avtpBase32[3] = htonl(uint32_t(mRefPlaneSampleTime + getPresentationTimeOffset()));

    packet->attime = mPacketLaunchTime;

    // If sparse time stamping bit (sp) is set to true
    if (avtpBase8[22] & 0x10)
    {
        // Set time stamp valid bit (tv) to true every 8th packet
        if (!(mSeqNum % 8))
        {
            DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "Sparse Time Stamping valid on packet SeqNum",
                    mSeqNum);
            avtpBase8[1] |= 0x01;
        }
        // Otherwise set it to false
        else
        {
            avtpBase8[1] &= uint8_t(~0x01);
        }
    }

    // insert sequence number and advance for next packet
    avtpBase8[2] = mSeqNum;
    mSeqNum++;


    if (isConnected())
    {
      AVB_ASSERT(NULL != mLocalStream);
      uint16_t ch;

      numChannels = mLocalStream->getNumChannels();

      if (mLocalStream->hasSideChannel())
      {
        AVB_ASSERT(numChannels > 0u);
        numChannels--;
      }

      if (mDummySamplesSent > 0u)
      {
        AVB_ASSERT(mWaitForData);
        /* dispose of samples until we've compensated for the dummy data we've sent out
         * This is needed to get the fill level back into balance
         */

        uint16_t dump = uint16_t(mDummySamplesSent);
        mLocalStream->dumpFromLocalAudioBuffer(dump);
        mDummySamplesSent -= dump;

        if (dump > 0u)
        {
          mDumpCount++;

          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "dumped samples:",
              dump, "remaining:", mDummySamplesSent);

          if ((mDumpCount > 10u) || (mDummySamplesSent > 1000u))
          {
            // way too many dummy samples accrued, or dumping takes too many iterations
            // time to reset the stream: trigger the TX engine to do it by invalidating the launch time
            packet->attime = 0u;

            uint64_t id = getStreamId();
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "stream reset after too many dummy samples",
                mDummySamplesSent, mDumpCount,
                "stream:", id);

            /*
             * stream should take over the current reference plane sample time even after reset
             * as long as master clock is still reliable. otherwise stream may produce packets with
             * negative time-jump which results in a burst of transmit packets.
             */
            mLastRefPlaneSampleTime = mRefPlaneSampleTime;
          }
        }
      }

      isReadReady = mLocalStream->isReadReady();

      // prevent AvbAlsaWrk from accessing the local audio stream
      if (true == mLocalStream->hasBufferDesc())
      {
        mLocalStream->lock();

        if (isReadReady &&  // local buffer is ready for reading
              (0u == mLocalStreamReadSampleCount))  // about to start reading samples from local buffer
        {
          uint64_t timeStamp = mLocalStream->getCurrentTimestamp();
          /*
           * dispose of samples behind mRefPlaneSampleTime
           */
          while ((0u != timeStamp) && (timeStamp < mRefPlaneSampleTime))
          {
            mLocalStream->dumpFromLocalAudioBuffer(mSamplesPerChannelPerPacket);
            timeStamp = mLocalStream->getCurrentTimestamp();

            /*
             * If timeStamps in descriptors are too far away, all data samples will be discarded.
             * And getCurrentTimestamp() will return zero so that we can exit the loop.
             */
          }

          if (0u != timeStamp)
          {
            uint32_t samplesToSkip = 0u;
            uint64_t alignedTimestamp = 0u;

            IasAvbClockDomain * const clock = getClockDomain();
            AVB_ASSERT(NULL != clock);

            samplesToSkip = uint32_t(ceil(double(timeStamp - mRefPlaneSampleTime) / mSampleIntervalNs));

            // timestamp aligned in multiples of sample period
            alignedTimestamp = mRefPlaneSampleTime + uint64_t(double(samplesToSkip) * mSampleIntervalNs * clock->getRateRatio());

            // offset to be applied on each timestamp read from the descriptor buffer so that it can be multiples of sample period
            mLocalStreamSampleOffset = alignedTimestamp - timeStamp;
          }
        }
      }

      // hard-coded SAF16 routine
      // observation logic only active for first channel, assume all others behave synchronously
      for (ch = 0u; ch < numChannels; ch++)
      {
        uint8_t * pBase = avtpBase8 + 24 + getSampleSize(mAudioFormat) * ch;

        if (mDummySamplesSent > 0u)
        {
          written = 0u;
        }
        else
        {
          if (true == isReadReady)
          {
            uint64_t timeStamp = 0u;
            mLocalStream->readLocalAudioBuffer(ch, mTempBuffer, mSamplesPerChannelPerPacket, written, timeStamp);

            if ((0u == ch) && (0u != written) && (0u != timeStamp))
            {
              timeStamp += mLocalStreamSampleOffset;
              mLocalStreamReadSampleCount += written;
            }
          }
          else
          {
            written = 0u;
          }
        }

        if (0u == written)
        {
          if (0u == ch)
          {
            if (true == isReadReady)
            {
              if (!mWaitForData)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Underrun condition begins at",
                    mRefPlaneSampleCount, "samples, launch time=",
                    mPacketLaunchTime);
                mWaitForData = true;
              }
              mDummySamplesSent += mSamplesPerChannelPerPacket;
            }
            else // !isReadReady
            {
              /*
               * nop: this will not be the underrun case since local stream will accumulate samples
               * up to half-full of the ring buffer at the beginning in case of time-aware buffering
               */
            }
          }

          // create '0' samples to be sent
          written = mSamplesPerChannelPerPacket;
          for (uint32_t sample = 0u; sample < written; sample++)
          {
            mTempBuffer[sample] = 0.0;
          }
        }
        else
        {
          if ((0u == ch) && mWaitForData)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Underrun condition ended after",
                mRefPlaneSampleCount, "samples, launch time=",
                mPacketLaunchTime);
            mWaitForData = false;
            // no soft reset required anymore; either, the dummy payload is balanced out, or the stream is reset anyway
          }
        }

        // copy samples to packet and do format conversion
        if (mUseSaturation)
        {
          for (uint32_t sample = 0u; sample < written; sample++)
          {
            *reinterpret_cast<uint16_t*>(pBase) = htons(uint16_t(mTempBuffer[sample]));
            pBase += mStride;
          }
        }
        else
        {
          for (uint32_t sample = 0u; sample < written; sample++)
          {
            *reinterpret_cast<uint16_t*>(pBase) = htons(uint16_t(int16_t(mTempBuffer[sample])));
            pBase += mStride;
          }
        }
      }

      uint8_t layout = 0u;
      if (mLocalStream->hasSideChannel())
      {
        uint16_t samplesWritten = 0;
        uint64_t timeStamp = 0u;
        // side channel is always the last one
        mLocalStream->readLocalAudioBuffer(ch, mTempBuffer, mSamplesPerChannelPerPacket, samplesWritten, timeStamp);
        if (samplesWritten > 0u)
        {
          SideChannel temp;
          temp.pseudoAudio = mTempBuffer[0];
          layout = temp.raw[0];
        }
        else
        {
          // since there are no audio samples, channel layout can be set to '0'
          layout = 0u;
        }
      }
      else
      {
        // use the static information from the time the local stream was created
         layout = mLocalStream->getChannelLayout();
      }

      if (true == mLocalStream->hasBufferDesc())
      {
        mLocalStream->unlock();
      }

      if ((eIasAvbCompSaf== mCompatibilityModeAudio) || (eIasAvbCompD6 == mCompatibilityModeAudio))
      {
        avtpBase8[17] = layout;
      }
      else // eIasAvbCompLatest
      {
        // Pass through layout value, any translation must be done by the user application
        // Write only the lower 4 bits from the channel layout into the 'evt' Packet Info field
        avtpBase8[22] = uint8_t((avtpBase8[22] & 0xF0) | (layout & 0x0F));
      }
    }
    else
    {
      // stream not connected but active -> send NULL stream (i.e. stream with zero audio channels)

      // written still needs to be set for proper time stamp calculation
      written = mSamplesPerChannelPerPacket;
      numChannels = 0u;
    }

    // set channels_per_frame
    AVB_ASSERT(numChannels <= mMaxNumChannels);

    if (eIasAvbCompLatest == mCompatibilityModeAudio)
    {
      uint8_t *avtpData = &avtpBase8[17];
      *avtpData++ = static_cast<uint8_t>((mSampleFrequencyCode << 4) | (uint8_t) ((numChannels >> 8) & 0x0003u));
      *avtpData = (uint8_t)(numChannels & 0x00ffu);
    }
    else
    {
      *(avtpBase16 + 11) = htons(numChannels); // assumes M1 and M0 fields are always 0
    }

    // set packet length and stream_data_length
    const uint16_t streamDataLength = uint16_t(written * numChannels * getSampleSize(mAudioFormat));
    *(avtpBase16 + 10) = htons(streamDataLength);
    packet->len = streamDataLength + IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf16>::cHeaderSize +
        IasAvbTSpec::cIasAvbPerFrameOverhead;
#if DEBUG_LAUNCHTIME
    (void) memcpy(avtpBase8 + IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf16>::cHeaderSize + streamDataLength, &mPacketLaunchTime, 8);
    packet->len += 8;
#endif
    bool btmEnable = false;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBootTimeMeasurement, btmEnable);
    if (btmEnable)
    {
      mFirstRun = false;
      // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb prepared first audio packet",NULL);
    }
    IasAvbClockDomain * const pClockDomain = getClockDomain();
    AVB_ASSERT(NULL != pClockDomain);

    // set timestamp uncertain bit, depending on lock state
    avtpBase8[3] = (IasAvbClockDomain::eIasAvbLockStateLocked == pClockDomain->getLockState()) ? 0x00u : 0x01u;

    // advance ref pane for next packet
    AVB_ASSERT(0u != mMasterTime); // otherwise, we would have created a dummy packet above
    {
      uint64_t newMasterTime = 0u;
      uint64_t newMasterCount = pClockDomain->getEventCount(newMasterTime);
      newMasterCount = newMasterCount * mSampleFrequency / pClockDomain->getEventRate();

      if (newMasterTime != mMasterTime)
      {
        // this will also catch negative changes of master time
        if (((newMasterTime - mMasterTime) > mMasterTimeout)
            || !(newMasterCount > mMasterCount))
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "clock reference out of expected interval, resetting.");
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "old time:", mMasterTime,
              "new time:", newMasterTime,
              "old count:", mMasterCount,
              "new count:", newMasterCount
              );

          // this will trigger resetTime for the next packet
          mRefPlaneSampleTime = 0u;
          mRefPlaneSampleCount = 0u;

          // this will skip the time calculation below
          mMasterTime = 0u;
        }
        else
        {
          bool doUpdate = true;
          if ((0u != mMasterTimeUpdateMinInterval) &&
              ((newMasterTime - mMasterTime) < mMasterTimeUpdateMinInterval))
          {
            // skip master time update in short measurement interval to avoid accidental error of timestamp calculation
            doUpdate = false;
          }

          if (doUpdate)
          {
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
      double sampleDuration = 0.0;
      if (0u == mLastMasterTime)
      {
        // first cycle after reset. use rateRatio instead
        AVB_ASSERT(0.0 != mSampleFrequency);
        sampleDuration = 1e9 * pClockDomain->getRateRatio() / double(mSampleFrequency);
      }
      else
      {
        AVB_ASSERT(mMasterCount - mLastMasterCount);
        sampleDuration = double(mMasterTime - mLastMasterTime) / double(mMasterCount - mLastMasterCount);
      }

      mRefPlaneSampleCount += written;
      mRefPlaneSampleTime = mMasterTime + uint64_t(int64_t(sampleDuration * double(int64_t(mRefPlaneSampleCount - (mMasterCount + mRefPlaneSampleOffset)))));
      mPacketLaunchTime = mRefPlaneSampleTime;

#if HURGHBLURB
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX,
          "w", written,
          "mT", mMasterTime,
          "mC", mMasterCount,
          "rT", mRefPlaneSampleTime,
          "drT", mRefPlaneSampleTime - oldRef,
          "rC", mRefPlaneSampleCount,
          "l", mPacketLaunchTime,
          "r", sampleDuration
          );

#endif
    }
  }
  return result;
}

void IasAvbAudioStream::readFromAvbPacket(const void* const packet, const size_t length)
{
  mLock.lock();

  if (isInitialized() && isReceiveStream())
  {
    IasAvbStreamState newState = IasAvbStreamState::eIasAvbStreamInvalidData;

    const uint8_t* const avtpBase8 = static_cast<const uint8_t*>(packet);
    const uint16_t* const avtpBase16 = static_cast<const uint16_t*>(packet);
    const uint32_t* const avtpBase32 = static_cast<const uint32_t*>(packet);

    const IasAvbStreamState oldState = mStreamStateInternal;
    size_t payloadLength = 0u;

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
        payloadLength = ntohs(avtpBase16[10]);
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
            if (IasAvbAudioFormat::eIasAvbAudioFormatIec61883 == mAudioFormat)
            {
            }
            else
            {
              validationStage++;
              // validate AVTP subtype
              if (0x02 == avtpBase8[0])
              {
                validationStage++;
                // validate format
                if (avtpBase8[16] == mAudioFormatCode)
                {
                  uint8_t sampleFrequencyCode;
                  validationStage++;
                  // validate sample frequency
                  if (eIasAvbCompLatest == mCompatibilityModeAudio)
                  {
                    sampleFrequencyCode = (avtpBase8[17] >> 4) & 0x0Fu;
                  }
                  else
                  {
                    sampleFrequencyCode = avtpBase8[18] & 0x0Fu;
                  }

                  if (sampleFrequencyCode == mSampleFrequencyCode)
                  {
                    validationStage++;
                    // validate stream data length
                    payloadLength = ntohs(avtpBase16[10]);

                    // ignore potential padding, just make sure packet is long enough
                    if ((length - cAvtpHeaderSize) >= payloadLength)
                    {
                      validationStage++;
                      newState = IasAvbStreamState::eIasAvbStreamValid;
                    }
                  }
                }
                else
                {
                  // not the expected format
                  // @@DIAG inc UNSUPPORTED FORMAT
                  mDiag.setUnsupportedFormat(mDiag.getUnsupportedFormat()+1);
                }
              }
              else
              {
                // not AVTP
                // @@DIAG inc UNSUPPORTED FORMAT
                mDiag.setUnsupportedFormat(mDiag.getUnsupportedFormat()+1);
              }
            }
          }
        }

        if (IasAvbStreamState::eIasAvbStreamValid == oldState)
        {
          payloadLength = ntohs(avtpBase16[10]);
          if (avtpBase8[2] == uint8_t(mSeqNum + 1u))
          {
            newState = IasAvbStreamState::eIasAvbStreamValid;
          }
          else
          {
            // @DIAG inc SEQ_NUM_MISMATCH
            mDiag.setSeqNumMismatch(mDiag.getSeqNumMismatch()+1);
            DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX,
              "1722 ANNEX E - SEQ_NUM_MISMATCH (", mDiag.getSeqNumMismatch(),
              " Stream:", ntohl(avtpBase32[1]), " ", ntohl(avtpBase32[2]),
              " SeqNum:", int32_t(avtpBase8[2]),
              " Expected:", (mSeqNum + 1u),
              " TS:", ntohl(avtpBase32[3]));

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
        if (isConnected())
        {
          mLocalStream->setClientActive(true);
        }
      }
    }

    if (newState != oldState)
    {
      mStreamStateInternal = newState;
      if (IasAvbStreamState::eIasAvbStreamValid == newState)
      {
        // flag clock domain to reset
        mWaitForData = true;
      }
      else if (IasAvbStreamState::eIasAvbStreamValid == oldState)
      {
        // @@DIAG if new state is no data then inc STREAM_INTERRUPTED
        if (IasAvbStreamState::eIasAvbStreamNoData == newState)
        {
          mDiag.setStreamInterrupted(mDiag.getStreamInterrupted()+1);
        }

        uint64_t id = getStreamId();
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "stream data invalid, stage:", validationStage,
            "stream:", id);
        mValidationCount = mValidationThreshold;
        setStreamState(newState);
        if (isConnected())
        {
          mLocalStream->setClientActive(false);
        }
        IasAvbClockDomain* const clockDomain = getClockDomain();
        if ((NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx))
        {
          IasAvbRxStreamClockDomain* const rxClockDomain = static_cast<IasAvbRxStreamClockDomain*>(clockDomain);

          rxClockDomain->invalidate();
        }
      }
      else
      {
        // do nothing, local side already set to inactive
      }
    }

    if (IasAvbStreamState::eIasAvbStreamValid == newState)
    {
      uint16_t written = 0u;
      uint16_t channel;
      uint16_t numLocalChannels = 0u;
      bool sideChannel = false;
      uint16_t numChannels;

      const uint32_t timestamp = ntohl(avtpBase32[3]);

      if (eIasAvbCompLatest == mCompatibilityModeAudio)
      {
        numChannels = static_cast<uint16_t>((((uint16_t)avtpBase8[17]) & 0x0003u) | ((uint16_t)avtpBase8[18]));
      }
      else
      {
        numChannels = ntohs(avtpBase16[11]) & 0x03FFu;
      }

      if (isConnected())
      {
        AVB_ASSERT(NULL != mLocalStream);
        numLocalChannels = mLocalStream->getNumChannels();
        sideChannel = mLocalStream->hasSideChannel();

        if (sideChannel && (numLocalChannels > 0u))
        {
          numLocalChannels--;
        }
      }

      // calculate iteration helpers before we adjust the number of channels
      const uint16_t sampleSize = getSampleSize(mAudioFormat);
      const uint16_t stride = static_cast<uint16_t>(sampleSize * numChannels);
      uint16_t numSamplesPerChannel = 0u;
      if (0u != stride)
      {
          numSamplesPerChannel = static_cast<uint16_t>(payloadLength / stride);
      }
      // ignore excess samples above the limit we allow
      if (numSamplesPerChannel > (mSamplesPerChannelPerPacket + mExcessSamples))
      {
        numSamplesPerChannel = static_cast<uint16_t>(mSamplesPerChannelPerPacket + mExcessSamples);
      }

      // ignore excess audio channels
      // implies limit to mMaxNumChannels
      if (numChannels > numLocalChannels)
      {
        numChannels = numLocalChannels;
      }

      if (isConnected() && (numChannels > 0u))
      {
        AVB_ASSERT(NULL != mLocalStream);

        // prevent AvbAlsaWrk from accessing the local audio stream
        if (true == mLocalStream->hasBufferDesc())
        {
          mLocalStream->lock();

          if (IasAvbStreamState::eIasAvbStreamValid != oldState)
          {
            // reset buffers to flush old data samples and timestamp epoch
            for (channel = 0u; channel < mLocalStream->getNumChannels(); channel++)
            {
              IasLocalAudioBuffer *ringBuf = mLocalStream->getChannelBuffers()[channel];
              AVB_ASSERT(NULL != ringBuf);
              ringBuf->reset(0u);
            }
            IasLocalAudioBufferDesc *descQ = mLocalStream->getBufferDescQ();
            AVB_ASSERT(NULL != descQ);
            descQ->reset();
          }
        }

        for (channel = 0u; channel < numChannels; channel++)
        {
          AVB_ASSERT(IasAvbAudioFormat::eIasAvbAudioFormatSaf16 == mAudioFormat);

          const uint8_t* in = avtpBase8 + (24u + (sampleSize * channel));

          for (uint32_t sample = 0u; sample < numSamplesPerChannel; sample++)
          {
            // orig
            mTempBuffer[sample] = AudioData(int16_t(ntohs(*reinterpret_cast<const uint16_t*>(in))));
            in += stride;
          }

          mLocalStream->writeLocalAudioBuffer(channel, mTempBuffer, numSamplesPerChannel, written, timestamp);

#if defined(DEBUG_LISTENER_UNCERTAINTY)
          /* DO NOT ENABLE THESE LINES FOR PRODUCTION SW */
          if (0u == channel)
          {
            IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
            const uint64_t now = ptp->getLocalTime();

            uint64_t rxTstamp = 0u;
            const size_t rxTstampSz = sizeof(rxTstamp);

            uint64_t rxTstampBuf = uint64_t(((uint8_t*)packet + length + (rxTstampSz - 1u))) & ~(rxTstampSz - 1u);
            rxTstamp = *((uint64_t*)rxTstampBuf);
            if (rxTstamp <= now)
            {
              const uint64_t elapsed = now - rxTstamp;

              if (gDebugRxDelayWorst < elapsed)
              {
                gDebugRxDelayWorst = elapsed;

                DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Rx elapsed time from MAC to Local Audio Buffer (worst case) = ",
                    uint64_t(elapsed));
              }
            }
          }
#endif
        }

        // if there are local channels left, fill them with zero
        (void) memset(mTempBuffer, 0, (mSamplesPerChannelPerPacket + mExcessSamples) * sizeof (AudioData));

        for (; channel < numLocalChannels; channel++)
        {
          mLocalStream->writeLocalAudioBuffer(channel, mTempBuffer, numSamplesPerChannel, written, timestamp);
        }

        // fill side channel
        if (sideChannel)
        {
          SideChannel layout;
          if ((eIasAvbCompSaf == mCompatibilityModeAudio) || (eIasAvbCompD6 == mCompatibilityModeAudio))
          {
            layout.value = uint32_t(avtpBase8[17]);
          }
          else // eIasAvbCompLatest
          {
            // Read the 4 bit 'evt' value from the Packet Info field
            layout.value = uint32_t(avtpBase8[22] & 0x0F);
          }

          for (uint32_t sample = 0u; sample < numSamplesPerChannel; sample++)
          {
            mTempBuffer[sample] = layout.pseudoAudio;
          }

          mLocalStream->writeLocalAudioBuffer(channel, mTempBuffer, numSamplesPerChannel, written, timestamp);
        }

        if (true == mLocalStream->hasBufferDesc())
        {
          mLocalStream->unlock();
        }
      }

      // @@DIAG process media reset bit to inc MEDIA_RESET
      if (avtpBase8[1] & 0x08)
      {

        mDiag.setMediaReset(mDiag.getMediaReset()+1);
      }
      // @@DIAG process tv bit to inc TIMESTAMP_VALID / NOT VALID
      if (avtpBase8[1] & 0x01)
      {
        mDiag.setTimestampValid(mDiag.getTimestampValid()+1);
      }
      else
      {
        mDiag.setTimestampNotValid(mDiag.getTimestampNotValid()+1);
      }
      // @@DIAG process tu bit to inc TIMESTAMP UNCERTAIN
      if (avtpBase8[3] & 0x01)
      {
        mDiag.setTimestampUncertain(mDiag.getTimestampUncertain()+1);
      }

      IasAvbClockDomain* const clockDomain = getClockDomain();
      if ((NULL != clockDomain) && (clockDomain->getType() == eIasAvbClockDomainRx))
      {
        IasAvbRxStreamClockDomain* const rxClockDomain = static_cast<IasAvbRxStreamClockDomain*>(clockDomain);

        if (0u == numChannels)
        {
          // NULL stream, use fictitious number of samples that would fit into one packet interval
          // NOTE: only works reliably for non-fractional values (e.g. 48000 samples/8000 Packets=6 samples)
          numSamplesPerChannel = uint16_t((1.0e9 / double(getTSpec().getPacketsPerSecond()) / mSampleIntervalNs) + 0.5);
        }

        // check time stamp valid flag
        /* Note: The current 1722 draft (1722a-D4) mandates all AVTP audio packets to carry a valid time stamp,
         * which in turn corresponds to the first sample in the packet. However, if they change their minds, or
         * if we are listening to a non-compliant stream, we still try to deal with it.
         */

        if ((avtpBase8[1] & 0x01) && mNumSkippedPackets >= mNumPacketsToSkip)
        {
          uint32_t deltaMediaClock = static_cast<uint32_t>(double(mRefPlaneSampleCount) * mSampleIntervalNs);

          // skip first time we receive a time stamp after creation of the stream
          // Also, check for reset request. Someone might detected that a reset is needed (e.g. PTP epoch change)
          if ((mWaitForData) || (rxClockDomain->getResetRequest()))
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Resetting rxClockDomain, timestamp=",
                timestamp);

            mWaitForData = false;
            rxClockDomain->reset(getTSpec().getClass(), timestamp, mSampleFrequency);
          }
          else
          {
            rxClockDomain->update(mRefPlaneSampleCount, timestamp, deltaMediaClock, timestamp - uint32_t(mRefPlaneSampleTime));
          }

          // @@DIAG Check with clock domain if MEDIA_LOCKED or UNLOCKED
          IasAvbClockDomain::IasAvbLockState lockState = rxClockDomain->getLockState();
          if (mCurrentAvbLockState != lockState)
          {
            mCurrentAvbLockState = lockState;
            if (IasAvbClockDomain::eIasAvbLockStateUnlocked == mCurrentAvbLockState)
            {
              mDiag.setMediaUnlocked(mDiag.getMediaUnlocked()+1);
            }
            else if (IasAvbClockDomain::eIasAvbLockStateLocked == mCurrentAvbLockState)
            {
              mDiag.setMediaLocked(mDiag.getMediaLocked()+1);
            }
          }

          mRefPlaneSampleTime = timestamp;
          mRefPlaneSampleCount = 0;
          mNumSkippedPackets = 1u;
        }
        else
        {
          mNumSkippedPackets++;
        }

        /* we use the ref sample count variable to memorize how many samples have passed since
         * the last time stamp and the ref sample time variable to memorize the last time stamp
         */

        mRefPlaneSampleCount += numSamplesPerChannel;
      }
    }
  }

  mLock.unlock();
}


IasAvbProcessingResult IasAvbAudioStream::connectTo(IasLocalAudioStream* localStream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

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
        mStride      = 0u;
      }

      if (NULL != localStream)
      {
        uint16_t numChannels = localStream->getNumChannels();
        if (localStream->hasSideChannel() && (numChannels > 0u))
        {
          numChannels--;
        }

        if ((0u == numChannels)
            || (numChannels > mMaxNumChannels)
            || (mSampleFrequency != localStream->getSampleFrequency())
            || (getDirection() != localStream->getDirection())
           )
        {
          result = eIasAvbProcInvalidParam;
        }

        if (eIasAvbProcOK == result)
        {
          result = localStream->connect(this);
        }

        if (eIasAvbProcOK == result)
        {
          localStream->setClientActive(isTransmitStream() && isActive());
          mLocalStream = localStream;
          mStride = uint16_t(numChannels * getSampleSize(mAudioFormat));
          mRefPlaneSampleCount  = 0u;
          mDummySamplesSent     = 0u;
          mDumpCount            = 0u;
          mRefPlaneSampleTime   = 0u;
          mAccumulatedFillLevel = 0;
          mFillLevelIndex       = 0;
          if (NULL != mFillLevelFifo)
          {
            for (uint32_t i = 0u; i < cFillLevelFifoSize; i++)
            {
              mFillLevelFifo[i] = 0;
            }
          }
          mLocalStreamReadSampleCount = 0u;
          mLocalStreamSampleOffset    = 0u;
        }
      }

      mLock.unlock();
    }
  }

  if (eIasAvbProcOK != result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error=", int32_t(result));
  }

  return result;
}


bool IasAvbAudioStream::signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples)
{
  bool requestReset = false;
  (void) numSamples;

  /* We're handling receive streams here only, transmit stream error handling is done
   * directly in the writeToAvbPacket method.
   */
  if (isReceiveStream())
  {
    switch (event)
    {
    case IasLocalAudioStreamClientInterface::eIasOverrun:
      /**
       * @log Overrun event caught.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "overrun");
      requestReset = true;
      break;

    case IasLocalAudioStreamClientInterface::eIasUnderrun:
      {
        /**
         * @log Underrun event caught.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "underrun");
        mLock.lock();
        setStreamState(IasAvbStreamState::eIasAvbStreamNoData);
        mWaitForData = true;
        mLock.unlock();
        /* Be aware that we're calling an operation on LocalStream out of one of its callback functions.
         * It would be nicer if we could decouple this to avoid undefined re-entrance issues.
         * We could inherit privately from IasLocalAudioStreamClientInterface and AVB_ASSERT(NULL != mLocalStream)
         * here, but that would complicate the existing unit-tests.
         */
        if (NULL != mLocalStream)
        {
          mLocalStream->setClientActive(false);
        }
      }
      break;

    case IasLocalAudioStreamClientInterface::eIasUnspecific:
    default:
      break;
    }
  }

  return requestReset;
}


void IasAvbAudioStream::updateRelativeFillLevel(int32_t relFillLevel)
{
  int32_t bend = 0;
  IasAvbClockDomain *clock = getClockDomain();
  // workaround for unit-testing: set id to 0 if called uninitialized
  uint64_t id = 0u;
  if (isInitialized())
  {
    id = getStreamId();
  }

  if ((0.0 != mRatioBendRate) && (NULL != clock))
  {
    AVB_ASSERT(NULL != mFillLevelFifo);
    // subtract oldest value
    mAccumulatedFillLevel -= mFillLevelFifo[mFillLevelIndex];
    // add new value and replace oldest value in FIFO
    mAccumulatedFillLevel += relFillLevel;
    mFillLevelFifo[mFillLevelIndex] = relFillLevel;
    mFillLevelIndex ++;
    if (cFillLevelFifoSize == mFillLevelIndex)
    {
      mFillLevelIndex = 0u;
    }

    // calculate rolling average over cFillLevelFifoSize measurements
    AVB_ASSERT(0u != cFillLevelFifoSize);
    const double bendRaw = mRatioBendRate * double(mAccumulatedFillLevel) / double(cFillLevelFifoSize);
    // to the power of three
    bend = int32_t(bendRaw * bendRaw * bendRaw);

    if (bend > mRatioBendLimit)
    {
      bend = mRatioBendLimit;
    }
    else if (bend < -mRatioBendLimit)
    {
      bend = -mRatioBendLimit;
    }

    clock->setDriftCompensation(bend);

    DltLogLevelType loglevel = DLT_LOG_VERBOSE;
    if (mDebugLogCount == 94u)
    {
      loglevel = DLT_LOG_DEBUG;
    }

    DLT_LOG_CXX(*mLog, loglevel, LOG_PREFIX, "IasAvbAudioStream=",
        id, "bend=", bend, "avg fill=", double(mAccumulatedFillLevel) / double(cFillLevelFifoSize));

    if (mDebugFile.is_open())
    {
      mDebugFile <<  relFillLevel << ' ' << mAccumulatedFillLevel;
      if (0 != bend)
      {
        mDebugFile <<  ' ' << bend;
      }
      mDebugFile << std::endl;
    }
  }

  if (mDebugLogCount == 94u)
  {
    mDebugLogCount = 0u;
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "IasAvbAudioStream=", id, "fill=", relFillLevel);
  }
  else
  {
    mDebugLogCount++;
  }
}


uint32_t IasAvbAudioStream::getMaxTransmitTime()
{
  uint32_t maxTransmitTime = getPresentationTimeOffset();

  if (0u == maxTransmitTime)  // receive stream
  {
    maxTransmitTime = getTSpec().getPresentationTimeOffset();
  }

  return maxTransmitTime;
}


uint32_t IasAvbAudioStream::getMinTransmitBufferSize(uint32_t periodCycle)
{
  uint64_t txWindowWidth = 24u * 125000u; // 3ms
  uint64_t txWindowPitch = 16u * 125000u; // 2ms

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitWndWidth, txWindowWidth);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitWndPitch, txWindowPitch);

  const uint32_t packetsPerSecond = getTSpec().getPacketsPerSecond();

  // the maximum number of packets could be passed to igb_avb during txWindowWidth + txWindowPitch.
  uint32_t maxPackets = uint32_t(::ceil((double)(txWindowWidth) * (double)(packetsPerSecond) / 1e9) +
                             ::floor((double)(periodCycle) / (double)(txWindowPitch)));

  // additional packet will be prepared to be sent in the next tx window
  maxPackets = maxPackets + 1u;

  return maxPackets * mSamplesPerChannelPerPacket;;
}


uint16_t IasAvbAudioStream::getPacketSize(const IasAvbAudioFormat format, const uint16_t numSamples)
{
  uint16_t size = 0u;

  switch (format)
  {
  case IasAvbAudioFormat::eIasAvbAudioFormatIec61883:
    size = getAvtpAudioPduSize<IasAvbAudioFormat::eIasAvbAudioFormatIec61883>(numSamples);
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSaf16:
    size = getAvtpAudioPduSize<IasAvbAudioFormat::eIasAvbAudioFormatSaf16>(numSamples);
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSaf24:
    size = getAvtpAudioPduSize<IasAvbAudioFormat::eIasAvbAudioFormatSaf24>(numSamples);
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSaf32:
    size = getAvtpAudioPduSize<IasAvbAudioFormat::eIasAvbAudioFormatSaf32>(numSamples);
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSafFloat:
    size = getAvtpAudioPduSize<IasAvbAudioFormat::eIasAvbAudioFormatSafFloat>(numSamples);
    break;
  default:
    break;
  }

  return size;
}


uint16_t IasAvbAudioStream::getSampleSize(const IasAvbAudioFormat format)
{
  uint16_t size = 0u;

  switch (format)
  {
  case IasAvbAudioFormat::eIasAvbAudioFormatIec61883:
    size = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatIec61883>::cSampleSize;
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSaf16:
    size = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf16>::cSampleSize;
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSaf24:
    size = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf24>::cSampleSize;
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSaf32:
    size = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf32>::cSampleSize;
    break;
  case IasAvbAudioFormat::eIasAvbAudioFormatSafFloat:
    size = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSafFloat>::cSampleSize;
    break;
  default:
    break;
  }

  return size;
}


uint8_t IasAvbAudioStream::getFormatCode(const IasAvbAudioFormat format)
{
  uint8_t code = 0u;

  std::string comp;
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cCompatibilityAudio, comp))
  {
    if (comp == "SAF")
    {
      if (IasAvbAudioFormat::eIasAvbAudioFormatSaf16 == format)
      {
        code = 2u;
      }
    }
    else
    {
      // unknown compatibility, use standard
      comp.clear();
    }
  }

  if (comp.empty())
  {
    switch (format)
    {
    case IasAvbAudioFormat::eIasAvbAudioFormatSaf16:
      code = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf16>::cFormatCode;
      break;
    case IasAvbAudioFormat::eIasAvbAudioFormatSaf24:
      code = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf24>::cFormatCode;
      break;
    case IasAvbAudioFormat::eIasAvbAudioFormatSaf32:
      code = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf32>::cFormatCode;
      break;
    case IasAvbAudioFormat::eIasAvbAudioFormatSafFloat:
      code = IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSafFloat>::cFormatCode;
      break;
    case IasAvbAudioFormat::eIasAvbAudioFormatIec61883:
    default:
      break;
    }
  }


  return code;
}


} // namespace IasMediaTransportAvb
