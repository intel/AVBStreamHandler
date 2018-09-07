/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file   IasAlsaHandlerWorkerThread.hpp
 * @date   2015
 * @brief  This class implements the worker thread that is applied if the ALSA handler works asynchronously.
 */

#ifndef IASALSAHANDLERWORKERTHREAD_HPP
#define IASALSAHANDLERWORKERTHREAD_HPP

#include <fstream>
#include "avb_helper/IasIRunnable.hpp"
#include "avb_helper/IasThread.hpp"
#include "internal/audio/common/samplerateconverter/IasSrcFarrow.hpp"
#include "internal/audio/common/samplerateconverter/IasSrcController.hpp"

// Forward declararations of classes provided by core_libraries/foundation
//namespace Ias
//{
//  class IasThread;
//}

using namespace IasAudio;
namespace IasMediaTransportAvb {


// Forward declararations of classes
class IasEventProvider;
//class IasSrcFarrow;
//class IasSrcController;


// Type definiton for shared pointers to IasAlsaHandlerWorkerThread objects.
class IasAlsaHandlerWorkerThread;
using IasAlsaHandlerWorkerThreadPtr = std::shared_ptr<IasAlsaHandlerWorkerThread>;

/**
 * @brief
 */
class /*IAS_DSO_PUBLIC*/ IasAlsaHandlerWorkerThread : public IasMediaTransportAvb::IasIRunnable
{
  public:

    /**
     * @brief Struct containing the parameters of one buffer (deviceBuffer or asrcBuffer).
     */
    struct IasAudioBufferParams
    {
      IasAudioBufferParams()
        :ringBuffer(nullptr)
        ,numChannels(0)
        ,dataFormat(eIasFormatUndef)
        ,periodSize(0)
        ,numPeriods(0)
      {}
      IasAudioBufferParams(IasAudioRingBuffer*      p_ringBuffer,
                           uint32_t                 p_numChannels,
                           IasAudioCommonDataFormat p_dataFormat,
                           uint32_t                 p_periodSize,
                           uint32_t                 p_numPeriods)
        :ringBuffer(p_ringBuffer)
        ,numChannels(p_numChannels)
        ,dataFormat(p_dataFormat)
        ,periodSize(p_periodSize)
        ,numPeriods(p_numPeriods)
      {}
      IasAudioRingBuffer*      ringBuffer;  //!< Handle of the ring buffer
      uint32_t                 numChannels; //!< Number of channels
      IasAudioCommonDataFormat dataFormat;  //!< The data format, see IasAudioCommonDataFormat for details
      uint32_t                 periodSize;  //!< The period size in frames
      uint32_t                 numPeriods;  //!< The number of periods that the ring buffer consists of
    };

    /**
     * @brief Struct containing the initialization parameters of the IasAlsaHandlerWorkerThread class.
     */
    struct IasAlsaHandlerWorkerThreadParams
    {
      IasAlsaHandlerWorkerThreadParams()
        :name()
        ,samplerate(0)
        ,deviceBufferParams()
        ,asrcBufferParams()
      {}
      IasAlsaHandlerWorkerThreadParams(string               p_name,
                                       uint32_t             p_samplerate,
                                       IasAudioBufferParams p_deviceBufferParams,
                                       IasAudioBufferParams p_asrcBufferParams)
        :name(p_name)
        ,samplerate(p_samplerate)
        ,deviceBufferParams(p_deviceBufferParams)
        ,asrcBufferParams(p_asrcBufferParams)
      {}
      string               name;               // Name of the ALSA device
      uint32_t             samplerate;         // Sample rate, expressed in Hz
      IasAudioBufferParams deviceBufferParams; // Parameters of the deviceBuffer
      IasAudioBufferParams asrcBufferParams;   // Parameters of the asrcBuffer
    };

    using IasAlsaHandlerWorkerThreadParamsPtr = std::shared_ptr<IasAlsaHandlerWorkerThreadParams>;

    /**
     * @brief  Result type of the class IasAlsaHandlerWorkerThread.
     */
    enum IasResult
    {
      eIasOk,               //!< Ok, Operation successful
      eIasInvalidParam,     //!< Invalid parameter, e.g., out of range or NULL pointer
      eIasInitFailed,       //!< Initialization of the component failed
      eIasNotInitialized,   //!< Component has not been initialized appropriately
      eIasFailed,           //!< other error
    };


    /**
     * @brief Constructor.
     *
     * @param[in] params  Initialization parameters
     */
    IasAlsaHandlerWorkerThread(IasAlsaHandlerWorkerThreadParamsPtr params);

    /**
     * @brief Destructor.
     *
     * Class is not intended to be subclassed.
     */
    ~IasAlsaHandlerWorkerThread();

    /**
     * @brief Initialization.
     *
     * @param[in] deviceType      Device type, either eIasDeviceTypeSource or eIasDeviceTypeSink
     * @param[in] avbSampleRate   Sample Rate for AVB Side 48 or 24 kHz
     */
    IasResult init(IasDeviceType deviceType, uint32_t avbSampleRate);

    /**
     * @brief Start the ALSA handler worker thread.
     * @return Status of the method.
     */
    IasResult start();

    /**
     * @brief Stop the ALSA handler worker thread.
     * @return Status of the method.
     */
    IasResult stop();

    /**
     * @brief Called once before starting the worker thread.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasMediaTransportAvb::IasResult beforeRun();

    /**
     * @brief The main worker thread function.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasMediaTransportAvb::IasResult run();

    /**
     * @brief Called to initiate the worker thread shutdown.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasMediaTransportAvb::IasResult shutDown();

    /**
     * @brief Called once after worker thread finished executing.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasMediaTransportAvb::IasResult afterRun();

    /**
     * @brief Reset the states of the ALSA handler worker thread
     *
     * This is mainly used to reset the sample rate converter states inside the ALSA handler worker thread.
     * It can be enhanced to do further resets of internal states if required.
     */
    void reset();


  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAlsaHandlerWorkerThread(IasAlsaHandlerWorkerThread const &other);
    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAlsaHandlerWorkerThread& operator=(IasAlsaHandlerWorkerThread const &other);


    /**
     * @brief Transfer PCM frames from asrcBuffer to deviceBuffer or vice versa.
     *
     * Depending on deviceType, this method copies PCM frames
     *
     * @li from from asrcBuffer to deviceBuffer (if deviceType is eIasDeviceTypeSink)
     * @li or from deviceBuffer to asrcBuffer (if deviceType is eIasDeviceTypeSource).
     *
     * @param[in]  deviceBufferAreas                 Memory areas of the device buffer
     * @param[in]  deviceBufferOffset                Offset of the device buffer
     * @param[in]  deviceBufferNumFrames             Number of frames available in the device buffer
     * @param[in]  asrcBufferAreas                   Memory areas of the device buffer
     * @param[in]  asrcBufferOffset                  Offset of the ASRC buffer
     * @param[in]  asrcBufferNumFrames               Number of frames available in the ASRC buffer
     * @param[in]  dataFormat                        Data format used for representing PCM samples
     * @param[in]  numChannels                       Number of channels to transfer
     * @param[in]  ratioAdaptive                     Adaptive conversion ratio
     * @param[in]  deviceType                        Device type: eIasDeviceTypeSource or eIasDeviceTypeSink
     * @param[out] deviceBufferNumFramesTransferred  Number of frames transferred from/to the device buffer
     * @param[out] asrcBufferNumFramesTransferred    Number of frames transferred from/to the ASRC buffer
     */
    void transferFrames(IasAudioArea const       *deviceBufferAreas,
                        uint32_t                  deviceBufferOffset,
                        uint32_t                  deviceBufferNumFrames,
                        IasAudioArea const       *asrcBufferAreas,
                        uint32_t                  asrcBufferOffset,
                        uint32_t                  asrcBufferNumFrames,
                        IasAudioCommonDataFormat  dataFormat,
                        uint32_t                  numChannels,
                        float                     ratioAdaptive,
                        IasDeviceType             deviceType,
                        uint32_t                 *deviceBufferNumFramesTransferred,
                        uint32_t                 *asrcBufferNumFramesTransferred);


    /**
     * @brief Adjust frames that are stored in the ring buffer.
     *
     * Depending on the parameter @a bufferAccessType, this function
     * either skips frames from the buffer (if @a bufferAccessType is eIasRingBufferAccessRead)
     * or inserts zero-valued frames into the buffer (if @a bufferAccessType is eIasRingBufferAccessWrite).
     *
     * This private method is used by the run() method in order to adjust the fill level of the
     * ASRC buffer at the end of the start-up phase.
     *
     * @param[in] bufferHandle      Handle for the ring buffer, which shall be adjusted.
     * @param[in] bufferAccessType  Access type used by the buffer: eIasRingBufferAccessRead or eIasRingBufferAccessWrite.
     * @param[in] bufferDataFormat  Data format used by the buffer to represent the PCM frames.
     * @param[in] numFramesToSkip   Number of frames that shall be skipped / inserted.
     * @param[in] numChannels       Number of channels that are transmitted by the buffer.
     */
    void bufferAdjustFrames(IasAudioRingBuffer*      bufferHandle,
                            IasRingBufferAccess      bufferAccessType,
                            IasAudioCommonDataFormat bufferDataFormat,
                            uint32_t                 numFramesToAdjust,
                            uint32_t                 numChannels);


    DltContext                          *mLog;                     //!< Handle for the DLT log object
    IasAlsaHandlerWorkerThreadParamsPtr  mParams;                  //!< Shared pointer to the initialization parameters of the object
    IasDeviceType                        mDeviceType;              //!< Device type: eIasDeviceTypeSource or eIasDeviceTypeSink
    uint32_t                             mSamplerate;              //!< Sample rate of the ALSA device, expressed in Hz
    uint32_t                             mSamplerateIn;            //!< Sample rate of the ALSA device input, expressed in Hz
    uint32_t                             mSamplerateOut;           //!< Sample rate of the ALSA device output, expressed in Hz
    uint32_t                             mNumChannels;             //!< Number of channels
    IasMediaTransportAvb::IasThread     *mThread;                  //!< Handle for the thread
    bool                                 mThreadIsRunning;         //!< Flag indicating whether thread is running
    IasSrcFarrow                        *mSrc;                     //!< Handle for the sample rate converter
    IasSrcController                    *mSrcController;           //!< Handle for the ASRC closed loop controller
    float const                        **mSrcInputBuffersFloat32;  //!< Vector with pointers to the SRC input buffers (Float32)
    int32_t   const                    **mSrcInputBuffersInt32;    //!< Vector with pointers to the SRC input buffers (Int32)
    int16_t   const                    **mSrcInputBuffersInt16;    //!< Vector with pointers to the SRC input buffers (Int16)
    float                              **mSrcOutputBuffersFloat32; //!< Vector with pointers to the SRC output buffers (Float32)
    int32_t                            **mSrcOutputBuffersInt32;   //!< Vector with pointers to the SRC output buffers (Int32)
    int16_t                            **mSrcOutputBuffersInt16;   //!< Vector with pointers to the SRC output buffers (Int16)
    string                               mDiagnosticsFileName;     //!< File name for saving AlsaHandler/ASRC diagnostics
    std::ofstream                        mDiagnosticsStream;       //!< Output stream for saving AlsaHandler/ASRC diagnostics
    uint32_t                             mLogCnt;                  //!< Log counter to control the amount of timeout messages
    uint32_t                             mLogInterval;             //!< The log interval to provide important logs that should not "spam" everything
};


/**
 * @brief Function to get a IasAlsaHandlerWorkerThread::IasResult as string.
 *
 * @return String carrying the result message.
 */
std::string toString(const IasAlsaHandlerWorkerThread::IasResult& type);



} //namespace IasMediaTransportAvb

#endif // IASALSAHANDLERWORKERTHREAD_HPP
