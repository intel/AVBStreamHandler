/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaVirtualDeviceStream.hpp
 * @brief   This class contains all methods to handle an Alsa stream
 * @details An Alsa stream is the audio data container for audio received from Alsa plugin.
 *          This class is derived from IasLocalAudioStream and uses IasAlsaStream as interface class
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAVIRTUALDEVICESTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAVIRTUALDEVICESTREAM_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasAlsaStreamInterface.hpp"
#include "avb_streamhandler/IasAvbAudioShmProvider.hpp"


namespace IasMediaTransportAvb {


class /*IAS_DSO_PUBLIC*/ IasAlsaVirtualDeviceStream : public IasLocalAudioStream, virtual public IasAlsaStreamInterface
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAlsaVirtualDeviceStream(DltContext &dltContext, IasAvbStreamDirection direction, uint16_t streamId);


    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAlsaVirtualDeviceStream();


    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *  Derived from base class.
     */
    virtual IasAvbProcessingResult init(uint16_t numChannels, uint32_t totalLocalBufferSize, uint32_t optimalFillLevel,
                                        uint32_t alsaPeriodSize, uint32_t numAlsaPeriods, uint32_t alsaSampleFrequency,
                                        IasAvbAudioFormat format, uint8_t channelLayout, bool hasSideChannel,
                                        std::string deviceName, IasAlsaDeviceTypes alsaDeviceType);


    /**
     *  @brief Clean up all allocated resources.
     */
    virtual void cleanup();


    /**
     *  @brief Resets the buffers.
     */
    virtual IasAvbProcessingResult resetBuffers();


    /**
     *  @brief Inform registered client about buffer status changes.
     */
    virtual void updateBufferStatus();


    /**
     * @brief Transfers auido data from local audio buffer to shared memory or vice versa.
     *
     * This shared memory is accessed from client side by the Alsa plugin which is used by an
     * Alsa playback or capture device.
     */
     virtual inline void copyJob(uint64_t timestamp);


    /**
     *  @brief Returns the Alsa period size used with this stream.
     */
    virtual inline uint32_t getPeriodSize() const;


    /**
     *  @brief Returns the Alsa number of periods used with this stream.
     */
    virtual inline uint32_t getNumPeriods() const;


    /**
     *  @brief Returns the name of the ALSA device that is being created by the stream.
     */
    virtual inline const std::string *getDeviceName() const;


    /**
     *  @brief Used by worker thread to set cycle load value
     */
    virtual inline void setCycle(uint32_t cycle);


    /**
     *  @brief Used by worker thread to count down cycle and reload
     *
     *  returns true if next service is due
     */
    virtual inline bool nextCycle(uint32_t cycle);

    virtual void dump(uint8_t *data, uint32_t len);

    virtual bool isConnected() const;
    virtual bool isReadReady() const;
    virtual uint32_t getSampleFrequency() const;
    virtual uint16_t getStreamId() const;
    virtual void setWorkerActive(bool active);
    virtual IasAvbProcessingResult writeLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer,
                                                         uint32_t bufferSize, uint16_t &samplesWritten, uint32_t timestamp);
    virtual uint64_t getCurrentTimestamp();
    virtual const std::vector<IasLocalAudioBuffer*> & getChannelBuffers() const;
    virtual IasLocalAudioBufferDesc * getBufferDescQ() const;
    virtual IasLocalAudioStreamDiagnostics* getDiag();
    virtual IasAlsaDeviceTypes getAlsaDeviceType() const;


  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAlsaVirtualDeviceStream(IasAlsaVirtualDeviceStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAlsaVirtualDeviceStream& operator=(IasAlsaVirtualDeviceStream const &other);


    ///
    /// Members
    ///
    IasAvbAudioShmProvider *mShm;     // Pointer to the shared memory provider object
    uint32_t mOptimalFillLevel;       // FillLevel = WritePointer - ReadPointer
    uint32_t mPeriodSize;
    uint32_t mNumAlsaPeriods;
    uint32_t mCycle;
    uint32_t mCycleCount;
    IasAlsaDeviceTypes mAlsaDeviceType;
};


inline IasAlsaDeviceTypes IasAlsaVirtualDeviceStream::getAlsaDeviceType() const
{
  return mAlsaDeviceType;
}


inline void IasAlsaVirtualDeviceStream::copyJob(uint64_t timestamp)
{
  if ((NULL != mShm) && (0u != mCycle))
  {
    mShm->copyJob(getChannelBuffers(), getBufferDescQ(), mPeriodSize / mCycle, !isConnected(), timestamp);
  }
}


inline uint32_t IasAlsaVirtualDeviceStream::getPeriodSize() const
{
  return mPeriodSize;
}


inline uint32_t IasAlsaVirtualDeviceStream::getNumPeriods() const
{
    return mNumAlsaPeriods;
}


inline const std::string *IasAlsaVirtualDeviceStream::getDeviceName() const
{
  if (NULL != mShm)
  {
    return &mShm->getDeviceName();
  }
  return (NULL);
}


inline void IasAlsaVirtualDeviceStream::setCycle(uint32_t cycle)
{
  mCycle = cycle;
}


inline bool IasAlsaVirtualDeviceStream::nextCycle(uint32_t cycle)
{
  return (0u == (cycle % mCycle));
}


inline IasLocalAudioStreamDiagnostics* IasAlsaVirtualDeviceStream::getDiag()
{
  return IasLocalAudioStream::getDiagnostics();
}


inline void IasAlsaVirtualDeviceStream::setWorkerActive(bool active)
{
  IasLocalAudioStream::setWorkerActive(active);
}


//  Inherited from IasAlsaStreamInterface class
inline uint16_t IasAlsaVirtualDeviceStream::getStreamId() const
{
  return IasLocalAudioStream::getStreamId();
}


inline void IasAlsaVirtualDeviceStream::dump(uint8_t *data, uint32_t len)
{
  (void) data;
  (void) len;
}


inline bool IasAlsaVirtualDeviceStream::isConnected() const
{
  return IasLocalAudioStream::isConnected();
}


inline bool IasAlsaVirtualDeviceStream::isReadReady() const
{
  return IasLocalAudioStream::isReadReady();
}


inline uint32_t IasAlsaVirtualDeviceStream::getSampleFrequency() const
{
  return IasLocalAudioStream::getSampleFrequency();
}

inline uint64_t IasAlsaVirtualDeviceStream::getCurrentTimestamp()
{
  return IasLocalAudioStream::getCurrentTimestamp();
}


inline const std::vector<IasLocalAudioBuffer*> & IasAlsaVirtualDeviceStream::getChannelBuffers() const
{
  return IasLocalAudioStream::getChannelBuffers();
}


inline IasLocalAudioBufferDesc * IasAlsaVirtualDeviceStream::getBufferDescQ() const
{
  return IasLocalAudioStream::getBufferDescQ();
}


} // namespace IasMediaTransportAvb


#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAVIRTUALDEVICESTREAM_HPP */
