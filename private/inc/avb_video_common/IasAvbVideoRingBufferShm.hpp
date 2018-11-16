/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBufferShm.hpp
 * @brief   This is the definition of the video ring buffer located in shared memory.
 * @details An instance of this class will be located in shared memory. Its member functions
 *          can be accessed from both sides, client and server. The ring buffer contains of several
 *          buffers
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERSHM_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERSHM_HPP


#include "avb_video_common/IasAvbVideoCondVar.hpp"
#include "avb_video_common/IasAvbVideoLog.hpp"
#include "avb_video_common/IasAvbVideoCommonTypes.hpp"
#include "avb_video_common/IasAvbVideoRingBufferResult.hpp"
#include "internal/audio/common/IasIntProcCondVar.hpp"
#include "internal/audio/common/IasIntProcMutex.hpp"

#include <atomic>
#include <boost/interprocess/offset_ptr.hpp>
#include <dlt/dlt.h>

// Using mutex and condvar from audio/common
using IasAudio::IasIntProcMutex;
using IasAudio::IasIntProcCondVar;

namespace IasMediaTransportAvb {

static const uint16_t cIasVideoRingBufferShmMaxReaders = 32;

class __attribute__ ((visibility ("default"))) IasAvbVideoRingBufferShm
{

  public:

   /**
    * @brief constructor
    */
    IasAvbVideoRingBufferShm();

    /**
     * @brief destructor
     */
    ~IasAvbVideoRingBufferShm();

    /*!
     * @brief Initialize an video ring buffer. This method is called by the ring buffer factory.The ring buffer will be
     *        of type eIasRingBufferShared or eIasRingBufferLocalReal, depending on the boolean parameter @a shared
     *        (not used at the moment).
     *
     * @param[in]  bufferSize     Size of one buffer (packet) in bytes
     * @param[in]  numBuffers     Number of buffers (packets)
     * @param[in]  dataBuf        The real data buffer
     * @param[in]  shared         Reserved for later use
     * @param[in]  ringBufShm     Pointer to the IasAvbVideoRingBufferShm
     *
     * @returns                   eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult init(uint32_t bufferSize, uint32_t numBuffers, void* dataBuf, bool shared);

    /*!
     * @brief Get the number of buffers (packets) ready to be read / written
     *
     * This method returns the number of packets that are currently available for read access or the number of free
     * entries to be written.
     *
     * @param[in]  access      Specifies the access type (either eIasRingBufferAccessRead or eIasRingBufferAccessWrite).
     * @param[in]  pid         Caller pid. Must match a previous registered pid for read access. Ignored for write access.
     * @param[out] numBuffers  Returned number of buffers (packets) that are ready to be processed.
     *
     * @returns                eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult updateAvailable(IasRingBufferAccess access, pid_t pid, uint32_t *numBuffers);

    /*!
     * @brief Request to access the video ring buffer
     *
     * The function should be called before a direct (mmap) area can be accessed.
     * The resulting size parameter is always less or equal to the input count of
     * buffers (packets) and can be zero, if no buffers can be processed, i.e., if the the ring
     * buffer is full (playback) or empty (capture).
     *
     * @param[in]     access  Specifies the access type (either eIasRingBufferAccessRead or eIasRingBufferAccessWrite).
     * @param[in]     pid     Caller pid. Must match a previous registered pid for read access. Ignored for write access.
     * @param[out]    dataPtr Returned mmap'ed base pointer to the data packets.
     * @param[out]    offset  Returned mmap offset in buffers (== packets).
     * @param[in,out] numBuffers  mmap area portion size in buffers (wanted on entry, contiguous available on exit).
     *
     * @returns                eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult beginAccess(IasRingBufferAccess access, pid_t pid, uint32_t* offset, uint32_t* numBuffers);

    /*!
     * @brief Declare that accessing a portion of an mmap'ed area has finished.
     *
     * The offset value that IasAvbVideoRingBuffer::beginAccess() has reported should be passed.
     * The numBuffers parameter should hold the number of buffers (packets) that have been written or read
     * to/from the video buffer. The numBuffers parameter must never exceed the contiguous frames
     * count that IasAvbVideoRingBuffer::beginAccess() returned.
     *
     * Each call of IasAvbVideoRingBuffer::beginAccess() must be followed by a call of
     * IasAvbVideoRingBuffer::endAccess().
     *
     * @param[in] access  Specifies the access type (either eIasRingBufferAccessRead or eIasRingBufferAccessWrite).
     * @param[in] pid     Caller pid. Must match a previous registered pid for read access. Ignored for write access.
     * @param[in] offset  Offset in buffers (== packets), must be equal to the offset value that
     *                    IasAvbVideoRingBuffer::beginAccess() returned.
     * @param[in] numBuffers  mmap area portion size in buffers (== number of packets that have been processed)
     *
     * @returns           eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult endAccess(IasRingBufferAccess access, pid_t pid, uint32_t offset, uint32_t numBuffers);

    /*!
     * @brief function to retrieve the data pointer.
     *
     * @returns a void pointer to the data.
     */
    void * getDataBuffer() const { return mDataBuf.get(); }

    /*!
     * @brief   Get the size of one data buffer (packet) within the ring buffer.
     *
     * @returns The size of one data buffer (packet).
     */
    uint32_t getBufferSize() const { return mBufferSize; }

    /*!
     * @brief   Get the overall number of buffers (packets) the ring buffer contains of.
     *
     * @returns The size of one data buffer (packet).
     */
    uint32_t getNumberBuffers() const { return mNumBuffers; }

    /*!
     * @brief function to read from ring buffer (with timeout) when a desired buffer level is reached.
     *        the function either returns when a timeout occurs or when the level is reached.
     *
     * @param[in] pid         Caller pid. Must match a previous registered pid for read access.
     * @param[in] numBuffers  the desired buffer level, must be > 0 and >= total number of buffers
     * @param[in] timeout_ms  timeout in ms, function will return if buffer level is not reached within timeout, must be > 0
     *
     * @returns               eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult waitRead(pid_t pid, uint32_t numBuffers, uint32_t timeout_ms);

    /*!
     * @brief function to write to ring buffer (with timeout) when a desired buffer level is reached.
     *        the function either returns when a timeout occurs or when the level is reached.
     *
     * @param[in] numBuffers  the desired buffer level, must be > 0 and >= total number of buffers
     * @param[in] timeout_ms  timeout in ms, function will return if buffer level is not reached within timeout, must be > 0
     *
     * @returns               eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult waitWrite(uint32_t numBuffers, uint32_t timeout_ms);

    /*!
     * @brief   Get the read offset (index within the ring buffer).
     * @returns The read offset.
     */
    uint32_t getReadOffset() const { return mReadOffset; }

    /*!
     * @brief   Get the write offset (index within the ring buffer).
     * @returns The write offset.
     */
    uint32_t getWriteOffset() const { return mWriteOffset; }

    /*!
     * @brief Register a reader on the ringbuffer.
     *
     * As it's possible to have multiple readers reading a ringbuffer, it is necessary some
     * coordination among them and the - single - writer. By registering a reader with the
     * ringbuffer, ringbuffer becomes aware of that new reader, and can keep proper track of it.
     * The id used here is expected on future beginAccess, endAccess and waitRead calls.
     *
     * @param[in] pid  An id for the reader - usually the thread id or process id for single thread readers.
     *
     * @returns        eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult addReader(pid_t pid);

    /*!
     * @brief Unregister a reader on the ringbuffer.
     *
     * After this call, future beginAccess, endAccess and waitRead calls will fail with
     * eIasRingBuffInvalidParam.
     *
     * @param[in] pid  Previously registered reader id.
     *
     * @returns        eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult removeReader(pid_t pid);

    /*!
     * @brief Time of writer last access to the ring buffer.
     *
     * Readers can check the liveliness of writer by ensuring this time doesn't
     * get too distant from now.
     *
     * @returns Last access time in nanoseconds.
     */
    uint64_t getWriterLastAccess() {
        return mWriterLastAccess;
    }

  private:

    /* Struct to keep track of reader register to read on this RingBuffer */
    struct RingBufferReader {
        pid_t pid;
        uint32_t offset;
        uint32_t allowedToRead;
        std::atomic<uint64_t> lastAccess;
    };

    /*!
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoRingBufferShm(IasAvbVideoRingBufferShm const &other);

    /*!
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoRingBufferShm& operator=(IasAvbVideoRingBufferShm const &other);

    /*!
     * @brief Get smaller reader offset and resets readers offset when due
     *
     * Walks readers list and get which one has the smallest offset - or, the
     * offset of the slowest reader. Also, if all readers are at the end of
     * the buffer, resets their offsets, so they can start reading from the
     * beginning again.
     *
     * @returns        smallest offset of all readers, or UINT32_MAX if no readers are registered.
     */
    uint32_t updateSmallerReaderOffset();

    /*!
     * @brief updates mReadOffset with smallest offset among readers
     *
     * This helps ringbuffer to keep track of what packets weren't read yet.
     */
    void aggregateReaderOffset();

    /*!
     * @brief Get individual reader buffer level
     *
     * As some readers may be faster than others, the buffer level (i.e., how many packets
     * were not read) will vary from reader to reader. This method calculates the right
     * buffer level for any reader.
     *
     * @returns     reader buffer level.
     */
    uint32_t calculateReaderBufferLevel(RingBufferReader *reader);

    /*!
     * @brief updates lastAccess for reader, so it's possible to track reader death by timeout
     */
    void updateReaderAccess(RingBufferReader *reader);

    /*!
     * @brief Removes any reader whose lastAccess is bigger than a defined threshold (a presumably dead reader).
     */
    void purgeUnresponsiveReaders();

    /*!
     * @brief Returns a RingBufferReader entry for a reader, given its id
     *
     * @returns     reader or nullptr if no reader with given id was found.
     */
    RingBufferReader *findReader(pid_t pid) {
      if (pid > 0)
      {
        for (int i = 0; i < cIasVideoRingBufferShmMaxReaders; i++)
        {
          if (mReaders[i].pid == pid)
          {
            return &mReaders[i];
          }
        }
      }
      return nullptr;
    };

    /*!
     * @brief updates lastAccess for writer, so it's possible to track writer death by timeout
     */
    void updateWriterAccess();

    /* As this class lives on shared memory, it can *not* save anything that
     * is related to any process, like the log context. It needs to get it every
     * time, so to get the right log context for the calling process */
    DltContext &getLogContext() {
        return IasAvbVideoLog::getDltContext("_RBF");
    }

    //
    // Member variables
    //

    uint32_t                              mBufferSize;           //!< size of one buffer in bytes
    uint32_t                              mNumBuffers;           //!< number of buffers
    uint32_t                              mReadOffset;           //!< number of buffers (packets) wrt the base pointer
    uint32_t                              mWriteOffset;          //!< number of buffers (packets) wrt the base pointer
    uint32_t                              mBufferLevel;          //!< fill level in samples
    bool                                  mShared;               //!< flag to indicate if the buffer is in shared memory
    bool                                  mInitialized;          //!< this flag is true when init function was successful
    std::atomic<bool>                     mReadInProgress;       //!< indicating a read in progress
    std::atomic<bool>                     mWriteInProgress;      //!< indicating a write in progress
    boost::interprocess::offset_ptr<void> mDataBuf;              //!< the offset pointer to the data memory
    IasIntProcMutex                       mMutex;                //!< used to lock critical sections
    IasIntProcMutex                       mMutexReadInProgress;  //!< to avoid that reset is executed while reading from buffer
    IasIntProcMutex                       mMutexWriteInProgress; //!< to avoid that reset is executed while writing into buffer
    IasAvbVideoCondVar                    mCondRead;             //!< conditional variable for read access
    IasAvbVideoCondVar                    mCondWrite;            //!< conditional variable for write access
    uint32_t                              mReadWaitLevel;        //!< buffer level that must be reached before a signal is sent
    uint32_t                              mWriteWaitLevel;       //!< buffer level that must be reached before a signal is sent
    uint32_t                              mAllowedToWrite;       //!< how many buffers (packets) were allowed to writer on last beginAccess
    uint64_t                              mWriterLastAccess;     //!< writer last access time

    IasIntProcMutex                       mMutexReaders;                              //!< Protects access to mReaders array
    RingBufferReader                      mReaders[cIasVideoRingBufferShmMaxReaders]; //!< List of active readers
};

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERSHM_HPP */
