/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestToneStream.hpp
 * @brief   Test tone generator pseudo-stream class
 * @details This class generates audio streams with test tones.
 *          This class is derived from IasLocalAudioStream
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_TESTTONESTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_TESTTONESTREAM_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerInterface.hpp"

namespace IasMediaTransportAvb {


class IasTestToneStream : public IasLocalAudioStream
{
  public:

    /**
     *  @brief Constructor.
     */
    IasTestToneStream(DltContext &dltContext, uint16_t streamId);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasTestToneStream();

    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameters.
     */
    IasAvbProcessingResult init(uint16_t numChannels, uint32_t sampleFrequency, uint8_t channelLayout);

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief set tone generation params per channel
     */
    IasAvbProcessingResult setChannelParams(uint16_t channel,
        uint32_t signalFrequency,
        int32_t level,
        IasAvbTestToneMode mode,
        int32_t userParam
        );

  private:
    typedef float AudioData;

    struct GeneratorParams
    {
      GeneratorParams();

      IasAvbTestToneMode mode;
      int32_t userParam;
      uint32_t signalFreq;
      int32_t level;
      AudioData peak;
      AudioData coeff;
      AudioData buf1;
      AudioData buf2;
    };

    typedef uint32_t (IasTestToneStream::*ProcessMethod)(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params);

    struct ChannelData
    {
        GeneratorParams params;
        ProcessMethod method;
    };

    typedef std::vector<ChannelData> ChannelVec;


    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasTestToneStream(IasTestToneStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasTestToneStream& operator=(IasTestToneStream const &other);

    /**
     * @brief overwritten version of base class implementation, only as debug safeguard
     */
    virtual IasAvbProcessingResult writeLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer, uint32_t bufferSize, uint16_t &samplesWritten, uint32_t timeStamp);

    /**
     * @brief overwritten version of base class implementation
     */
    virtual IasAvbProcessingResult readLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer, uint32_t bufferSize, uint16_t &samplesRead, uint64_t &timeStamp);


    /**
     * @brief must be implemented by each class derived from IasLocalStream
     * our implementation does nothing
     */
    IasAvbProcessingResult resetBuffers() { return eIasAvbProcOK; }

    ///@brief helper for coeff calculation
    void calcCoeff(uint32_t sampleFreq, GeneratorParams & params);

    ///@brief helper for setting the right process method
    void setProcessMethod(ChannelData & ch);

    inline int16_t convertFloatToPcm16(AudioData val);

    uint32_t generateSineWave(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params);
    uint32_t generatePulseWave(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params);
    uint32_t generateSawtoothWaveRising(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params);
    uint32_t generateSawtoothWaveFalling(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params);


    ///
    /// Members
    ///

    ChannelVec mChannels;
    AudioData  mConversionGain;
    bool       mUseSaturation;
};


} // namespace IasMediaTransportAvb


#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_TESTTONESTREAM_HPP */
