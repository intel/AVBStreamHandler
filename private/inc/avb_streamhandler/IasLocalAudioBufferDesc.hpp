/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalAudioBufferDesc.hpp
 * @brief   This class contains all methods to access the audio buffer descriptor queue
 * @details Each channel of a local audio stream handles its data in accordance with
 *          the timestamps stored in this queue.
 *
 * @date    2016
 */

#ifndef IASLOCALAUDIOBUFFERDESC_HPP_
#define IASLOCALAUDIOBUFFERDESC_HPP_


#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include <mutex>

namespace IasMediaTransportAvb {

static const char* AudioBufferDescModeNames[] =
  {
      "off",
      "fail-safe",
      "hard",
      "invalid"
  };

class IasLocalAudioBufferDesc
{
  public:

    enum AudioBufferDescMode
    {
      eIasAudioBufferDescModeOff,
      eIasAudioBufferDescModeFailSafe,
      eIasAudioBufferDescModeHard,
      eIasAudioBufferDescModeLast     // invalid entry
    };

    /**
     *  @brief Constructor.
     */
    IasLocalAudioBufferDesc(uint32_t qSize);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLocalAudioBufferDesc();

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    struct AudioBufferDesc
    {
      uint64_t timeStamp; // time at the samples were written to a buffer
      uint64_t bufIndex;  // virtual position in the ring buffer where the samples start from
      uint32_t sampleCnt; // number of samples which belongs to the timestamp
    };

    /**
     *  @brief put in a descriptor to the FIFO queue
     */
    void enqueue(const IasLocalAudioBufferDesc::AudioBufferDesc &desc);

    /**
     *  @brief get a descriptor from FIFO (take off it from queue)
     */
    IasAvbProcessingResult dequeue(IasLocalAudioBufferDesc::AudioBufferDesc &desc);

    /**
     *  @brief get a descriptor from FIFO w/o dequeuing
     */
    inline IasAvbProcessingResult peek(IasLocalAudioBufferDesc::AudioBufferDesc &desc);

    /**
     *  @brief get a descriptor at index from the queue's head w/o dequeuing
     */
    IasAvbProcessingResult peekX(IasLocalAudioBufferDesc::AudioBufferDesc &desc, uint32_t index);

    /**
     *  @brief flush all descriptors from FIFO
     */
    inline void reset();

    /**
     *  @brief get the exclusive access right to FIFO (to be used in combination with peek() if needed)
     */
    inline void lock();

    /**
     *  @brief release the exclusive access right to FIFO
     */
    inline void unlock();

    /**
     *  @brief return const string corresponding to mode
     */
    static inline const char* getAudioBufferDescModeString(IasLocalAudioBufferDesc::AudioBufferDescMode mode);

    /**
     * @brief Sets a flag indicating that a reset is needed
     */
    inline void setResetRequest();

    /**
     * @brief Returns the reset request flag.
     */
    inline bool getResetRequest();


    /**
     * @brief set time when warning message is output for presentation time handling
     *
     * This is used by the AvbAlsaWrk thread for debugging purposes only. The thread records
     * time when it waives presenting audio samples in the hard-mode or enforces presenting
     * audio samples regardless of the presentation time in case it has a risk of local buffer
     * overrun in the fail-safe mode. The recorded time is used to control the output rate of
     * warning messages.
     */
    inline void setDbgPresentationWarningTime(const uint64_t time);

    /**
     * @brief get the last recorded warned time
     */
    inline uint64_t getDbgPresentationWarningTime() const;

    /**
     * @brief set the flag to let ALSA discard received samples from network which are behind PTS
     */
    inline void setAlsaRxSyncStartMode(const bool on);

    /**
     * @brief get the flag which was set by setAlsaRxSyncStartMode()
     */
    inline bool getAlsaRxSyncStartMode();

  private:

    typedef std::vector<AudioBufferDesc> AudioBufferDescVec;

    std::recursive_mutex          mLock;
    AudioBufferDescVec  mDescQ;
    uint32_t              mDescQsz;
    /*AudioBufferDescMode mMode*/;
    bool                mResetRequest;

    uint64_t              mDbgPresentationWarningTime;
    bool                mAlsaRxSyncStart;
};

inline IasAvbProcessingResult IasLocalAudioBufferDesc::peek(IasLocalAudioBufferDesc::AudioBufferDesc &desc)
{
  return peekX(desc, 0u);
}

inline void IasLocalAudioBufferDesc::reset()
{
  cleanup();
  setResetRequest();
}

inline void IasLocalAudioBufferDesc::lock()
{
  mLock.lock();
}

inline void IasLocalAudioBufferDesc::unlock()
{
  mLock.unlock();
}

inline const char* IasLocalAudioBufferDesc::getAudioBufferDescModeString(IasLocalAudioBufferDesc::AudioBufferDescMode mode)
{
  if (mode < eIasAudioBufferDescModeLast)
  {
    return AudioBufferDescModeNames[mode];
  }
  else
  {
    return AudioBufferDescModeNames[eIasAudioBufferDescModeLast];
  }
}

inline void IasLocalAudioBufferDesc::setResetRequest()
{
  lock();

  mResetRequest = true;

  unlock();
}

inline bool IasLocalAudioBufferDesc::getResetRequest()
{
  lock();

  bool ret = mResetRequest;
  mResetRequest = false;

  unlock();
  return ret;
}

inline void IasLocalAudioBufferDesc::setDbgPresentationWarningTime(const uint64_t time)
{
  mDbgPresentationWarningTime = time;
}

inline uint64_t IasLocalAudioBufferDesc::getDbgPresentationWarningTime() const
{
  return mDbgPresentationWarningTime;
}

inline void IasLocalAudioBufferDesc::setAlsaRxSyncStartMode(const bool on)
{
  // caller should hold a lock
  mAlsaRxSyncStart = on;
}

inline bool IasLocalAudioBufferDesc::getAlsaRxSyncStartMode()
{
  // caller should hold a lock
  return mAlsaRxSyncStart;
}


} // namespace IasMediaTransportAvb


#endif /* IASLOCALAUDIOBUFFERDESC_HPP_ */
