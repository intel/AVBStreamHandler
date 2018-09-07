/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalAudioStream.hpp
 * @brief   This class contains all methods to handle a local audio stream
 * @details A local audio stream is defined as an audio data container which
 *          can be connected to an AvbAudioStream. The source of the data could be a
 *          a wave file player or pulse etc. The standard audio format
 *          which is handled currently are 32-bit float values.
 *
 * @date    2013
 */

#ifndef IASLOCALAUDIOSTREAM_HPP_
#define IASLOCALAUDIOSTREAM_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "IasAvbTSpec.hpp"
#include "avb_streamhandler/IasLocalAudioBuffer.hpp"
#include "avb_streamhandler/IasLocalAudioBufferDesc.hpp"
#include "avb_streamhandler/IasAvbClockDomain.hpp"

#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include <vector>

namespace IasMediaTransportAvb {


/**
 * @brief callback interface class for clients of the LocalAudioStream (i.e. IasAvbAudioStream)
 */
class IasLocalAudioStreamClientInterface
{
  protected:
    IasLocalAudioStreamClientInterface() {}
    virtual ~IasLocalAudioStreamClientInterface() {}
  public:
    enum DiscontinuityEvent
    {
      eIasUnspecific = 0,
      eIasOverrun    = 1,
      eIasUnderrun   = 2
    };

    /**
     * @brief LocalStream indicates to client that an discontinuity occurred
     *
     * A discontinuity occurs when an amount of samples is missing in the stream due to
     * an overrun, underrun or other events (if any). The client can then decide whether
     * it wants to reset the ringbuffer to the optimal fill level (which means that even more
     * samples get lost) or whether it tries to conceal the problem by losing as few samples
     * as possible, without a ringbuffer reset.
     *
     * @param[in] event code indicating the type of event
     * @param[in] number of affected samples or 0 if unknown
     * @returns true if ring buffer shall be reset, false otherwise
     */
    virtual bool signalDiscontinuity( DiscontinuityEvent event, uint32_t numSamples ) = 0;

    /**
     * @brief update relative fill level
     *
     * Upon the first write to the ring buffer, the buffer snapshots the current fill level as
     * "reference". Afterwards, the buffer fill level relative to the reference is communicated.
     * Use this for fine-tuning the transmission speed
     * On reading (AVB transmit):
     * Positive values mean that the client should read faster, negative values mean that it should read slower.
     * On writing (AVB receive):
     * Positive values mean that the client should write slower, negative values mean that it should write faster.
     *
     * @param[in] relFillLevel relative fill level as defined above
     */
    virtual void updateRelativeFillLevel( int32_t relFillLevel ) = 0 ;

    /**
     * @brief return the max transmit time of the client AVB stream
     */
    virtual uint32_t getMaxTransmitTime() = 0;

    /**
     * @brief return the minimum transmit buffer size
     */
    virtual uint32_t getMinTransmitBufferSize(uint32_t periodCycle) = 0;
};

/**
 * @brief Diagnostics counters and info storage class
 */
struct IasLocalAudioStreamDiagnostics
{
public:
  /*!
   * @brief Default constructor.
   */
  IasLocalAudioStreamDiagnostics();

  /*!
   * @brief Copy constructor.
   */
  IasLocalAudioStreamDiagnostics(IasLocalAudioStreamDiagnostics const & iOther);


  IasLocalAudioStreamDiagnostics(uint32_t basePeriod, uint32_t baseFreq, uint32_t baseFillMultiplier, uint32_t baseFillMultiplierTx, uint32_t cycleWait, uint32_t totalBufferSize, uint32_t bufferReadThreshold, uint32_t resetBuffersCount, uint32_t deviationOutOfBounds);

  /*!
   * @brief Assignment operator.
   */
  IasLocalAudioStreamDiagnostics & operator = (IasLocalAudioStreamDiagnostics const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (IasLocalAudioStreamDiagnostics const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (IasLocalAudioStreamDiagnostics const & iOther) const;

  inline const uint32_t &getBasePeriod() const { return basePeriod; }
  inline void setBasePeriod(const uint32_t &value) { basePeriod = value; }

  inline const uint32_t &getBaseFreq() const { return baseFreq; }
  inline void setBaseFreq(const uint32_t &value) { baseFreq = value; }

  inline const uint32_t &getBaseFillMultiplier() const { return baseFillMultiplier; }
  inline void setBaseFillMultiplier(const uint32_t &value) { baseFillMultiplier = value; }

  inline const uint32_t &getBaseFillMultiplierTx() const { return baseFillMultiplierTx; }
  inline void setBaseFillMultiplierTx(const uint32_t &value) { baseFillMultiplierTx = value; }

  inline const uint32_t &getCycleWait() const { return cycleWait; }
  inline void setCycleWait(const uint32_t &value) { cycleWait = value; }

  inline const uint32_t &getTotalBufferSize() const { return totalBufferSize; }
  inline void setTotalBufferSize(const uint32_t &value) { totalBufferSize = value; }

  inline const uint32_t &getBufferReadThreshold() const { return bufferReadThreshold; }
  inline void setBufferReadThreshold(const uint32_t &value) { bufferReadThreshold = value; }

  inline const uint32_t &getResetBuffersCount() const { return resetBuffersCount; }
  inline void setResetBuffersCount(const uint32_t &value) { resetBuffersCount = value; }

  inline const uint32_t &getDeviationOutOfBounds() const { return deviationOutOfBounds; }
  inline void setDeviationOutOfBounds(const uint32_t &value) { deviationOutOfBounds = value; }

private:
  uint32_t basePeriod;
  uint32_t baseFreq;
  uint32_t baseFillMultiplier;
  uint32_t baseFillMultiplierTx;
  uint32_t cycleWait;
  uint32_t totalBufferSize;
  uint32_t bufferReadThreshold;
  uint32_t resetBuffersCount;
  uint32_t deviationOutOfBounds;
};


/**
 * @brief base class for all local stream classes, provides buffering & connection functionality
 */
class IasLocalAudioStream
{
  public:
    enum ClientState
    {
      eIasNotConnected = 0, ///< no network stream connected to the local stream
      eIasIdle         = 1, ///< network stream connected, but not reading/writing data
      eIasActive       = 2  ///< network stream connected and reading/writing data
    };

    typedef std::vector<IasLocalAudioBuffer*> LocalAudioBufferVec;

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLocalAudioStream();

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief write samples to local audio buffer
     *
     * @param[in] channelIdx      index of the channel, starting at 0
     * @param[in] buffer          pointer to the buffer containing the samples to be copied into the ring buffer
     * @param[in] bufferSize      number of samples to be copied into the ring buffer
     * @param[out] samplesWritten
     * @param[in] timestamp       timestamp which the samples belong to
     */
    virtual IasAvbProcessingResult writeLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer,
                                                         uint32_t bufferSize, uint16_t &samplesWritten, uint32_t timestamp);

    /**
     * @brief read samples from local audio buffer
     *
     * @param[in] channelIdx      index of the channel, starting at 0
     * @param[in] buffer          pointer to the buffer which samples to be copied into
     * @param[in] bufferSize      number of samples to be copied into the buffer
     * @param[out] samplesRead
     * @param[out] timestamp      timestamp which the read samples belong to
     */
    virtual IasAvbProcessingResult readLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer,
                                                        uint32_t bufferSize, uint16_t &samplesRead, uint64_t &timeStamp);

    virtual IasAvbProcessingResult dumpFromLocalAudioBuffer(uint16_t &numSamples);


    inline bool isInitialized() const;
    inline IasAvbStreamDirection getDirection() const;
    inline IasLocalStreamType getType() const;
    inline uint16_t  getStreamId() const;
    inline uint16_t  getNumChannels() const;
    inline uint8_t   getChannelLayout() const;
    inline uint32_t  getSampleFrequency() const;
    inline bool    hasSideChannel() const;
    inline const LocalAudioBufferVec & getChannelBuffers() const;
    inline IasLocalAudioBufferDesc * getBufferDescQ() const;
    inline bool hasBufferDesc() const;
    inline uint32_t getAudioRxDelay() const;

    //
    // methods to be used by client
    //
    /**
     * @brief called by client to reset all current local audio buffers to a start position.
     * @returns eIasAvbProcOK
     */
    virtual IasAvbProcessingResult resetBuffers() = 0;

    /**
     * @brief called by client to register at the local stream upon connection
     *
     * @param[in] client instance pointer of the object implementing the client interface
     * @returns eIasAvbProcOK on success
     * @returns eIasAvbProcInvalidParam on invalid client pointer
     * @returns eIasAvbProcAlreadyInUse when already connected
     */
    virtual IasAvbProcessingResult connect( IasLocalAudioStreamClientInterface * client );

    /**
     * @brief called by client to unregister at the local stream upon disconnection
     * @returns eIasAvbProcOK
     */
    virtual IasAvbProcessingResult disconnect();

    /**
     * @brief notifies local audio stream about activity state of client
     *
     * A change from inactive to active resets the ring buffer to optimal fill level.
     * In inactive state, the local stream will not report discontinuity events to the client.
     *
     * @param[in] active true if client is now active, false otherwise
     */
    virtual void setClientActive( bool active );
    inline bool isConnected() const;
    inline bool isReadReady() const;
    inline IasAvbProcessingResult setChannelLayout(uint8_t layout);

    /**
     *  @brief get the exclusive access right to the audio buf and the descriptor fifo
     */
    inline bool lock() const;

    /**
     *  @brief release the exclusive access right
     */
    inline bool unlock() const;

    /**
     *  @brief notifies local audio stream about activity state of worker thread i.e. AvbAlsaWrk
     */
    void setWorkerActive( bool active );

    /**
     *  @brief get current timestamp value from the time-aware descriptors FIFO
     */
    uint64_t getCurrentTimestamp();

    /**
     * @brief get diagnostics counters and info
     */
    inline IasLocalAudioStreamDiagnostics* getDiagnostics() { return &mDiag; }

  protected:

    /**
     *  @brief Constructor.
     */
    IasLocalAudioStream(DltContext &dltContext, IasAvbStreamDirection direction, IasLocalStreamType type,
        uint16_t streamId);

    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *  Derived from base class.
     */
    IasAvbProcessingResult init(uint8_t channelLayout, uint16_t numChannels, bool hasSideChannel,
        uint32_t totalSize, uint32_t sampleFrequency, uint32_t alsaPeriodSize = 0u);

    inline ClientState getClientState() const;
    inline IasLocalAudioStreamClientInterface * getClient() const;

    //
    // Members shared with derived class
    //
    DltContext                     *mLog;
    const IasAvbStreamDirection     mDirection;
    const IasLocalStreamType        mType;
    const uint16_t                  mStreamId;
    uint8_t                         mChannelLayout;
    uint16_t                        mNumChannels;
    uint32_t                        mSampleFrequency;
    bool                            mHasSideChannel;
    LocalAudioBufferVec             mChannelBuffers;

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLocalAudioStream(IasLocalAudioStream const &other);

    typedef IasLocalAudioBufferDesc::AudioBufferDescMode AudioBufferDescMode;

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLocalAudioStream& operator=(IasLocalAudioStream const &other);

    /**
     * @brief update timestamp referred to by the time-aware buffer on the receiver side
     *
     * It should be called atomically. Currently it is called from writeLocalAudioBuffer()
     * which is called from IasAvbAudioStream with holding a mutex lock.
     */
    void updateRxTimestamp(const uint32_t timestamp);

    //
    // Members
    //
    ClientState                                 mClientState;
    IasLocalAudioStreamClientInterface *        mClient;
    IasLocalAudioBufferDesc *                   mBufferDescQ;
    AudioBufferDescMode                         mDescMode;            // -k audio.tstamp.buffer option
    IasLibPtpDaemon *                           mPtpProxy;
    uint32_t                                    mEpoch;
    uint64_t                                    mLastTimeStamp;       // timestamp of the last received audio packet
    uint32_t                                    mLastSampleCnt;
    uint32_t                                    mLaunchTimeDelay;     // estimated delay needed to fill buffer half hull
    uint32_t                                    mAudioRxDelay;
    uint32_t                                    mPeriodSz;
    bool                                        mWorkerRunning;
    uint32_t                                    mPendingSamples;
    IasLocalAudioBuffer::AudioData *            mNullData;
    IasLocalAudioBufferDesc::AudioBufferDesc    mPendingDesc;
    IasLocalAudioStreamDiagnostics              mDiag;
    bool                                        mAlsaRxSyncStart;
};

inline bool IasLocalAudioStream::isInitialized() const
{
  return (0 != mSampleFrequency);
}

inline uint16_t IasLocalAudioStream::getNumChannels() const
{
  return mNumChannels;
}

inline uint16_t IasLocalAudioStream::getStreamId() const
{
  return mStreamId;
}

inline uint8_t IasLocalAudioStream::getChannelLayout() const
{
  return mChannelLayout;
}

inline uint32_t IasLocalAudioStream::getSampleFrequency() const
{
  return mSampleFrequency;
}

inline bool IasLocalAudioStream::hasSideChannel() const
{
  return mHasSideChannel;
}

inline const IasLocalAudioStream::LocalAudioBufferVec & IasLocalAudioStream::getChannelBuffers() const
{
  return mChannelBuffers;
}

inline IasLocalAudioBufferDesc * IasLocalAudioStream::getBufferDescQ() const
{
  return mBufferDescQ;
}

inline IasAvbStreamDirection IasLocalAudioStream::getDirection() const
{
  return mDirection;
}

inline IasLocalStreamType IasLocalAudioStream::getType() const
{
  return mType;
}

inline IasLocalAudioStream::ClientState IasLocalAudioStream::getClientState() const
{
  return mClientState;
}

inline IasLocalAudioStreamClientInterface * IasLocalAudioStream::getClient() const
{
  return mClient;
}

inline bool IasLocalAudioStream::isConnected() const
{
  return (eIasNotConnected != mClientState);
}

inline bool IasLocalAudioStream::isReadReady() const
{
  bool isReadReady = true;

  if (true == hasBufferDesc())
  {
    return getChannelBuffers()[0]->isReadReady();
  }

  return isReadReady;
}

inline IasAvbProcessingResult IasLocalAudioStream::setChannelLayout(uint8_t layout)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (hasSideChannel())
  {
    result = eIasAvbProcErr;
  }
  else
  {
    mChannelLayout = layout;
  }

  return result;
}

inline bool IasLocalAudioStream::hasBufferDesc() const
{
  if ((AudioBufferDescMode::eIasAudioBufferDescModeOff < mDescMode) &&
      (mDescMode < AudioBufferDescMode::eIasAudioBufferDescModeLast))
  {
    return true;
  }
  else
  {
    return false;
  }
}

inline uint32_t IasLocalAudioStream::getAudioRxDelay() const
{
  return mAudioRxDelay;
}

inline bool IasLocalAudioStream::lock() const
{
  bool ret = false;

  if (true == hasBufferDesc())
  {
    AVB_ASSERT( NULL != mBufferDescQ );
    mBufferDescQ->lock();
    ret = true;
  }
  return ret;
}

inline bool IasLocalAudioStream::unlock() const
{
  bool ret = false;

  if (true == hasBufferDesc())
  {
    AVB_ASSERT( NULL != mBufferDescQ );
    mBufferDescQ->unlock();
    ret = true;
  }
  return ret;
}


struct IasLocalAudioStreamAttributes {

public:
    /*!
     * @brief Default constructor.
     */
    IasLocalAudioStreamAttributes();

    /*!
     * @brief Copy constructor.
     */
    IasLocalAudioStreamAttributes(IasLocalAudioStreamAttributes const & iOther);

    /*!
     * @brief Field constructor.
     *
     * @param[in] iiDirection Initial value for field IasAvbClockReferenceStreamAttributes.
     */
    IasLocalAudioStreamAttributes(IasAvbStreamDirection iDirection, uint16_t iNumChannels, uint32_t iSampleFrequency,
                                  IasAvbAudioFormat iFormat, uint32_t iPeriodSize, uint32_t iNumPeriods, uint8_t iChannelLayout,
                                  bool iHasSideChannel, std::string iDeviceName, uint16_t iStreamId, bool connected,
                                  IasLocalAudioStreamDiagnostics streamDiagnostics);

    /*!
     * @brief Assignment operator.
     */
    IasLocalAudioStreamAttributes & operator = (IasLocalAudioStreamAttributes const & iOther);

    /*!
     * @brief Equality comparison operator.
     */
    bool operator == (IasLocalAudioStreamAttributes const & iOther) const;

    /*!
     * @brief Inequality comparison operator.
     */
    bool operator != (IasLocalAudioStreamAttributes const & iOther) const;


    inline const IasAvbStreamDirection &getDirection() const { return direction; }
    inline void setDirection(const IasAvbStreamDirection &value) { direction = value; }

    inline const uint16_t &getNumChannels() const { return numChannels; }
    inline void setNumChannels(const uint16_t &value) { numChannels = value; }

    inline const uint32_t &getSampleFrequency() const { return sampleFrequency; }
    inline void setSampleFrequency(const uint32_t &value) { sampleFrequency = value; }

    inline const IasAvbAudioFormat &getFormat() const { return format; }
    inline void setFormat(const IasAvbAudioFormat &value) { format = value; }

    inline const uint32_t &getPeriodSize() const { return periodSize; }
    inline void setPeriodSize(const uint32_t &value) { periodSize = value; }

    inline const uint32_t &getNumPeriods() const { return numPeriods; }
    inline void setNumPeriods(const uint32_t &value) { numPeriods = value; }

    inline const uint8_t &getChannelLayout() const { return channelLayout; }
    inline void setChannelLayout(const uint8_t &value) { channelLayout = value; }

    inline const bool &getHasSideChannel() const { return hasSideChannel; }
    inline void setHasSideChannel(bool value) { hasSideChannel = value; }

    inline const std::string &getDeviceName() const { return deviceName; }
    inline void setDeviceName(std::string value) { deviceName = value; }

    inline const uint16_t &getStreamId() const { return streamId; }
    inline void setStreamId(const uint16_t &value) { streamId = value; }

    inline const bool &getConnected() const { return connected; }
    inline void setConnected(bool value) { connected = value; }

    inline const IasLocalAudioStreamDiagnostics &getStreamDiagnostics() const { return streamDiagnostics; }
    inline void setStreamDiagnostics(const IasLocalAudioStreamDiagnostics &value) { streamDiagnostics = value; }

private:
    IasAvbStreamDirection direction;
    uint16_t numChannels;
    uint32_t sampleFrequency;
    IasAvbAudioFormat format;
    uint32_t periodSize;
    uint32_t numPeriods;
    uint8_t channelLayout;
    bool hasSideChannel;
    std::string deviceName;
    uint16_t streamId;
    bool connected; // in use
    IasLocalAudioStreamDiagnostics streamDiagnostics;
};

typedef std::vector<IasLocalAudioStreamAttributes> LocalAudioStreamInfoList;

} // namespace IasMediaTransportAvb

#endif /* IASLOCALAUDIOSTREAM_HPP_ */
