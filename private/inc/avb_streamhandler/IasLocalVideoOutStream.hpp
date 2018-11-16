/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoOutStream.hpp
 * @brief   This class contains all methods to handle an outgoing video stream.
 * @details An local video stream is the video data container for video received via SHM.
 *          This class is derived from IasLocalVideoStream.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOOUTSTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOOUTSTREAM_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_video_common/IasAvbVideoShmConnection.hpp"


namespace IasMediaTransportAvb {

struct PacketH264;
struct PacketMpegTS;

class IasLocalVideoOutStream : public IasLocalVideoStream
{
  public:

    /*!
     * @brief Constructor.
     */
    IasLocalVideoOutStream(DltContext &dltContext, uint16_t streamId);

    /*!
     * @brief Destructor, virtual by default.
     */
    virtual ~IasLocalVideoOutStream();

    /*!
     * @brief Initialize method.
     *
     * Pass component specific initialization parameter.
     *
     * @param[in]  ipcName        Name of the IPC/ video stream
     * @param[in]  maxPacketRate  Maximum number of video packets per second
     * @param[in]  maxPacketSize  Maximum size of a video packet
     * @param[in]  format         Format of the packet (H.264 or MPEG-TS)
     * @param[in]  numPackets     Number of packets
     * @param[in]  optimalFillLevel Reserved for future use
     * @param[in]  internalBuffers Internal buffers shall be used (true) or not (false)
     *
     * @returns    eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult init(std::string ipcName, uint16_t maxPacketRate, uint16_t maxPacketSize,
                                IasAvbVideoFormat format, uint16_t numPackets, uint32_t optimalFillLevel,
                                bool internalBuffers);

    /*!
     * @brief Clean up all allocated resources.
     */
    void cleanup();

  private:

    /*!
     * @brief Writes video data into the local ring buffer
     */
    virtual IasAvbProcessingResult writeLocalVideoBuffer(IasLocalVideoBuffer * buffer, IasLocalVideoBuffer::IasVideoDesc * descPacket);

    /*!
     * @brief Will transfer an H.264 video packet via IPC to the client side.
     *
     * @param[in]  packet Video packet to be transfered.
     *
     * @returns    eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult pushPayload(PacketH264 const & packet);

    /*!
     * @brief Will transfer an MPEG-TS video packet via IPC to the client side.
     *
     * @param[in]  packet Video packet to be transfered.
     *
     * @returns    eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult pushPayload(PacketMpegTS const & packet);

    /*!
     * @brief Sends null packet to local ring buffer
     *
     * This will allow client to track liveliness of the server, as
     * local ring buffer keeps the last time of access made by the
     * server
     *
     * @returns    eIasAvbProcOK on success, otherwise an error code.
     */
    IasAvbProcessingResult pushNull();

    /*!
     * @brief Reset video buffer to initial state
     */
    IasAvbProcessingResult resetBuffers();

    /*!
     * @brief Update buffer status
     */
    void updateBufferStatus();

    /*!
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLocalVideoOutStream(IasLocalVideoOutStream const &other);

    /*!
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLocalVideoOutStream& operator=(IasLocalVideoOutStream const &other);


    //
    // Members
    //

    std::string               mIpcName;           //!< Name of the IPC/stream
    IasAvbVideoShmConnection  mShmConnection;     //!< Connection providing the shared memory access
    uint32_t                  mOptimalFillLevel;  //!< Reserved for future use
    uint16_t                  mRtpSequenceNumber; //!< Continuous sequence counter
    uint32_t                  mWriteCount;        //!< Counting absolute number of packets sent
};

} // namespace IasMediaTransportAvb


#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOOUTSTREAM_HPP */
