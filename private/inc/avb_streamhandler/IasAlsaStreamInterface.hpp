/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaStream.hpp
 * @brief   This class is a pure virtual interface to handle all methods of an Alsa stream
 * @details An Alsa stream is the audio data container for audio received from Alsa plugin or Alsa HW Device.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSASTREAMINTERFACE_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSASTREAMINTERFACE_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "IasAvbAudioShmProvider.hpp"

namespace IasMediaTransportAvb {

class /*IAS_DSO_PUBLIC*/ IasAlsaStreamInterface
{
  public:

    /**
     *  @brief Constructor.
     */
    // no constructor needed - it is an pure virtual interface - constructor in derived classes implemented


    /**
     *  @brief Destructor, pure virtual by default.
     */
    //virtual ~IasAlsaStreamInterface() = 0;


    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *  Derived from base class.
     */
    virtual IasAvbProcessingResult init(uint16_t numChannels, uint32_t totalLocalBufferSize, uint32_t optimalFillLevel,
                                uint32_t alsaPeriodSize, uint32_t numAlsaPeriods, uint32_t alsaSampleFrequency,
                                IasAvbAudioFormat format, uint8_t channelLayout, bool hasSideChannel,
                                std::string deviceName, IasAlsaDeviceTypes alsaDeviceType) = 0;


    /**
     *  @brief Clean up all allocated resources.
     */
    virtual void cleanup() = 0;


    /**
     *  @brief Resets the buffers.
     */
    virtual IasAvbProcessingResult resetBuffers() = 0;


    /**
     *  @brief Inform registered client about buffer status changes.
     */
    virtual void updateBufferStatus() = 0;


    /**
     * @brief Transfers auido data from local audio buffer to shared memory or vice versa.
     *
     * This shared memory is accessed from client side by the Alsa plugin which is used by an
     * Alsa playback or capture device.
     */
    virtual void copyJob(uint64_t timestamp) = 0;


    /**
     *  @brief Returns the Alsa period size used with this stream.
     */
    virtual uint32_t getPeriodSize() const = 0;


    /**
      *  @brief Returns the Alsa number of periods used with this stream.
      */
    virtual uint32_t getNumPeriods() const = 0;


    /**
     *  @brief Returns the name of the ALSA device that is being created by the stream.
     */
    virtual const std::string *getDeviceName() const = 0;


    /**
     *  @brief Used by worker thread to set cycle load value
     */
    virtual void setCycle(uint32_t cycle) = 0;


    /**
     *  @brief Used by worker thread to count down cycle and reload
     *
     *  returns true if next service is due
     */
    virtual bool nextCycle(uint32_t cycle) = 0;


    virtual void dump(uint8_t *data, uint32_t len) = 0;
    virtual void setWorkerActive(bool active) = 0;
    virtual bool isConnected() const = 0;
    virtual bool isReadReady() const = 0;
    virtual uint32_t getSampleFrequency() const = 0;
    virtual uint16_t getStreamId() const = 0;
    virtual IasAvbProcessingResult writeLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer,
                                                         uint32_t bufferSize, uint16_t &samplesWritten, uint32_t timestamp) = 0;
    virtual uint64_t getCurrentTimestamp() = 0;
    virtual const std::vector<IasLocalAudioBuffer*> & getChannelBuffers() const = 0;
    virtual IasLocalAudioBufferDesc * getBufferDescQ() const = 0;

    virtual IasLocalAudioStreamDiagnostics* getDiag() = 0;

    /**
     *  @brief returns Alsa Device Type IasAlsaDeviceTypes
     *
     *  returns eIasAlsaVirtualDevice = 0, eIasAlsaHwDevice = 1, eIasAlsaHwDeviceAsync = 2,
     */
    virtual IasAlsaDeviceTypes getAlsaDeviceType() const = 0;



  protected:
    //@{
    /// can only be created and destroyed through implementation class
    IasAlsaStreamInterface() {}
    virtual ~IasAlsaStreamInterface() {}
    //@}
};


//IasAlsaStreamInterface::~IasAlsaStreamInterface() {}

} // namespace IasMediaTransportAvb


#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSASTREAMINTERFACE_HPP */
