/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBuffer.hpp
 * @brief   Definition of the video ring buffer.
 * @details This video ring buffer class aggregates the actual ring buffer (IasAvbVideoRingBufferShm)
 *          which is located in shared memory.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFER_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFER_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_video_common/IasAvbVideoCommonTypes.hpp"
#include "avb_video_common/IasAvbVideoRingBufferResult.hpp"

#include <dlt/dlt.h>

namespace IasMediaTransportAvb {

// forward declaration
class IasAvbVideoRingBufferShm;


class __attribute__ ((visibility ("default"))) IasAvbVideoRingBuffer
{

  public:

   /**
    * @brief constructor
    */
    IasAvbVideoRingBuffer();

    /**
     * @brief destructor
     */
    ~IasAvbVideoRingBuffer();

    /*!
     * @brief Initialize a video ring buffer. This method is called by the ring buffer factory.The ring buffer will be
     *        of type eIasRingBufferShared or eIasRingBufferLocalReal, depending on the boolean parameter @a shared
     *        (not used at the moment).
     *
     * @param[in]  bufferSize     Size of one buffer (packet) in bytes
     * @param[in]  numBuffers     Number of buffers (packets)
     * @param[in]  dataBuf        The real data buffer
     * @param[in]  shared         Reserved for later use
     * @param[in]  ringBufShm     Pointer to the IasAvbVideoRingBufferShm
     * @param[in]  dltContext     Log context
     *
     * @returns                   eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult init(uint32_t bufferSize,
                                  uint32_t numBuffers,
                                  void *dataBuf,
                                  bool shared,
                                  IasAvbVideoRingBufferShm *ringBufShm);

    /*!
     * @brief Clean up all resources used.
     */
    void cleanup();

    /*!
     * @brief Setup a ring buffer. This function is used by the ring buffer factory when
     *        an IasAvbVideoRingBufferShm was found in shared memory or a new created IasAvbVideoRingBuffer
     *        needs to be setup finally.
     *
     * @param[in]  ringBufShm  pointer to IasAvbVideoRingBufferShm
     *
     * @returns                eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult setup(IasAvbVideoRingBufferShm *ringBufShm);

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
    IasVideoRingBufferResult beginAccess(IasRingBufferAccess access,
                                         pid_t pid,
                                         void **dataPtr,
                                         uint32_t *offset,
                                         uint32_t *numBuffers);

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
     * @returns                eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult endAccess(IasRingBufferAccess access, pid_t pid, uint32_t offset, uint32_t numBuffers);

    /*!
     * @brief function to read from ring buffer (with timeout) when a desired buffer level is reached.
     *        the function either returns when a timeout occurs or when the level is reached.
     *
     * @param[in] pid         Caller pid. Must match a previous registered pid for read access.
     * @param[in] numBuffers  the desired buffer level, must be > 0 and >= total number of buffers
     * @param[in] timeout_ms  timeout in ms, function will return if buffer level is not reached within timeout, must be > 0
     *
     * @returns                eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult waitRead(pid_t pid, uint32_t numBuffers, uint32_t timeout_ms);

    /*!
     * @brief function to write to ring buffer (with timeout) when a desired buffer level is reached.
     *        the function either returns when a timeout occurs or when the level is reached.
     *
     * @param[in] numBuffers  the desired buffer level, must be > 0 and >= total number of buffers
     * @param[in] timeout_ms  timeout in ms, function will return if buffer level is not reached within timeout, must be > 0
     *
     * @returns                eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoRingBufferResult waitWrite(uint32_t numBuffers, uint32_t timeout_ms);

    /*!
     * @brief function to return the pointer to the video shm buffer
     *
     * @returns pointer to video shm buffer, which is const.
     *
     */
    const IasAvbVideoRingBufferShm* getShm() const { return mRingBufShm; }

    /*!
     * @brief   Get the read offset (index within the ring buffer).
     * @returns The read offset.
     */
    uint32_t getReadOffset() const;

    /*!
     * @brief   Get the write offset (index within the ring buffer).
     * @returns The write offset.
     */
    uint32_t getWriteOffset() const;

    /*!
     * @brief   Get the size of one data buffer (packet).
     * @returns The size of one data buffer (packet).
     */
    uint32_t getBufferSize() const;

    /*!
     * @brief Set the name of the ring buffer.
     *
     * This function is called by the buffer factory to give the ring buffer a name.
     *
     * @param[in] name The name of the ring buffer
     */
    void setName(const std::string &name) { mName = name; }

    /*!
     * @brief Get the name of the ring buffer
     *
     * @returns The name of the ring buffer
     */
    const std::string& getName() const { return mName; }

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
    uint64_t getWriterLastAccess();

  private:
    /*!
     *  @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoRingBuffer(IasAvbVideoRingBuffer const &other);

    /*!
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoRingBuffer& operator=(IasAvbVideoRingBuffer const &other);


    //
    // Member variables
    //

    IasAvbVideoRingBufferShm   *mRingBufShm;       //!< pointer to IasVideoRingBufferShm
    void                       *mDataPtr;          //!< pointer to start of data in ring buffer
    bool                       mIsShm;             //!< flag to indicate if it is a buffer in shared memory
    std::string                mName;              //!< the name of the ring buffer
};


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFER_HPP */
