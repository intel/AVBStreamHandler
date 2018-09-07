/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasVideoStreamInterface.hpp
 * @brief   This class ...
 * @details The ...
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_VIDEOSTREAMINTERFACE_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_VIDEOSTREAMINTERFACE_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_streamhandler/IasLocalVideoInStream.hpp"
#include "avb_streamhandler/IasLocalVideoOutStream.hpp"

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"


namespace IasMediaTransportAvb {

class IasVideoStreamInterface
{
  public:
    /**
     *  @brief Constructor.
     */
    IasVideoStreamInterface();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasVideoStreamInterface();

    /**
     *  @brief Initialize method.
     */
    virtual IasAvbProcessingResult init();

    /*!
     * @brief Starts operation.
     */
    IasAvbProcessingResult start();

    /*!
     * @brief Stops operation.
     */
    IasAvbProcessingResult stop();

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
      *  @brief Creates a video stream buffer container
      */
    IasAvbProcessingResult createVideoStream(IasAvbStreamDirection direction,
                                             uint16_t maxPacketRate,
                                             uint16_t maxPacketSize,
                                             IasAvbVideoFormat format,
                                             std::string ipcName,
                                             uint16_t streamId);

    /**
      *  @brief Creates a video IN stream buffer container
      */
    IasAvbProcessingResult createVideoInStream(uint16_t maxPacketRate,
                                             uint16_t maxPacketSize,
                                             IasAvbVideoFormat format,
                                             std::string ipcName,
                                             uint16_t streamId);

    /**
      *  @brief Creates a video OUT stream buffer container
      */
    IasAvbProcessingResult createVideoOutStream(uint16_t maxPacketRate,
                                             uint16_t maxPacketSize,
                                             IasAvbVideoFormat format,
                                             std::string ipcName,
                                             uint16_t streamId);

    /**
     *  @brief Get local video stream element
     */
    IasLocalVideoStream * getLocalVideoStream(uint16_t streamId);

    /**
     *  @brief Destroy local video stream element
     */
    IasAvbProcessingResult destroyVideoStream(uint16_t streamId);

    /**
     * @brief Get parameters of all available video streams
     * @param streamId Id of specific stream searched for; 0 if all streams should be returned
     * @param infoList Output list of stream attributes
     * @return 'true' if a non-zero id is specified and found, otherwise 'false'
     */
    bool getLocalStreamInfo(const uint16_t &streamId, LocalVideoStreamInfoList &infoList) const;

  private:
     /*!
      *  @brief Copy constructor, private unimplemented to prevent misuse.
      */
    IasVideoStreamInterface(IasVideoStreamInterface const &other);

     /*!
      *  @brief Assignment operator, private unimplemented to prevent misuse.
      */
    IasVideoStreamInterface& operator=(IasVideoStreamInterface const &other);

    DltContext       *mLog;
    bool             mIsInitialized;
    bool             mRunning;

    typedef std::map<uint16_t, IasLocalVideoInStream*> VideoInStreamMap;
    VideoInStreamMap   mVideoInStreams;

    typedef std::map<uint16_t, IasLocalVideoOutStream*> VideoOutStreamMap;
    VideoOutStreamMap   mVideoOutStreams;
};


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_VIDEOSTREAMINTERFACE_HPP */




