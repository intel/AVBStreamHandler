/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbTransmitEngine.hpp
 * @brief   The Transmit Engine manages the TX Sequencers and the AVB Streams using them.
 * @details All variants of AVB TX Streams are created through the TX Engine. TX Sequencers
 *          are created on-demand on a "per SR class" basis. Streams are assigned to sequencers
 *          upon their activation. Starting/stopping the engine results in starting/stopping
 *          the sequencers.
 * @date    2013
 */

#ifndef IASAVBTRANSMITENGINE_HPP_
#define IASAVBTRANSMITENGINE_HPP_

#include "IasAvbTypes.hpp"
#include "IasAvbStream.hpp"
#include "IasAvbStreamHandlerEnvironment.hpp"
#include "IasAvbStreamHandlerEventInterface.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_helper/IasIRunnable.hpp"

namespace IasMediaTransportAvb {

class IasLocalAudioStream;
class IasLocalVideoStream;
class IasAvbClockDomain;
class IasAvbTransmitSequencer;

class IasAvbTransmitEngine : private IasAvbStreamHandlerEventInterface
{
  public:

    /**
     *  @brief Constructor.
     */
    IasAvbTransmitEngine();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbTransmitEngine();

    /**
     * @brief Allocates internal resources and initializes instance.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult init();

    /**
     *  @brief Clean up all allocated resources.
     *
     *  Calling this routine turns the object into the pre-init state, i.e.
     *  init() can be called again.
     *  Also called by the destructor.
     */
    void cleanup();

    /**
     * @brief register interface for event callbacks
     *
     * @param[in] interface pointer to instance implementing the callback interface
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbProcessingResult registerEventInterface( IasAvbStreamHandlerEventInterface * eventInterface );

    /**
     * @brief delete registration of interface for event callbacks
     *
     * @param[in] interface pointer to registered client
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbProcessingResult unregisterEventInterface( IasAvbStreamHandlerEventInterface * eventInterface );


    /**
     * @brief Starts the sequencer(s).
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult start();

    /**
     * @brief Starts the sequencer(s).
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult stop();


    /**
     * @brief Creates an AvbAudioStream. This stream can be used to be connected
     *        to a local audio stream.
     *
     * @param[in] srClass           Stream reservation class to be used
     * @param[in] maxNumberChannels The maximum number of channels the stream can hold
     * @param[in] sampleFreq        The frequency of the underlying samples
     * @param[in] format            The format of the data (IEC61883, SAF16, ...)
     * @param[in] clockDomain       Selects one of the various clock domains the stream should be bound to
     * @param[in,out] streamId      The id of the AVB stream.
     * @param[in,out] destMacAddr   The destination MAC address
     * @param[in] preconfigured     Indicates whether the stream has been created during setup (true) or runtime (false)
     *
     * @returns   eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult createTransmitAudioStream(IasAvbSrClass srClass,
                                                uint16_t const maxNumberChannels,
                                                uint32_t const sampleFreq,
                                                IasAvbAudioFormat const format,
                                                IasAvbClockDomain * const clockDomain,
                                                const IasAvbStreamId & streamId,
                                                const IasAvbMacAddress & destMacAddr,
                                                bool preconfigured);

    /**
     * @brief Creates an AvbVideoStream. This stream can be used to be connected
     *        to a local video stream.
     *
     * @param[in] srClass           Stream reservation class to be used
     * @param[in] maxPacketRate     Maximum number of packets that will be transmitted per second
     * @param[in] maxPacketSize     Maximum size of a packet in bytes
     * @param[in] format            Format of the data (IEC61883, RTP, ...)
     * @param[in] clockDomain       Selects one of the various clock domains the stream should be bound to
     * @param[in,out] streamId      The id of the AVB stream.
     * @param[in,out] destMacAddr   The destination MAC address
     * @param[in] preconfigured     Indicates whether the stream has been created during setup (true) or runtime (false)
     *
     * @returns   eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult createTransmitVideoStream(IasAvbSrClass srClass,
                                                uint16_t const maxPacketRate,
                                                uint16_t const maxPacketSize,
                                                IasAvbVideoFormat const format,
                                                IasAvbClockDomain * const clockDomain,
                                                const IasAvbStreamId & streamId,
                                                const IasAvbMacAddress & destMacAddr,
                                                bool preconfigured);


    /**
     * @brief Creates an AvbClockReferenceStream. This kind of stream cannot be connected to local streams.
     *
     * @param[in] srClass           Stream reservation class to be used
     * @param[in] type              type of of the stream as defined in IEEE1722-rev1 11.2.6
     * @param[in] crfStampsPerPdu   number of CRF time stamps to be put into each PDU
     * @param[in] crfStampInterval  number of clock events per time stamp
     * @param[in] baseFreq          base frequency (events per second)
     * @param[in] pull              frequency modifier as defined in IEEE1722-rev1 11.2.8
     * @param[in] clockDomain       clock domains the stream generates its timing from
     * @param[in,out] streamId      The id of the AVB stream
     * @param[in,out] destMacAddr   The destination MAC address
     *
     * @returns   eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult createTransmitClockReferenceStream(IasAvbSrClass srClass,
                                                IasAvbClockReferenceStreamType type,
                                                uint16_t crfStampsPerPdu,
                                                uint16_t crfStampInterval,
                                                uint32_t baseFreq,
                                                IasAvbClockMultiplier pull,
                                                IasAvbClockDomain * const clockDomain,
                                                const IasAvbStreamId & streamId,
                                                const IasAvbMacAddress & destMacAddr);

    /**
     * @brief Destroys an already created AVB stream.
     *
     * @param[in] streamId The id of the AVB stream.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult destroyAvbStream(IasAvbStreamId const & streamId);

    /**
     * @brief Activates an AVB stream so it starts transmitting.
     * For an stream that is already active, the request is ignored and success
     * is indicated.
     *
     * @param[in] streamId The id of the AVB stream.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult activateAvbStream(IasAvbStreamId const & streamId);

    /**
     * @brief Deactivates an previously activated AVB stream so it stops transmitting.
     * For an stream that is already inactive, the request is ignored and success
     * is indicated.
     *
     * @param[in] streamId The id of the AVB stream.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult deactivateAvbStream(IasAvbStreamId const & streamId);

    /**
     * @brief Connects an AVB audio stream to a local audio stream.
     *
     * @param[in] avbStreamId The id of the AVB (audio) stream.
     * @param[in] localStream A pointer to the local audio stream instance
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult connectAudioStreams(const IasAvbStreamId & avbStreamId, IasLocalAudioStream * localStream);

    /**
     * @brief Connects an AVB video stream to a local video stream.
     *
     * @param[in] avbStreamId The id of the AVB (video) stream.
     * @param[in] localStream A pointer to the local video stream instance
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult connectVideoStreams(const IasAvbStreamId & avbStreamId, IasLocalVideoStream * localStream);

    /**
     * @brief Disconnects an AVB stream from a local audio/video stream.
     *
     * @param[in] avbStreamId The id of the AVB stream.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult disconnectStreams(const IasAvbStreamId & avbStreamId);

    /**
     * @brief checks if the given stream id is valid (available)
     *
     * @returns 'true' if id is valid, otherwise 'false'
     */
    inline bool isValidStreamId(const IasAvbStreamId & avbStreamId) const;

    /**
     * @brief Populates the provided list structures with the info for one or all streams.
     *
     * @param[in] id of the one stream to provide info for, otherwise value 0 to indicate all stream info.
     * @param[out] audioStreamInfo vector to populate with audio stream data.
     * @param[out] videoStreamInfo vector to populate with video stream data.
     * @param[out] clockRefStreamInfo vector to populate with clock reference stream data.
     *
     * @returns 'true' if a non-zero id is specified and found, otherwise 'false'.
     *
     */
    bool getAvbStreamInfo(const IasAvbStreamId &id, AudioStreamInfoList &audioStreamInfo,
                          VideoStreamInfoList &videoStreamInfo, ClockReferenceStreamInfoList &clockRefStreamInfo) const;

  private:

    //
    // types
    //

    typedef std::map<IasAvbStreamId, IasAvbStream*> AvbStreamMap;

    //
    // constants
    //

    static const uint32_t cIgbAccessSleep = 100000u; // in us: 100 ms
    //
    // helpers
    //

    IasAvbTransmitSequencer * getSequencerByStream(IasAvbStream *stream) const;
    IasAvbTransmitSequencer * getSequencerByClass(IasAvbSrClass qavClass) const;
    IasAvbProcessingResult createSequencerOnDemand(IasAvbSrClass qavClass);
    void updateShapers();

    //{@
    /// @brief IasAvbStreamHandlerEventInterface implementation
    virtual void updateLinkStatus(const bool linkIsUp);
    virtual void updateStreamStatus(uint64_t streamId, IasAvbStreamState status);
    //@}

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbTransmitEngine(IasAvbTransmitEngine const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbTransmitEngine& operator=(IasAvbTransmitEngine const &other);

    inline bool isInitialized() const;
    inline bool isRunning() const;

    ///
    /// Member Variables
    ///

    device_t          *mIgbDevice;
    AvbStreamMap       mAvbStreams;
    bool               mUseShaper;
    bool               mUseResume;
    bool               mRunning;
    IasAvbTransmitSequencer * mSequencers[IasAvbTSpec::cIasAvbNumSupportedClasses];
    IasAvbStreamHandlerEventInterface *mEventInterface;
    DltContext     *mLog;           // context for Log & Trace
    bool               mBTMEnable;
};

inline bool IasAvbTransmitEngine::isInitialized() const
{
  return (NULL != mIgbDevice);
}


inline bool IasAvbTransmitEngine::isValidStreamId(const IasAvbStreamId & avbStreamId) const
{
  return (mAvbStreams.end() != mAvbStreams.find(avbStreamId));
}

inline bool IasAvbTransmitEngine::isRunning() const
{
  return mRunning;
}


} // namespace IasMediaTransportAvb

#endif /* IASAVBTRANSMITENGINE_HPP_ */
