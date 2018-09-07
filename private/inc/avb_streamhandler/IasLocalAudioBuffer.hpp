/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalAudioBuffer.hpp
 * @brief   This class contains all methods to access the ring buffers
 * @details Each channel of a local audio stream handles its data via a
 *          separate ring buffer.
 *
 * @date    2013
 */

#ifndef IASLOCALAUDIOBUFFER_HPP_
#define IASLOCALAUDIOBUFFER_HPP_


#include "avb_streamhandler/IasAvbTypes.hpp"
#include <cstring>
#include <mutex>
#include <dlt.h>

namespace IasMediaTransportAvb {


class IasLocalAudioBuffer
{
  public:
    typedef int16_t AudioData;

    enum IasAudioBufferState
    {
      eIasAudioBufferStateInit     = 0,
      eIasAudioBufferStateOk       = 1,
      eIasAudioBufferStateUnderrun = 2,
      eIasAudioBufferStateOverrun  = 3,
    };

    struct DiagData
    {
        uint32_t numOverrun;
        uint32_t numUnderrun;
        uint32_t numOverrunTotal;
        uint32_t numUnderrunTotal;
        uint32_t numReset;

        DiagData();
    };

    /**
     *  @brief Constructor.
     */
    IasLocalAudioBuffer();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLocalAudioBuffer();

    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *  Derived from base class.
     */
    IasAvbProcessingResult init(uint32_t totalSize, bool doAnalysis);

    /**
     *  @brief Reset functionality for the channel buffers
     */
    IasAvbProcessingResult reset(uint32_t optimalFillLevel);

    /**
     *  @brief Writes data into the local ring buffer
     */
    uint32_t write(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples);

    /**
     *  @brief Writes data into the local ring buffer iterating through samples here instead of in copyJob
     */
    uint32_t write(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples, uint32_t step);

    /**
     *  @brief Reads data from the local ring buffer iterating through samples here instead of in copyJob
     */
    uint32_t read(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples, uint32_t step);

    /**
     *  @brief Reads data from the local ring buffer
     */
    uint32_t read(IasLocalAudioBuffer::AudioData * buffer, uint32_t nrSamples);

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief get current fill level
     */
    inline uint32_t getFillLevel() const;

    /**
     * @brief get current fill level compared to reference level
     */
    inline int32_t getRelativeFillLevel() const;

    /**
     * @brief get maximum fill level (i.e. total size)
     */
    inline uint32_t getTotalSize() const;

    /**
     * @brief indicate the Audio ring buffer has reached the required fill level to start reading
     */
    inline bool isReadReady() const;

    /**
     * @brief set fill level to allow initial read access
     */
    inline IasAvbProcessingResult setReadThreshold(uint32_t fillLevel);

    /**
     * @brief get fill level to allow initial read access
     */
    inline uint32_t getReadThreshold();

    /**
     * @brief get current continuous read index
     */
    inline uint64_t getMonotonicReadIndex() const;

    /**
     * @brief get current continuous write index
     */
    inline uint64_t getMonotonicWriteIndex() const;

    /**
     * @brief get diagnostic information
     */
    inline DiagData *getDiagData() const;

  private:

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLocalAudioBuffer(IasLocalAudioBuffer const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLocalAudioBuffer& operator=(IasLocalAudioBuffer const &other);

    uint32_t              mReadIndex;
    uint32_t              mWriteIndex;
    uint32_t              mReadCnt;
    uint32_t              mWriteCnt;
    uint32_t              mTotalSize;       //in samples (IasLocalAudioBuffer::AudioData)
    uint32_t              mReferenceFill;   //in samples (IasLocalAudioBuffer::AudioData)
    IasAudioBufferState   mBufferState;
    IasAudioBufferState   mBufferStateLast;
    bool                  mDoAnalysis;
    uint32_t              mReadIndexLastWriteCall;
    std::mutex            mLock;
    int16_t              *mBuffer;
    uint32_t              mLastRead;
    DiagData              mDiagData;
    DltContext           *mLog;
    bool                  mReadReady;
    uint32_t              mReadThreshold;
    uint64_t              mMonotonicReadIndex;
    uint64_t              mMonotonicWriteIndex;
};


inline uint32_t IasLocalAudioBuffer::getFillLevel() const
{
  uint32_t ret = mWriteIndex - mReadIndex;

  if (ret > mTotalSize)
  {
    ret += mTotalSize;
  }

  return ret;
}


inline int32_t IasLocalAudioBuffer::getRelativeFillLevel() const
{
  int32_t ret = 0;

  if (0 != mReferenceFill)
  {
    ret = int32_t(getFillLevel() - mReferenceFill);
  }

  return ret;
}


inline uint32_t IasLocalAudioBuffer::getTotalSize() const
{
  return mTotalSize;
}


inline bool IasLocalAudioBuffer::isReadReady() const
{
  return mReadReady;
}


inline IasAvbProcessingResult IasLocalAudioBuffer::setReadThreshold(uint32_t fillLevel)
{
  IasAvbProcessingResult result = eIasAvbProcInvalidParam;

  if (fillLevel <= getTotalSize())
  {
    mReadThreshold = fillLevel;
    result = eIasAvbProcOK;
  }
  return result;
}


inline uint32_t IasLocalAudioBuffer::getReadThreshold()
{
  return mReadThreshold;
}


inline uint64_t IasLocalAudioBuffer::getMonotonicReadIndex() const
{
  return mMonotonicReadIndex;
}


inline uint64_t IasLocalAudioBuffer::getMonotonicWriteIndex() const
{
  return mMonotonicWriteIndex;
}


} // namespace IasMediaTransportAvb

#endif /* IASLOCALAUDIOBUFFER_HPP_ */
