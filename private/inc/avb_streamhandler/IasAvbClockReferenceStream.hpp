/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockReferenceStream.hpp
 * @brief   This is the definition of the IasAvbClockReferenceStream class.
 * @details The IasAvbClockReferenceStream is an successor of the IasAvbStream class.
 *          It cares for all regarding the reception or transmission of AVB
 *          streams.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBCLOCKREFERENCESTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBCLOCKREFERENCESTREAM_HPP

#include "IasAvbTypes.hpp"
#include "IasAvbStream.hpp"
#include "IasLocalAudioBuffer.hpp"
#include "IasLocalAudioStream.hpp"
#include <mutex>

namespace IasMediaTransportAvb {

class IasAvbClockReferenceStream : public IasAvbStream
{
  public:

    static const uint32_t cValidateNever = 0u;      ///< constant for validation mode: never validate, always use in-packet data
    static const uint32_t cValidateOnce = 1u;       ///< constant for validation mode: stop validation after first validated packet
    static const uint32_t cValidateAlways = 2u;     ///< constant for validation mode: validate each packet

    /**
     *  @brief Constructor.
     */
    IasAvbClockReferenceStream();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbClockReferenceStream();

    IasAvbProcessingResult initTransmit(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type, uint16_t crfStampsPerPdu, uint16_t crfStampInterval, uint32_t baseFreq, IasAvbClockMultiplier pull, const IasAvbStreamId & streamId, uint32_t poolSize, IasAvbClockDomain * clockDomain, const IasAvbMacAddress & dmac);
    IasAvbProcessingResult initReceive(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type, uint16_t maxCrfStampsPerPdu, const IasAvbStreamId & streamId, const IasAvbMacAddress & dmac);
    IasAvbCompatibility getCompatibilityMode();

    /* Getters for diagnostics */

    IasAvbClockReferenceStreamType getCrfStreamType() const { return mType;              }
    uint32_t getBaseFrequency()                         const { return mBaseFrequency;     }
    const IasAvbClockMultiplier& getClockMultiplier() const { return mPull;              }
    uint16_t getTimeStampInterval()                     const { return mTimeStampInterval; }
    uint16_t getTimeStampsPerPdu()                      const { return mTimeStampsPerPdu;  }

  protected:

    // implementation mandated through base class
    virtual bool writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart);
    virtual void readFromAvbPacket(const void* packet, size_t length);
    virtual void derivedCleanup();

    // overrides
    virtual void activationChanged();

  private:

    static const uint32_t cCrfTimeStampSize = 8u;

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbClockReferenceStream(IasAvbClockReferenceStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbClockReferenceStream& operator=(IasAvbClockReferenceStream const &other);

    ///
    /// Helpers
    ///
    static uint32_t decodeNominalFreq(uint8_t nominalField);
    IasAvbProcessingResult prepareAllPackets();
    bool resetTime(uint64_t nextWindowStart);
    void initFormat();

    ///
    /// Members
    ///
    uint32_t                mCrfHeaderSize;
    uint16_t                mPayloadHeaderSize;
    IasAvbClockReferenceStreamType mType;
    uint32_t                mBaseFrequency;
    IasAvbCompatibility   mCompatibilityMode;
    uint16_t                mPayloadOffset32;
    uint16_t                mPayloadLenOffset16;
    uint16_t                mSubtype;
    uint16_t                mTypeoffset8;
    IasAvbClockMultiplier mPull;
    uint16_t                mTimeStampInterval;
    uint16_t                mTimeStampsPerPdu;
    uint8_t                 mMediaClockRestartToggle;
    uint64_t                mRefPlaneEventCount;
    uint64_t                mRefPlaneEventTime;
    uint64_t                mMasterCount;
    uint64_t                mLastMasterCount;
    uint64_t                mMasterTime;
    uint64_t                mLastMasterTime;
    uint64_t                mMasterTimeout;
    int64_t                 mRefPlaneEventOffset;
    uint64_t                mPacketLaunchTime;
    std::mutex         mLock;
    uint8_t               mSeqNum;
    bool                  mClockValid;
    uint64_t              mHoldoffTime;
    uint32_t              mValidationMode;
    uint32_t              mValidationThreshold;
    uint32_t              mValidationCount;
    bool                  mFirstRun;
    bool                  mBTMEnable;
    uint64_t              mMasterTimeUpdateMinInterval;
};

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBCLOCKREFERENCESTREAM_HPP */
