/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBufferFactory.hpp
 * @brief   Creates video ring buffer instances.
 * @details With the help of IasMemoryAllocator lend from audio/common it creates video ring buffer instances
 *          that are used to transfer video date across process boundaries.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERFACTORY_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERFACTORY_HPP


#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_video_common/IasAvbVideoCommonTypes.hpp"
#include <dlt/dlt.h>
#include <map>


namespace IasAudio {
class IasMemoryAllocator;
}


/*!
 * @brief namespace IasMediaTransportAvb
 */
namespace IasMediaTransportAvb {


class IasAvbVideoRingBuffer;


using IasMemoryAllocatorMap = std::map<IasAvbVideoRingBuffer*, IasAudio::IasMemoryAllocator*>;


class __attribute__ ((visibility ("default"))) IasAvbVideoRingBufferFactory
{
  public:
    /**
     * @brief the function return the pointer to the factory. If not yet created, the factory will be created
     *
     * @returns pointer to the factory
     */
    static IasAvbVideoRingBufferFactory* getInstance();

    /**
     * @brief function to create and init a ring buffer
     *
     * @param[in] rinbuffer pointer where the created object is stored
     * @param[in] packetSize packet size in bytes
     * @param[in] numPackets the number of packets
     * @param[in] name of the shared memory
     *
     * @return cInitFailed FileDescriptor could not be created.
     */
    IasVideoCommonResult createRingBuffer(IasAvbVideoRingBuffer **ringbuffer,
                                          uint32_t packetSize,
                                          uint32_t numPackets,
                                          std::string name,
                                          std::string groupName);

    /*!
     * @brief The function destroys a ring buffer
     *
     * @param ringBuf pointer to the buffer that shall be destroyed
     */
    void destroyRingBuffer(IasAvbVideoRingBuffer *ringBuf);

    /*!
     * @brief the function is used to find a ring buffer in shared memory
     *
     * @param[in] name the name of the shared memory
     *
     * @returns   eIasRingBuffOk on success, otherwise an error code.
     */
    IasAvbVideoRingBuffer *findRingBuffer(const std::string name);

    /*!
     * @brief The function is used to "lose" a ring buffer which was previously retrieved by findRingBuffer
     *
     * Losing a ring buffer is used on the client side to delete the ring buffer instance, but not the shared memory
     * associated with it.
     *
     * @param[in] ringBuf A pointer to the ring buffer which has to be lost.
     */
    void loseRingBuffer(IasAvbVideoRingBuffer* ringBuf);

  private:
    /*!
     * @brief destructor
     */
    ~IasAvbVideoRingBufferFactory();

    /*!
     * @brief constructor
     */
    IasAvbVideoRingBufferFactory();

    /*!
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoRingBufferFactory( IasAvbVideoRingBufferFactory const &other );

    /*!
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoRingBufferFactory& operator=(IasAvbVideoRingBufferFactory const &other);


    //
    // Member variables
    //

    DltContext              *mLog;       //!< The DLT log context
    IasMemoryAllocatorMap   mMemoryMap;  //!< map where the allocated memories and the ring buffer pointers are stored

};

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERFACTORY_HPP */
