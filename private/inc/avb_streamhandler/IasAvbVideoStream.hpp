/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoStream.hpp
 * @brief   This is the definition of the IasAvbVideoStream class.
 * @details The IasAvbVideoStream is an successor of the IasAvbStream class.
 *          It cares for all regarding the reception or transmission of AVB
 *          streams.
 * @date    2014
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOSTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOSTREAM_HPP

#include "IasAvbTypes.hpp"
#include "IasAvbStream.hpp"
#include "IasLocalVideoBuffer.hpp"
#include "IasLocalVideoStream.hpp"
#include <mutex>

namespace IasMediaTransportAvb {

class IasLocalVideoStream;

class IasAvbVideoStream : public IasAvbStream, public virtual IasLocalVideoStreamClientInterface
{
  public:

    static const uint32_t cValidateNever = 0u;      ///< constant for validation mode: never validate, always use in-packet data
    static const uint32_t cValidateOnce = 1u;       ///< constant for validation mode: stop validation after first validated packet
    static const uint32_t cValidateAlways = 2u;     ///< constant for validation mode: validate each packet
    static const size_t cAvtpHeaderSize = 24u;
    static const size_t cRtppHeaderSize = 12u;
    static const size_t cEthHeaderSize = 18u;
    static const size_t cCIPHeaderSize = 8u;

    typedef IasLocalVideoBuffer::VideoData VideoData;

    /**
     *  @brief Constructor.
     */
    IasAvbVideoStream();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbVideoStream();

    inline bool isConnected() const;

    IasAvbProcessingResult initTransmit(IasAvbSrClass srClass, uint16_t maxPacketRate, uint16_t maxPacketSize,
                                        IasAvbVideoFormat format, const IasAvbStreamId & streamId, uint32_t poolSize,
                                        IasAvbClockDomain * clockDomain, const IasAvbMacAddress & dmac, bool preconfigured);
    IasAvbProcessingResult initReceive(IasAvbSrClass srClass, uint16_t maxPacketRate, uint16_t maxPacketSize,
                                       IasAvbVideoFormat format, const IasAvbStreamId & streamId,
                                       const IasAvbMacAddress & dmac, uint16_t vid, bool preconfigured);

    IasAvbProcessingResult connectTo(IasLocalVideoStream* localStream);

    uint8_t getFormatCode(const IasAvbVideoFormat format);
    uint8_t getVideoFormatCode(const IasAvbVideoFormat format);

    /* Getters for diagnostics */

    IasAvbVideoFormat getAvbVideoFormat() const { return mVideoFormat; }
    uint16_t getLocalStreamId() const { return (NULL != mLocalStream ? mLocalStream->getStreamId() : 0); }
    uint16_t getMaxPacketRate() const { return mMaxPacketRate; }
    uint16_t getMaxPacketSize() const { return mMaxPacketSize; }

  protected:

    // implementation mandated through base class
    virtual void readFromAvbPacket(const void* packet, size_t length);
    virtual void derivedCleanup();

    // overrides
    virtual IasAvbPacket* preparePacket(uint64_t nextWindowStart);
    virtual void activationChanged();

    //@{
    /// @brief IasLocalVideoStreamClientInterface implementation
    virtual bool signalDiscontinuity(DiscontinuityEvent event, uint32_t numSamples);
    //@}


  private:

    enum Compatibility
    {
      eCompCurrent,    //draft 14 - D14
      eComp1722aD5,
      eComp1722aD9
    };

    /**
     * @brief Constant used to define the measurement window for calculating packets/s value for debug
     */
    static const uint64_t cObservationInterval = 1000000000u;

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoStream(IasAvbVideoStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoStream& operator=(IasAvbVideoStream const &other);

    ///
    /// Helpers
    ///
    IasAvbProcessingResult prepareAllPackets();
    void resetTime(bool hard);
    bool finalizeAvbPacket(IasLocalVideoBuffer::IasVideoDesc *descPacket);
    bool prepareDummyAvbPacket(IasAvbPacket* packet);

    // dummy override, not used, no implementation
    virtual bool writeToAvbPacket(IasAvbPacket* packet, uint64_t nextWindowStart);

    ///
    /// Members
    ///
    IasAvbVideoFormat     mVideoFormat;
    uint8_t                 mVideoFormatCode;
    Compatibility         mCompatibility;
    IasLocalVideoStream   *mLocalStream;
    uint16_t                mMaxPacketRate;
    uint16_t                mMaxPacketSize;
    uint32_t                mLaunchTimeDelta;
    uint64_t                mPacketLaunchTime;
    std::mutex            mLock;
    uint8_t                 mSeqNum;
    uint16_t                mRtpSequNrLast;
    uint8_t                 mRtpSeqHighByte;
    double               mSampleIntervalNs;
    bool                  mWaitForData;
    uint32_t                mValidationMode;
    uint32_t                mDebugLogCount;
    uint32_t                mNumSkippedPackets;
    uint32_t                mNumPacketsToSkip;
    bool                  mDebugIn;
    uint32_t                mValidationThreshold;
    uint32_t                mValidationCount;
    uint32_t                mMsgCount;
    uint32_t                mMsgCountMax;
    uint64_t                mLocalTimeLast;
    uint32_t                mRefPaneSampleCount;
    uint32_t                mRefPaneSampleTime;
    uint8_t                 mDatablockSeqNum;
};

inline bool IasAvbVideoStream::isConnected() const
{
  return (NULL != mLocalStream);
}


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOSTREAM_HPP */
