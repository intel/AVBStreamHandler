/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamHandlerInterface.hpp
 * @brief   This is the interface to access the AVB stream handler.
 * @details This is a pure virtual interface class that will be
 *          implemented by the AVB stream handler. It provides
 *          functions that are intended to be used to control the
 *          stream handler from the outside.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLERINTERFACE_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLERINTERFACE_HPP

#include "IasAvbStreamHandlerTypes.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_helper/ias_visibility.h"
#include <string>

namespace IasMediaTransportAvb {


/**
 * @brief the API of the AVB stream handler
 */
class IAS_DSO_PUBLIC IasAvbStreamHandlerInterface
{
  protected:
    //@{
    /// Implementation object cannot be created or destroyed through this interface
    IasAvbStreamHandlerInterface() {}
    /* non-virtual */ ~IasAvbStreamHandlerInterface() {}
    //@}

  public:
    typedef uint64_t AvbStreamId;
    typedef uint64_t MacAddress;

    /**
     * @brief Creates a stream to receive audio data from AVB network.
     *
     * @param[in] srClass             Stream reservation class to be used
     * @param[in] maxNumberChannels   Maximum number of channels within the stream
     * @param[in] sampleFreq          Underlying sample frequency in Hz
     * @param[in] streamId            ID of the AVB stream
     * @param[in] destMacAddr         MAC address to listen on
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createReceiveAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
        AvbStreamId streamId, MacAddress destMacAddr) = 0;

    /**
     * @brief Creates a stream to send audio data over AVB network.
     *
     * @param[in] srClass             Stream reservation class to be used
     * @param[in] maxNumberChannels   maximum number of channels within the stream
     * @param[in] sampleFreq          Underlying sample frequency in Hz
     * @param[in] format              Format of the stream (SAF, 61883, ...)
     * @param[in] clockId             ID of the clock that is used
     * @param[in] assignMode          Specifies how MAC address and stream ID are assigned
     * @param[in,out] streamId        ID of the AVB stream
     * @param[in,out] destMacAddr     MAC address to which the packets will be sent
     * @param[in] active              if true, the stream will be activated immediately (see @ref setStreamActive)
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createTransmitAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
        IasAvbAudioFormat format, uint32_t clockId, IasAvbIdAssignMode assignMode, AvbStreamId &streamId,
        MacAddress &destMacAddr, bool active) = 0;

    /**
     * @brief Destroys a previously created AVB stream.
     *
     * @param[in] streamId   The ID of the AVB stream that should be destroyed.
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult destroyStream(AvbStreamId streamId) = 0;

    /**
     * @brief sets an AVB transmit stream to active or inactive
     *
     *  For streams not handled through SRP, the application can set those
     *  to active or inactive manually. It is not recommended to use this
     *  method on SRP-managed streams. Receive streams cannot be deactivated.
     *
     * @param[in] streamId   The ID of the AVB stream.
     * @param[in] active     true if the stream shall be activated, false if the stream shall be deactivated
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult setStreamActive(AvbStreamId streamId, bool active) = 0;

    /**
     * @brief Creates a local audio stream and a virtual ALSA interface
     *
     *  Creates a local stream that in turn will create a virtual ALSA device.
     *
     *  If hasSideChannel is set to true, the channel_layout parameter is ignored. Instead, the audio channel with
     *  the highest index (i.e. numberOfChannels-1) shall contain the channel layout code instead of audio data.
     *
     *  The format parameter selects the PCM data format to be used at the ALSA interface. Currently, only
     *  eIasAvbAudioFormatSaf16 is supported.
     *
     *  If streamId is 0, the stream handler will generate a valid ID and return it in the streamId parameter.
     *  Otherwise, the stream handler will use the specified value or return an error if the value is already in use.
     *
     * @param[in] direction          specifies whether this is a transmit or receive stream
     * @param[in] numberOfChannels   number of channels provided by the ALSA interface
     * @param[in] sampleFreq         nominal sample frequency in Hz
     * @param[in] format             data format to be used at the ALSA interface
     * @param[in] clockId            ID fo the clock domain the stream in driven from
     * @param[in] periodSize         size of the ALSA period (number of frames between each hardware interrupt)
     * @param[in] numPeriods         size of the IPC buffer in periods
     * @param[in] channelLayout      Application specific value indicating layout of audio data within the channel;
     *                               depending on the setting of the compatibility.audio option, only the lower 4 bits
     *                               of the layout argument are valid
     * @param[in] hasSideChannel     use last audio channel for channel info
     * @param[in] deviceName         string used as device name in ALSA configuration
     * @param[inout] streamId        ID of the local stream
     * @param[in] alsaHwDevice       use a virtual Alsa device == 0 or HW Alsa Device without ASRC == 1 or HW Alsa Device with ASRC == 2
     * @param[in] sampleFreqASRC     nominal sample frequency in Hz for ASRC (only needed when it's used)
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createAlsaStream(IasAvbStreamDirection direction, uint16_t numberOfChannels,
        uint32_t sampleFreq, IasAvbAudioFormat format, uint32_t clockId, uint32_t periodSize, uint32_t numPeriods, uint8_t channelLayout,
        bool hasSideChannel, std::string const &deviceName, uint16_t &streamId, IasAlsaDeviceTypes alsaDeviceType, uint32_t sampleFreqASRC) = 0;

    /**
     * @brief Creates a local audio stream using test tone generators
     *
     *  Creates a local stream that produces test tones on its audio channels.
     *
     *  Upon creation, all channels will be filled with a 1kHz sine wave at full scale. Use the @ref setTestToneParams
     *  method to modify the test tones after creation.
     *
     *  The format parameter selects the PCM data format to be used by the generator. Currently, only
     *  eIasAvbAudioFormatSaf16 is supported.
     *
     *  If streamId is 0, the stream handler will generate a valid ID and return it in the streamId parameter.
     *  Otherwise, the stream handler will use the specified value or return an error if the value is already in use.
     *
     * @param[in] numberOfChannels   number of channels to generate
     * @param[in] sampleFreq         nominal sample frequency in Hz
     * @param[in] format             data format to be used
     * @param[in] channelLayout      Application specific value indicating layout of audio data within the channel;
     *                               depending on the setting of the compatibility.audio option, only the lower 4 bits
     *                               of the layout argument are valid
     * @param[inout] streamId        ID of the local stream
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createTestToneStream(uint16_t numberOfChannels,
        uint32_t sampleFreq, IasAvbAudioFormat format, uint8_t channelLayout, uint16_t &streamId ) = 0;

    /**
     * @brief Destroys a local stream
     *
     * Destroys streams that have been created with @ref createAlsaStream.
     *
     * @param[in] streamId   ID of stream to be destroyed
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult destroyLocalStream(uint16_t streamId) = 0;


    /**
     * @brief Connects an AVB stream and a local audio stream .
     *
     * @param[in] networkStreamId   ID of AVB stream
     * @param[in] localStreamId     ID of local stream
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult connectStreams(AvbStreamId networkStreamId, uint16_t localStreamId) = 0;

    /**
     * @brief Disconnects an already connected AVB stream and local audio stream.
     *
     * @param[in] networkStreamId   ID of AVB stream
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult disconnectStreams(AvbStreamId networkStreamId) = 0;

    /**
     * @brief Sets the layout of the audio data within the stream.
     *
     *  For local streams that do not have a side channel containing the
     *  layout, the layout can be changed on the fly using this method.
     *  Note: dynamic change of the channel layout on AVB is recommended
     *  to be used only for audio signals using eight channels!
     *
     * @param[in] localStreamId   ID of the local audio stream
     * @param[in] channelLayout   Application specific value indicating layout of audio data within the channel;
     *                            with regard to AVB specification 1722-D16 only the lower 4 bits of the layout argument
     *                            are valid (see 'evt' field in the specification). To enable compatibility mode for
     *                            old draft (which uses 8 bits) add key 'compatibility.audio' option as a registry entry
     *                            to the configuration library.
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult setChannelLayout(uint16_t localStreamId, uint8_t channelLayout) = 0;

    /**
     * @brief Changes parameters of test tone generators
     *
     *  For each channel of each test tone stream, the generated tone can be changed in frequency, amplitude
     *  and waveform. The following wave forms are supported: sine, pulse and sawtooth. For sine waves, userParam
     *  is ignored. For pulse waves, userParam specifies the duty cycle of the "high" phase in % (0-100). For
     *  sawtooth, userParam specifies whether it is falling slope (-1) or rising slope (+1). For file playback,
     *  userParam is an id referencing a pre-configured file (Note: This feature is currently not implemented).
     *
     *  Note that positive values for level are allowed, but will very likely lead to clipping effects.
     *
     * @param[in] localStreamId      ID of the local audio stream
     * @param[in] channel            index of audio channel to modify
     * @param[in] signalFrequency    frequency of the tone to be generated (<=sampleFreq/2)
     * @param[in] level              level/amplitude of the tone in dBFS (0 = full scale, -6 = half, etc.)
     * @param[in] mode               wave form selection (see above)
     * @param[in] userParam          additional param to modify wave generation, depending on mode (see above)
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult setTestToneParams(uint16_t localStreamId, uint16_t channel, uint32_t signalFrequency,
        int32_t level, IasAvbTestToneMode mode, int32_t userParam ) = 0;

    /**
     * @brief Assigns a clock ID to the rate of the specified receive stream.
     *
     *  Creates a clock domain which is maintained on the basis of the stream's
     *  received time stamps, which can then be used to clock other streams. If
     *  a clockId other than 0 is specified, and the clockId is not in use, the
     *  specified clockId will be used. If clockId is 0, an id is generated.
     *
     * @param[in] rxStreamId   ID of receive stream
     * @param[inout] clockId     ID of clock
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult deriveClockDomainFromRxStream(AvbStreamId rxStreamId, uint32_t &clockId) = 0;

    /**
     * @brief Sets parameters for clock recovery
     *
     *  Links a slave clock domain (typically a local clock like a HW time-captured clock) to a master
     *  clock (typically a clock domain defined through a received AVB stream, see @ref deriveClockDomainFromRxStream),
     *  such that deviations between the two rate ratios are passed to the clock driver interface of the stream handler.
     *  A clock driver can then adjust the slave clock to run faster or slower in order to minimize the deviation.
     *
     *  The driverId parameter is passed through to the driver interface, so that multiple instances of clock domains
     *  can be distinguished by the driver, if necessary.
     *
     * @param[in] masterClockId   ID of master clock
     * @param[in] slaveClockId    ID of slave clock
     * @param[in] driverId        ID of driver
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult setClockRecoveryParams(uint32_t masterClockId, uint32_t slaveClockId, uint32_t driverId) = 0;

    /*!
     * @brief retrieves information about all AVB streams currently created.
     *
     * @param[in] audioStreamInfo list of audio
     * @param[in] videoStreamInfo
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult getAvbStreamInfo(AudioStreamInfoList &audioStreamInfo, VideoStreamInfoList &videoStreamInfo, ClockReferenceStreamInfoList &clockRefStreamInfo) = 0;

    /*!
     * @brief retrieves information about all AVB streams currently created.
     *
     * @param[in] audioStreamInfo list of local audio stream attributes
     * @param[in] videoStreamInfo list of local video stream attributes
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult getLocalStreamInfo(LocalAudioStreamInfoList &audioStreamInfo, LocalVideoStreamInfoList &videoStreamInfo) = 0;

    /*!
     * @brief creates a new video stream that transmits data to the network
     *
     * @param[in] srClass Stream reservation class to be used
     * @param[in] maxPacketRate maximum number of packets that will be transmitted per second
     * @param[in] maxPacketSize maximum size of a packet in bytes
     * @param[in] format format of the video stream
     * @param[in] clockId Id of the clock domain to be used by the stream
     * @param[in] assignMode controls the definition of streamId and destination MAC
     * @param[in,out] streamId streamId, if assignMode indicates manual configuration
     * @param[in,out] dmac destination MAC address, if assignMode indicates manual configuration. The 16 most
     *              significant bits are unused.
     * @param[in] active if set to true, stream is activated immediately
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createTransmitVideoStream(IasAvbSrClass srClass,
                                               uint16_t maxPacketRate,
                                               uint16_t maxPacketSize,
                                               IasAvbVideoFormat format,
                                               uint32_t clockId,
                                               IasAvbIdAssignMode assignMode,
                                               uint64_t &streamId,
                                               uint64_t &dmac,
                                               bool active) = 0;

    /*!
     * @brief creates a new video stream that receives data from the network
     *
     * @param[in] srClass Stream reservation class to be used
     * @param[in] callId Unique interface/call identifier
     * @param[in] maxPacketRate maximum number of packets that will be transmitted per second
     * @param[in] maxPacketSize maximum size of a packet in bytes
     * @param[in] format format of the video stream
     * @param[in] streamId ID of the AVB stream
     * @param[in] destMacAddr MAC address to listen on
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createReceiveVideoStream(IasAvbSrClass srClass,
                                              uint16_t maxPacketRate,
                                              uint16_t maxPacketSize,
                                              IasAvbVideoFormat format,
                                              uint64_t streamId,
                                              uint64_t destMacAddr) = 0;

    /*!
     * @brief Creates a local stream that uses a dedicated Video channel to stream video data to applications like GStreamer plugins, etc.
     *
     * @param[in] direction specifies whether this is a transmit or receive stream
     * @param[in] maxPacketRate maximum number of packets that will be transmitted per second
     * @param[in] maxPacketSize maximum size of a packet in bytes
     * @param[in] format video container format to be used
     * @param[in] gStreamerName string identifying the stream to the GStreamer IPC
     * @param[inout] streamId        ID of the local stream
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createLocalVideoStream(IasAvbStreamDirection direction,
                                           uint16_t maxPacketRate,
                                           uint16_t maxPacketSize,
                                           IasAvbVideoFormat format,
                                           const std::string &ipcName,
                                           uint16_t &streamId ) = 0;

    /*!
     * @brief creates a clock reference stream as defined in IEEE1722-rev1 section 11
     *
     * @param[in] srClass             Stream reservation class to be used
     * @param[in] type                type of of the stream as defined in IEEE1722-rev1 11.2.6
     * @param[in] crfStampsPerPdu     number of CRF time stamps to be put into each PDU
     * @param[in] crfStampInterval    number of clock events per time stamp
     * @param[in] baseFreq            base frequency (events per second)
     * @param[in] pull                frequency modifier as defined in IEEE1722-rev1 11.2.8
     * @param[in] clockId             ID of the clock that is used
     * @param[in] assignMode          Specifies how MAC address and stream ID are assigned
     * @param[in,out] streamId        ID of the AVB stream
     * @param[in,out] destMacAddr     MAC address to which the packets will be sent
     * @param[in] active              if true, the stream will be activated immediately (see @ref setStreamActive)
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createTransmitClockReferenceStream(IasAvbSrClass srClass,
                                                IasAvbClockReferenceStreamType type,
                                                uint16_t crfStampsPerPdu,
                                                uint16_t crfStampInterval,
                                                uint32_t baseFreq,
                                                IasAvbClockMultiplier pull,
                                                uint32_t clockId,
                                                IasAvbIdAssignMode assignMode,
                                                uint64_t &streamId,
                                                uint64_t &dmac,
                                                bool active) = 0;

    /*!
     * @brief creates a stream to receive clock reference information.
     *
     * A corresponding clock domain object will be created along. This clock domain can then be used to
     * control transmission of other streams and/or control media clock recovery. If clockId is set to 0,
     * an id value will be generated by the stream handler. Any other clockId value will be used as specified,
     * unless the id is already in use by another clock domain.
     *
     * @param[in] srClass             Stream reservation class to be used
     * @param[in] type                The type of the clock reference stream as defined in IEEE1722-rev1 11.2.6
     * @param[in] maxCrfStampsPerPdu  Maximum number of stamps to be expected in a PDU.
     * @param[in] streamId            ID of the AVB stream
     * @param[in] destMacAddr         MAC address to listen on
     * @param[in, out] clockId        id of the cock domain dynamically created for the stream
     *
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult createReceiveClockReferenceStream(IasAvbSrClass srClass,
                                                IasAvbClockReferenceStreamType type,
                                                uint16_t maxCrfStampsPerPdu,
                                                uint64_t streamId,
                                                uint64_t dmac,
                                                uint32_t &clockId) = 0;
};


} // namespace


#endif // IAS_MEDIATRANSPORT_AVBSTREAMHANDLERINTERFACE_HPP
