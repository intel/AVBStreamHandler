/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoInStream.hpp
 * @brief   This class contains all methods to handle an incoming video stream (from SHM side).
 * @details An video stream is the video data container for video transmitted via shared memory.
 *          This class is derived from IasLocalVideoStream.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOINSTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOINSTREAM_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_video_common/IasAvbVideoShmConnection.hpp"
#include "avb_helper/IasIRunnable.hpp"



namespace IasMediaTransportAvb {

class IasThread;
struct PacketH264;
struct PacketMpegTS;

class IasLocalVideoInStream : public IasLocalVideoStream, public IasIRunnable
{
  public:

    /*!
     * @brief mpegts - tsp size
     */
    static const uint16_t cTspSize = 188u;

    /*!
     * @brief mpegts - tsp + sph size
     */
    static const uint16_t cTSP_SIZE_SPH = 192u;

    /*!
     * @brief Constructor.
     */
    IasLocalVideoInStream(DltContext &dltContext, uint16_t streamId);

    /*!
     * @brief Destructor, virtual by default.
     */
    virtual ~IasLocalVideoInStream();


    /*!
     * @brief Initialize method.
     *
     * Pass component specific initialization parameter.
     *
     * @param[in]  ipcName          Name of the IPC/ video stream
     * @param[in]  maxPacketRate    Maximum number of video packets per second
     * @param[in]  maxPacketSize    Maximum size of a video packet
     * @param[in]  format           Format of the packet (H.264 or MPEG-TS)
     * @param[in]  numPackets       Number of packets
     * @param[in]  optimalFillLevel Reserved for future use
     * @param[in]  internalBuffers  Internal buffers shall be used (true) or not (false)
     *
     * @returns    eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult init(std::string ipcName, uint16_t maxPacketRate, uint16_t maxPacketSize,
                                IasAvbVideoFormat format, uint16_t numPackets, uint32_t optimalFillLevel,
                                bool internalBuffers);

    /*!
     * @brief Cleans up all resources used.
     */
    void cleanup();

    /*!
     * @brief Starts the stream (worker thread).
     *
     * @returns  eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult start();

    /*!
     * @brief Stops the stream (worker thread).
     *
     * @returns  eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult stop();

  private:

    /*!
     * @brief copys the data form/to shared memory .
     *
     * @param[in]  packet  thread-id of current thread
     *
     * @returns  eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult copyJob(pid_t tid);

    /*!
     * @brief Returns whether the instance is initialized or not.
     *
     * @returns  'true' if already initialized, 'false' if not.
     */
    bool isInitialized() const { return !mIpcName.empty(); }


    /*!
     * @brief Called once before starting the worker thread.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasResult beforeRun();

    /*!
     * @brief The main worker thread function.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasResult run();

    /*!
     * @brief Called to initiate the worker thread shutdown.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasResult shutDown();

    /*!
     * @brief Called once after worker thread finished executing.
     *
     * Inherited by Ias::IasIRunnable.
     *
     * @return Status of the method.
     */
    IasResult afterRun();

    /*!
     * @brief Reset video buffer to initial state
     */
    IasAvbProcessingResult resetBuffers();

    /*!
     * @brief Update buffer status
     */
    void updateBufferStatus();

    /*!
     * @brief Called if new IasAvbStreaming h264 packet is available to be passed to AVB stack.
     *
     * @param[in]  packet  H.264 video packet to be transferred over AVB
     */
    void eventOnNewPacketH264(PacketH264 const &packet);

    /*!
     * @brief Called if new IasAvbStreaming MPEG-TS packet is available to be passed to AVB stack.
     *
     * @param[in]  packet  MPEG-TS video packet to be transferred over AVB
     */
    void eventOnNewPacketMpegTS(PacketMpegTS const &packet);

    /*!
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLocalVideoInStream(IasLocalVideoInStream const &other);

    /*!
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLocalVideoInStream& operator=(IasLocalVideoInStream const &other);


    //
    // Member Variables
    //

    std::string               mIpcName;           //!< Name of the stream
    IasAvbVideoShmConnection  mShmConnection;     //!< Connection providing the shared memory access
    IasAvbVideoRingBuffer    *mRingBuffer;        //!< Pointer to the ring buffer
    uint32_t                  mRingBufferSize;    //!< Size of the ring buffer
    IasThread                 *mThread;           //!< Handle for the thread
    bool                      mThreadIsRunning;   //!< Flag indicating whether thread is running
    uint32_t                  mOptimalFillLevel;  //!< FillLevel = WritePointer - ReadPointer
    uint16_t                  mRtpSequNrLast;     //!< Continuous packet counter
    uint16_t                  mTimeout;           //!< Timeout value waiting for data
};


} // namespace IasMediaTransportAvb


#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOINSTREAM_HPP */
