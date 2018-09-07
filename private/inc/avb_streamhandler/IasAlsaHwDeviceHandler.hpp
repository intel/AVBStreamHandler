/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file   IasAlsaHwDeviceHandler.hpp
 * @date   2018
 * @brief
 */

#ifndef IASALSAHWDEVICEHANDLER_HPP_
#define IASALSAHWDEVICEHANDLER_HPP_

#include "audio/common/IasAudioCommonTypes.hpp"
#include "avb_streamhandler/IasAlsaHandlerWorkerThread.hpp"
#include "avb_streamhandler/IasAlsaStreamInterface.hpp"
#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "avb_streamhandler/IasAvbAudioShmProvider.hpp"



// Forward declarations (usually defined in alsa/asoundlib.h).
typedef struct _snd_pcm    snd_pcm_t;
typedef struct _snd_output snd_output_t;

using namespace IasAudio;
namespace IasMediaTransportAvb {


class IasAudioRingBuffer;

class /* IAS_DSO_PUBLIC */ IasAlsaHwDeviceHandler : public IasLocalAudioStream, virtual public IasAlsaStreamInterface
{
  public:
    /**
     * @brief  Result type of the class IasAlsaHandler.
     */
    enum IasResult
    {
      eIasOk                = 0,   //!< Ok, Operation successful
      eIasInvalidParam      = 1,   //!< Invalid parameter, e.g., out of range or NULL pointer
      eIasInitFailed        = 2,   //!< Initialization of the component failed
      eIasNotInitialized    = 3,   //!< Component has not been initialized appropriately
      eIasAlsaError         = 4,   //!< Error reported by the ALSA device
      eIasTimeOut           = 5,   //!< Time out occured
      eIasRingBufferError   = 6,   //!< Error reported by the ring buffer
      eIasFailed            = 7,   //!< Other error (operation failed)
    };

    /**
     * @brief  Type definition of the direction in which the ALSA handler operates.
     */
    enum IasDirection
    {
      eIasDirectionUndef    = 0,   //!< Undefined
      eIasDirectionCapture  = 1,   //!< Capture direction
      eIasDirectionPlayback = 2,   //!< Playback direction
    };

    /**
     * @brief Constructor.
     *
     * @param[in] params  Pointer (shared pointer) to the audio device configuration.
     */
    IasAlsaHwDeviceHandler(DltContext &dltContext, IasAvbStreamDirection direction, uint16_t streamId, IasAudioDeviceParamsPtr params);


    /**
     * @brief Destructor, virtual by default.
     */
    virtual ~IasAlsaHwDeviceHandler();


    /**
     * @brief Initialization function.
     *
     * @param[in] deviceType The device type of the created audio device to which the ALSA handler belongs
     *
     * @returns IasAlsaHandler::IasResult::eIasOk on success, otherwise an error code.
     */
    IasResult initHandler(IasDeviceType deviceType);

    /**
     * @brief Start the ALSA handler.
     *
     * @returns IasAlsaHandler::IasResult::eIasOk on success, otherwise an error code.
     */
    IasResult start();

    /**
     * @brief Stop the ALSA handler.
     */
    void stop();

    /**
     * @brief Get a handle to the ring buffer used by the AlsaHandler.
     *
     * @returns    IasAlsaHandler::IasResult::eIasOk on success, otherwise an error code.
     * @param[out] ringBuffer  Returns the handle to the ring buffer.
     */
    IasResult getRingBuffer(IasAudio::IasAudioRingBuffer **ringBuffer) const;

    /**
     * @brief Get a the period size used by the AlsaHandler.
     *
     * @returns    IasAlsaHandler::IasResult::eIasOk on success, otherwise an error code.
     * @param[out] periodSize  Returns the period size.
     */
    IasResult getPeriodSize(uint32_t *periodSize) const;

    /**
     * @brief Let the ALSA handler operate in non-blocking mode.
     *
     * In non-blocking mode, the method updateAvailable() does not wait for the output buffer
     * to provide enough free PCM frames for one period. The default behavior of the ALSA
     * handler (if this function is not called) is the blocking mode.
     *
     * @returns                 IasAlsaHandler::IasResult::eIasOk on success, otherwise an error code.
     *
     * @param[in] isNonBlocking Activates (if true) or deactivates (if false) the non-blocking mode.
     */
    IasResult setNonBlockMode(bool isNonBlocking);

    /**
     * @brief Reset the states of the ALSA handler
     *
     * This is mainly used to reset the sample rate converter states inside the ALSA handler.
     * It can be enhanced to do further resets of internal states if required.
     */
    void reset();


    // Inherited from interface (all virtual functions)

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
     * @brief Transfers audio data from local audio buffer to shared memory or vice versa.
     *
     * This shared memory is accessed from by the Alsa HW Handler which contains ASRC an connects to
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
    virtual IasAvbProcessingResult setDeviceType(IasDeviceType deviceType);

    virtual IasLocalAudioStreamDiagnostics* getDiag();
    virtual IasAlsaDeviceTypes getAlsaDeviceType() const;


  private:

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAlsaHwDeviceHandler(IasAlsaHwDeviceHandler const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAlsaHwDeviceHandler& operator=(IasAlsaHwDeviceHandler const &other);

    /**
     * @brief  Set the hardware parameters of an ASLA PCM device.
     *
     * @param[in,out] pcmHandle            Handle of the ALSA PCM device
     * @param[in]     format               Format of the PCM samples
     * @param[in]     rate                 Sample rate
     * @param[in]     channels             Number of channels
     * @param[in]     numPeriods           Number of periods that shall be buffered
     * @param[in]     period_size          Length of each period, expressed in PCM frames
     * @param[out]    actualBufferSize     Returned actual buffer size
     * @param[out]    actualPeriodSize     Returned actual period length
     * @param[in,out] sndLogger            Handle to the debug output for ALSA
     */
    int set_hwparams(snd_pcm_t                *pcmHandle,
                     IasAudioCommonDataFormat  dataFormat,
                     unsigned int              rate,
                     unsigned int              channels,
                     unsigned int              numPeriods,
                     unsigned int              period_size,
                     int32_t                  *actualBufferSize,
                     int32_t                  *actualPeriodSize);

    int set_swparams(snd_pcm_t   *pcmHandle,
                     int32_t      bufferSize,
                     int32_t      periodSize);


    IasAudioDeviceParamsPtr       mParams;             //!< Pointer to device params
    IasDeviceType                 mDeviceType;         //!< Device type: eIasDeviceTypeSource or eIasDeviceTypeSink
    IasAudio::IasAudioRingBuffer *mRingBuffer;         //!< Handle of the ring buffer
    IasAudio::IasAudioRingBuffer *mRingBufferAsrc;     //!< Handle of the ring buffer (jitter buffer for ASRC)
    snd_pcm_t                    *mAlsaHandle;         //!< Handle of the ALSA device
    uint32_t                      mBufferSize;         //!< Total buffer size (i.e., mPeriodSize * mParams->numPeriods)
    uint32_t                      mPeriodSize;         //!< Period size, expressed in PCM frames
    uint32_t                      mPeriodTime;         //!< Period time, expressed in microseconds
    snd_output_t                 *mSndLogger;          //!< Handle of the debug output for ALSA
    int32_t                       mTimeout;            //!< Timeout [ms] for snd_pcm_wait, or -1 for infinity
    uint32_t                      mTimevalUSecLast;    //!< Time stamp [us] when last call of updateAvailable() was completed
    bool                          mIsAsynchronous;     //!< true, if asynchronous sample rate conversion needs to be performed
    IasAlsaHandlerWorkerThreadPtr mWorkerThread;       //!< Handle for the workerthread object
    uint32_t                      mOptimalFillLevel;   //!< FillLevel = WritePointer - ReadPointer
    IasLocalAudioBufferDesc::AudioBufferDescMode           mDescMode;
    IasAlsaDeviceTypes            mAlsaDeviceType;
    uint32_t                      mLastPtpEpoch;
    uint32_t                      mSampleFreq;         //!< nominal Sample frequency of AVB Side in Hz
};


/**
 * @brief Function to get a IasAlsaHandler::IasResult as string.
 *
 * @return String carrying the result message.
 */
std::string toString(const IasAlsaHwDeviceHandler::IasResult& type);


// Inherited from IasAlasStreamInterface class
inline uint32_t IasAlsaHwDeviceHandler::getPeriodSize() const
{
  return mPeriodSize;
}


inline IasAlsaDeviceTypes IasAlsaHwDeviceHandler::getAlsaDeviceType() const
{
  return mAlsaDeviceType;
}


inline uint32_t IasAlsaHwDeviceHandler::getNumPeriods() const
{
    return mParams->numPeriods;
}


inline IasLocalAudioStreamDiagnostics* IasAlsaHwDeviceHandler::getDiag()
{
  return IasLocalAudioStream::getDiagnostics();
}


inline const std::string *IasAlsaHwDeviceHandler::getDeviceName() const
{
  return &mParams->name;
}


inline void IasAlsaHwDeviceHandler::setCycle(uint32_t cycle)
{
  (void)cycle;
}


inline bool IasAlsaHwDeviceHandler::nextCycle(uint32_t cycle)
{
  (void)cycle;
  return false;
}


inline void IasAlsaHwDeviceHandler::setWorkerActive(bool active)
{
  IasLocalAudioStream::setWorkerActive(active);
}


inline uint16_t IasAlsaHwDeviceHandler::getStreamId() const
{
  return IasLocalAudioStream::getStreamId();
}


inline void IasAlsaHwDeviceHandler::dump(uint8_t *data, uint32_t len)
{
  (void) data;
  (void) len;
}


inline bool IasAlsaHwDeviceHandler::isConnected() const
{
  return IasLocalAudioStream::isConnected();
}


inline bool IasAlsaHwDeviceHandler::isReadReady() const
{
  return IasLocalAudioStream::isReadReady();
}


inline uint32_t IasAlsaHwDeviceHandler::getSampleFrequency() const
{
  return IasLocalAudioStream::getSampleFrequency();
}


inline uint64_t IasAlsaHwDeviceHandler::getCurrentTimestamp()
{
  return IasLocalAudioStream::getCurrentTimestamp();
}


inline const std::vector<IasLocalAudioBuffer*> & IasAlsaHwDeviceHandler::getChannelBuffers() const
{
  return IasLocalAudioStream::getChannelBuffers();
}


inline IasLocalAudioBufferDesc * IasAlsaHwDeviceHandler::getBufferDescQ() const
{
  return IasLocalAudioStream::getBufferDescQ();
}


} //namespace IasMediaTransportAvb

#endif /* IASALSAHWDEVICEHANDLER_HPP_ */
