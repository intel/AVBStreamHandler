/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbAudioShmProvider.hpp
 * @brief   This class uses the ringbuffer concept of the SmartXBar.
 * @details This concept supports buffers located in various memory sections (heap, shared, ..). By using this
 *          type of SHM ringbuffer it is possible to communicate with the same Alsa plugin that has originally been
 *          created for SmartXbar.
 *
 * @date    2016
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAUDIOSHMPROVIDER_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAUDIOSHMPROVIDER_HPP

#include <thread>
#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalAudioStream.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "audio/common/IasAudioCommonTypes.hpp"
#include "internal/audio/common/alsa_smartx_plugin/IasAlsaPluginShmConnection.hpp"

namespace IasMediaTransportAvb {

class IasAvbAudioShmProvider
{
  public:
    typedef int16_t AudioData;

    enum IasResult
    {
      eIasOk,                     //!< Operation successful
      eIasFailed,                 //!< Operation failed
    };

    /**
     * This state indicates whether an Alsa device is waiting for a client
     * accessing data in shared memory (eIdle), or a client is transferring data (eRunning),
     * or it is in the initial pre-fill state (ePrefilling).
     */
    enum bufferState
    {
      eIdle,
      ePrefilling,         //!< Pre-filling the buffer
      eRunning             //!< Data transfer is ongoing
    };


    /**
     * @brief Constructor.
     */
    IasAvbAudioShmProvider(const std::string & deviceName);

    /**
     * @brief Destructor.
     *
     * Class is not intended to be subclassed.
     */
    ~IasAvbAudioShmProvider();

    /**
     * @brief The init method.
     *
     * @param[in] numChannels number of channels
     * @param[in] alsaPeriodSize the ALSA period size (the size of the buffer in Alsa frames)
     * @param[in] numPeriods number of periods that the ring buffer consists of
     * @param[in] sampleRate the sample rate in Hz, e.g. 48000
     * @param[in] dirWriteToShm direction of the data flow (true for write to shm, false for read from shm)
     * @returns eIasAvbProcOK upon success, an error code otherwise
     */
    IasAvbProcessingResult init(uint16_t numChannels, uint32_t alsaPeriodSize, uint32_t numPeriods, uint32_t sampleRate,
                                bool dirWriteToShm);

    /**
     * @brief Transfers the data from local audio buffer to shared memory or vice versa.
     *
     * @param[in] buffers Reference to a vector containing the local audio buffers per channel.
     * @param[in] descriptors Reference to a vector containing the timestamps for the local audio buffers per channel.
     * @param[in] numFrames number of frames (samples) that shall be processed. (periodSize % numFrames) must be 0.
     * @param[in] dummy if true, does a dummy call that only moves the read/write pointer but does not transfer data
     * @param[in] timestamp to be enqueued to the descriptors
     * @returns eIasAvbProcOK upon success, an error code otherwise
     */
    IasAvbProcessingResult copyJob(const IasLocalAudioStream::LocalAudioBufferVec & buffers,
                                   IasLocalAudioBufferDesc * descQ, uint32_t numFrames, bool dummy, uint64_t timestamp);

    /**
     * @brief This function is used to notify the client abort the communication. Not used at the moment!
     */
    void abortTransmission();

    /**
     * @brief Returns the name of the shared memory which is also the name of the ALSA device being created.
     *
     * @returns the device name of the shared memory.
     */
    inline const std::string & getDeviceName();


  private:

    typedef IasLocalAudioBufferDesc::AudioBufferDescMode AudioBufferDescMode;

    /**
     * @brief This function cleans up allocated resources
     *
     * @returns eIasAvbProcOK upon success, an error code otherwise
     */
    IasAvbProcessingResult cleanup();

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbAudioShmProvider(IasAvbAudioShmProvider const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbAudioShmProvider& operator=(IasAvbAudioShmProvider const &other);

    /**
     * @brief Set the hw constraints that shall be valid for the Alsa device
     */
    void setHwConstraints();

    /**
     * @brief The IPC thread receiving the commands from the alsa-smartx-plugin
     */
    void ipcThread();

    inline bool hasBufferDesc() const;

    /**
     * @brief Return TRUE if the ALSA prefilling feature is enabled
     */
    inline bool isPrefillEnable() const;

    /**
     * @brief Reset the shared memory buffer fill level at mAlsaPrefill
     *
     * @param[in] nextState the next buffer state to be changed after resetting the buffer
     */
    IasAvbProcessingResult resetShmBuffer(bufferState nextState);
    IasAvbProcessingResult resetShmBuffer(bufferState nextState, uint32_t frames);

    /**
     *  @brief get the exclusive access right to the alsa ringbuf
     */
    inline void lockShmBuffer();

    /**
     *  @brief release the exclusive access right
     */
    inline void unlockShmBuffer();

    DltContext                            *mLog;             //!< Dlt Log Context
    std::string                            mDeviceName;      //!< Name of the device
    IasAudio::IasAudioDeviceParamsPtr      mParams;          //!< Pointer to device params
    IasAudio::IasAlsaPluginShmConnection   mShmConnection;   //!< The shm connection to the alsa-smartx-plugin
    std::thread                           *mIpcThread;       //!< Pointer to the IPC thread
    volatile bool                          mIsRunning;       //!< Flag used as exit condition for the IPC thread
    IasAudio::IasAudioIpc                 *mInIpc;           //!< Pointer to the incoming ipc (from alsa-smartx-plugin)
    IasAudio::IasAudioIpc                 *mOutIpc;          //!< Pointer to the outgoing ipc (to alsa-smartx-plugin)
    bool                                   mDirWriteToShm;   //!< Direction of the data transfer
    AudioData                             *mNullData;        //!< Null samples for the case that there is no actual data available
    AudioBufferDescMode                    mDescMode;
    IasLibPtpDaemon                       *mPtpProxy;
    uint32_t                               mAlsaPrefill;     //!< Number of periods the shm buffer is being prefilled (capture device only)
    bufferState                            mBufferState;     //!< state of the buffer handling, initial state is pre-fill
    uint32_t                               mBufRstContCnt;   //!< Current continuous buffer reset count
    uint32_t                               mBufRstContCntThr;//!< Max continuous buffer reset count to trigger ALSA pre-filling again
    uint32_t                               mAlsaPrefilledSz; //!< Current pre-filled sample count
    std::mutex                             mShmBufferLock;
    bool                                   mIsClientSmartX;
    uint32_t                               mLastPtpEpoch;
    uint64_t                               mDbgLastTxBufOverrunIdx;
};


inline const std::string & IasAvbAudioShmProvider::getDeviceName()
{
  return mDeviceName;
}


inline bool IasAvbAudioShmProvider::hasBufferDesc() const
{
  return (AudioBufferDescMode::eIasAudioBufferDescModeOff < mDescMode) &&
            (mDescMode < AudioBufferDescMode::eIasAudioBufferDescModeLast);
}


inline bool IasAvbAudioShmProvider::isPrefillEnable() const
{
  return (0u != mAlsaPrefill);
}


inline void IasAvbAudioShmProvider::lockShmBuffer()
{
  if (mDirWriteToShm && isPrefillEnable())
  {
    mShmBufferLock.lock();
  }
}


inline void IasAvbAudioShmProvider::unlockShmBuffer()
{
  if (mDirWriteToShm && isPrefillEnable())
  {
    mShmBufferLock.unlock();
  }
}


} // namespace IasMediaTransportAvb


#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAUDIOSHMPROVIDER_HPP */
