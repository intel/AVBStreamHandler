/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbAudioStream.hpp
 * @brief   This is the definition of the IasAvbAudioStream class.
 * @details The IasAvbAudioStream is an successor of the IasAvbStream class.
 *          It cares for all regarding the reception or transmission of AVB
 *          streams.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBAUDIOSTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBAUDIOSTREAM_HPP

#include "IasAvbTypes.hpp"
#include "IasAvbStream.hpp"
#include "IasLocalAudioBuffer.hpp"
#include "IasLocalAudioStream.hpp"
#include <fstream>
#include <mutex>

namespace IasMediaTransportAvb {

class IasLocalAudioStream;

class IasAvbAudioStream : public IasAvbStream, public virtual IasLocalAudioStreamClientInterface
{
  public:

    static const uint32_t cValidateNever = 0u;      ///< constant for validation mode: never validate, always use in-packet data
    static const uint32_t cValidateOnce = 1u;       ///< constant for validation mode: stop validation after first validated packet
    static const uint32_t cValidateAlways = 2u;     ///< constant for validation mode: validate each packet
    static const size_t cAvtpHeaderSize = 24u;

    typedef IasLocalAudioBuffer::AudioData AudioData;

    /**
     *  @brief Constructor.
     */
    IasAvbAudioStream();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbAudioStream();

    inline bool isConnected() const;

    IasAvbProcessingResult initTransmit(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
                                        IasAvbAudioFormat format, const IasAvbStreamId & streamId, uint32_t poolSize,
                                        IasAvbClockDomain * clockDomain, const IasAvbMacAddress & dmac, bool preconfigured);
    IasAvbProcessingResult initReceive(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
                                       IasAvbAudioFormat format, const IasAvbStreamId & streamId,
                                       const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured);

    IasAvbProcessingResult connectTo(IasLocalAudioStream* localStream);

    static uint16_t getPacketSize(const IasAvbAudioFormat format, const uint16_t numSamples);
    static uint16_t getSampleSize(const IasAvbAudioFormat format);
    static uint8_t getFormatCode(const IasAvbAudioFormat format);

    /* Getters for diagnostics */

    uint16_t getMaxNumChannels() const         { return mMaxNumChannels;  }
    uint32_t getSampleFrequency() const        { return mSampleFrequency; }
    IasAvbAudioFormat getAudioFormat() const { return mAudioFormat;     }
    uint16_t getLocalNumChannels() const       { return (NULL != mLocalStream ? mLocalStream->getNumChannels() : 0); }
    uint16_t getLocalStreamId() const          { return (NULL != mLocalStream ? mLocalStream->getStreamId() : 0);    }

  protected:

    // implementation mandated through base class
    virtual bool writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart);
    virtual void readFromAvbPacket(const void* packet, size_t length);
    virtual void derivedCleanup();

    // overrides
    virtual void activationChanged();

    //@{
    /// @brief IasLocalAudioStreamClientInterface implementation
    virtual bool signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples);
    virtual void updateRelativeFillLevel(int32_t relFillLevel);
    virtual uint32_t getMaxTransmitTime();
    virtual uint32_t getMinTransmitBufferSize(uint32_t periodCycle);
    //@}


  private:

    /* @brief only needed for stream providing drift compensation reference
     */
    static const uint32_t cFillLevelFifoSize = 64u;

    /**
     * @brief local helper type for side channel conversion
     */
    union SideChannel
    {
      AudioData pseudoAudio;
      uint8_t raw[4];
      uint32_t value;
    };

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbAudioStream(IasAvbAudioStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbAudioStream& operator=(IasAvbAudioStream const &other);

    ///
    /// Helpers
    ///
    IasAvbProcessingResult prepareAllPackets();
    bool resetTime(uint64_t nextWindowStart);
    static uint8_t getSampleFrequencyCode(uint32_t sampleFrequency);
    IasAvbCompatibility getCompatibilityModeAudio();

    ///
    /// Members
    ///
    IasAvbCompatibility   mCompatibilityModeAudio;
    IasAvbAudioFormat     mAudioFormat;
    uint8_t                 mAudioFormatCode;
    uint16_t                mMaxNumChannels;
    IasLocalAudioStream   *mLocalStream;
    uint32_t                mSampleFrequency;
    uint8_t                 mSampleFrequencyCode;
    uint64_t                mRefPlaneSampleCount;
    int64_t                 mRefPlaneSampleOffset;
    uint64_t                mRefPlaneSampleTime;
    uint64_t                mMasterCount;
    uint64_t                mLastMasterCount;
    uint64_t                mMasterTime;
    uint64_t                mLastMasterTime;
    uint64_t                mMasterTimeout;
    uint32_t                mDummySamplesSent;
    uint32_t                mDumpCount;
    uint64_t                mPacketLaunchTime;
    std::mutex            mLock;
    uint16_t                mSamplesPerChannelPerPacket;
    uint16_t                mStride;
    uint8_t                 mSeqNum;
    AudioData*            mTempBuffer;
    AudioData             mConversionGain;
    bool                  mUseSaturation;
    double               mSampleIntervalNs;
    bool                  mWaitForData;
    double               mRatioBendRate;
    int32_t                 mRatioBendLimit;
    int32_t                 mAccumulatedFillLevel;
    uint32_t                mFillLevelIndex;
    int32_t*                mFillLevelFifo;
    uint32_t                mValidationMode;
    uint32_t                mDebugLogCount;
    uint32_t                mNumSkippedPackets;
    uint32_t                mNumPacketsToSkip;
    bool                  mDebugIn;
    uint32_t                mValidationThreshold;
    uint32_t                mValidationCount;
    uint32_t                mExcessSamples;
    std::ofstream         mDebugFile;
    uint64_t                mLocalStreamReadSampleCount;
    uint64_t                mLocalStreamSampleOffset;
    uint64_t                mLastRefPlaneSampleTime;
    bool                  mFirstRun;
    bool                  mBTMEnable;
    uint64_t              mMasterTimeUpdateMinInterval;

    static uint32_t sampleRateTable[];
};

inline bool IasAvbAudioStream::isConnected() const
{
  return (NULL != mLocalStream);
}


/**
 * @brief helper template to deal with audio format traits. Could go to separate header file later.
 */
template<IasAvbAudioFormat>
class IasAvbAudioFormatTraits;

static const uint16_t cIasAvtpHeaderSize = 24u;
static const uint16_t cIasCipHeaderSize = 8u;

template<>
class IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatIec61883>
{
  public:
    static const uint16_t cSampleSize = 4u;
    static const uint16_t cHeaderSize = cIasAvtpHeaderSize + cIasCipHeaderSize;
};

template<>
class IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf16>
{
  public:
    static const uint16_t cSampleSize = 2u;
    static const uint16_t cHeaderSize = cIasAvtpHeaderSize;
    static const uint8_t  cFormatCode = 4u;
};

template<>
class IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf24>
{
  public:
    static const uint16_t cSampleSize = 3u;
    static const uint16_t cHeaderSize = cIasAvtpHeaderSize;
    static const uint8_t  cFormatCode = 3u;
};

template<>
class IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSaf32>
{
  public:
    static const uint16_t cSampleSize = 4u;
    static const uint16_t cHeaderSize = cIasAvtpHeaderSize;
    static const uint8_t  cFormatCode = 2u;
};

template<>
class IasAvbAudioFormatTraits<IasAvbAudioFormat::eIasAvbAudioFormatSafFloat>
{
  public:
    static const uint16_t cSampleSize = 4u;
    static const uint16_t cHeaderSize = cIasAvtpHeaderSize;
    static const uint8_t  cFormatCode = 1u;
};

template <IasAvbAudioFormat F>
inline uint16_t getAvtpAudioPduSize(const uint16_t numSamples)
{
  return uint16_t(IasAvbAudioFormatTraits<F>::cHeaderSize + IasAvbAudioFormatTraits<F>::cSampleSize * numSamples);
}


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBAUDIOSTREAM_HPP */
