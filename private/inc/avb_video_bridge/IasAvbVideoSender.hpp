/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoSender.hpp
 * @brief   Definition of the video sender.
 * @details The video sender is created by a video client application that uses the video bridge library to connect
 *          to the AVB Streamhandler. Therefore it has to pass a sender role name. This role name is used as
 *          the name for the underlying data exchange mechanism (e.g. shared memory) and basically the name of the
 *          video stream handled by this sender instance, respectively.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEOSENDER_HPP
#define IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEOSENDER_HPP

#include <string>
#include <atomic>
#include "avb_video_common/IasAvbVideoShmConnection.hpp"
#include "media_transport/avb_video_bridge/IasAvbVideoBridge.h"


namespace IasMediaTransportAvb {

class __attribute__ ((visibility ("default"))) IasAvbVideoSender
{
  public:

   /*!
    * @brief constructor
    *
    * @param[in] senderRole The name of the sender and so the name of the video stream.
    */
    explicit IasAvbVideoSender(const char* senderRole);

    /*!
     * @brief destructor
     */
    ~IasAvbVideoSender();

    /*!
     * @brief   Inits the video sender class.
     *
     * @returns IAS_AVB_RES_OK on success, otherwise an error code.
     */
    ias_avbvideobridge_result init();

    /*!
     * @brief Passes an H.264 packet to be sent by AVB Streamhandler.
     *
     * @param[in] packet The pointer to the H.264 video packet to be sent over AVB.
     *
     * @returns IAS_AVB_RES_OK on success, otherwise an error code.
     */
    ias_avbvideobridge_result sendPacketH264(ias_avbvideobridge_buffer const * packet);

    /*!
     * @brief Passes an MPEG-TS packet to be sent by AVB Streamhandler.
     *
     * @param[in] packet The pointer to the MPEG-TS video packet to be sent over AVB.
     *
     * @returns IAS_AVB_RES_OK on success, otherwise an error code.
     */
    ias_avbvideobridge_result sendPacketMpegTs(bool sph, ias_avbvideobridge_buffer const * packet);

    // not used at the moment
    uint32_t getInstCounter() { return  uint32_t (mNumberInstances); }

  private:

    /*!
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoSender(IasAvbVideoSender const &other);

    /*!
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoSender& operator=(IasAvbVideoSender const &other);


    //
    // Member variables
    //

    // not used at the moment
    static std::atomic_int    mNumberInstances;   //!< Count the maximum allowed instances of IasAvbBridge
    std::string               mSenderRole;        //!< Name of the video stream
    IasAvbVideoShmConnection  mShmConnection;     //!< The connection providing the data exchange mechanism (shm)
    IasAvbVideoRingBuffer     *mRingBuffer;       //!< Pointer to the ring buffer
};

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBVIDEOBRIDGE_AVBVIDEOSENDER_HPP */
