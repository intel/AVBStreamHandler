/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoShmConnection.hpp
 * @brief   Connection between application and shared memory.
 * @details This class is meant to be the link between the application and the data exchange mechanism that
 *          transports data across process boundaries. Basically there is a "server" and a "client" side where
 *          the server creates the data exchange object (which is a shared memory in this case) and the client
 *          connects to the shared memory object. This connector class is used from both sides, the server
 *          and client side.
 * .
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOCONNECTOR_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOCONNECTOR_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_video_common/IasAvbVideoCommonTypes.hpp"
#include "avb_video_common/IasAvbVideoRingBuffer.hpp"

#include <dlt/dlt.h>


namespace IasMediaTransportAvb {


class IasAvbVideoShmConnection
{
  public:
    /*!
     * @brief Constructor.
     *
     * @param[in] isCreator Indicates whether the shared memory object shall be created (true) or find (false).
     */
    explicit IasAvbVideoShmConnection(bool isCreator);

    /*!
     *  @brief Destructor, virtual by default.
     */
    ~IasAvbVideoShmConnection();

    /*!
     * @brief   Initializes the connection.
     *
     * @param[in] connectionName the name of the connection. It is also the name of the shared memory.
     * @param[in] groupName the name of the group that is allowed to access the shared memory.
     *
     * @returns   eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoCommonResult init(const std::string & connectionName, const std::string& groupName = "ias_avb");

    /*!
     * @brief   Cleans up resources acquired.
     *
     * @returns The write offset.
     */
    void cleanup();

    /*!
     * @brief   Create a ring buffer object (server side).
     *          Befor calling this the init method has to be called.
     *
     * @param[in] numPackets number of buffers (packets) the shm shall contain.
     * @param[in] maxPacketSize the size of a single element/buffer (packet).
     *
     * @returns   eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoCommonResult createRingBuffer(uint32_t numPackets, uint32_t maxPacketSize);

    /*!
     * @brief   Find an already created ring buffer object (client side).
     *          Befor calling this the init method has to be called.
     *
     * @param[in] numPackets number of buffers (packets) the shm shall contain.
     * @param[in] maxPacketSize the size of a single element/buffer (packet).
     *
     * @returns   eIasRingBuffOk on success, otherwise an error code.
     */
    IasVideoCommonResult findRingBuffer();


    /*!
     * @brief   Retrieve the ringBuffer instance.
     *
     * @returns the a pointer to the instance of the ring buffer.
     */
    inline IasAvbVideoRingBuffer *getRingBuffer() { return mRingBuffer; }

  private:

    /*!
     *  @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoShmConnection(IasAvbVideoShmConnection const &other); //lint !e1704

    /*!
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoShmConnection& operator=(IasAvbVideoShmConnection const &other); //lint !e1704


    //
    // Member variables
    //

    DltContext              *mLog;                    //!< Pointer to the DLT context
    IasAvbVideoRingBuffer   *mRingBuffer;             //!< Pointer to the ring buffer instance
    bool                    mIsCreator;               //!< indicator whether the shm shall be created or find
    std::string             mConnectionName;          //!< The connection name, also the name of the ring buffer
    std::string             mGroupName;               //!< The group name, this user group has permission to access
};


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOCONNECTOR_HPP */




