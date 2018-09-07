/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoStream.hpp
 * @brief   This class contains all methods to handle a local video stream
 * @details A local video stream is defined as an video data container which
 *          can be connected to an AvbVideoStream. The source of the data is an
 *          video interface. The supported video format is h264 due to P1722a
 *
 * @date    2014
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOSTREAM_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_LOCALVIDEOSTREAM_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "avb_streamhandler/IasLocalVideoBuffer.hpp"

namespace IasMediaTransportAvb {


/**
 * @brief callback interface class for clients of the LocalVideoStream (i.e. IasAvbVideoStream)
 */
class IasLocalVideoStreamClientInterface
{
  protected:
    IasLocalVideoStreamClientInterface() {}
    virtual ~IasLocalVideoStreamClientInterface() {}
  public:
    enum DiscontinuityEvent
    {
      eIasUnspecific,
      eIasOverrun,
      eIasUnderrun
    };

    /**
     * @brief LocalStream indicates to client that an discontinuity occurred
     *
     * A discontinuity occurs when an amount of samples is missing in the stream due to
     * an overrun, underrun or other events (if any). The client can then decide whether
     * if wants to reset the ring buffer to the optimal fill level (which means that even more
     * samples get lost) or whether it tries to conceal the problem by losing as few samples
     * as possible, without a ring buffer reset.
     *
     * @param[in] event code indicating the type of event
     * @param[in] number of affected samples or 0 if unknown
     * @returns true if ring buffer shall be reset, false otherwise
     */
    virtual bool signalDiscontinuity( DiscontinuityEvent event, uint32_t numSamples ) = 0;

};


/**
 * @brief base class for all local stream classes, provides buffering & connection functionality
 */
class IasLocalVideoStream
{
  public:
    enum ClientState
    {
      eIasNotConnected, ///< no network stream connected to the local stream
      eIasIdle,         ///< network stream connected, but not reading/writing data
      eIasActive        ///< network stream connected and reading/writing data
    };

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLocalVideoStream();

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief write data to local video buffer
     *
     * @param[in] buffer          pointer to the buffer containing the video data to be copied into the ring buffer
     * @param[in] descPacket      descriptor packet which contains specific video information
     */
    virtual IasAvbProcessingResult writeLocalVideoBuffer(IasLocalVideoBuffer * buffer, IasLocalVideoBuffer::IasVideoDesc * descPacket);

    /**
     * @brief read data to local video buffer
     *
     * @param[in] buffer          pointer to the buffer containing the video data to be copied into the ring buffer
     * @param[in] descPacket      descriptor packet which contains specific video information
     */
    virtual IasAvbProcessingResult readLocalVideoBuffer(IasLocalVideoBuffer * buffer, IasLocalVideoBuffer::IasVideoDesc * descPacket);

    inline bool isInitialized() const;
    inline IasAvbStreamDirection getDirection() const;
    inline uint16_t getMaxPacketSize() const;
    inline uint16_t getMaxPacketRate() const;
    inline IasLocalStreamType getType() const;
    inline uint16_t  getStreamId() const;
    inline IasLocalVideoBuffer * getLocalVideoBuffer();

    //
    // methods to be used by client
    //
    /**
     * @brief called by client to reset all current local video buffers to a start position.
     * @returns eIasAvbProcOK
     */
    virtual IasAvbProcessingResult resetBuffers() = 0;

    /**
     * @brief called by client to register at the local stream upon connection
     *
     * @param[in] client instance pointer of the object implementing the client interface
     * @returns eIasAvbProcOK on success
     * @returns eIasAvbProcInvalidParam on invalid client pointer
     * @returns eIasAvbProcAlreadyInUse when already connected
     */
    virtual IasAvbProcessingResult connect( IasLocalVideoStreamClientInterface * client );

    /**
     * @brief called by client to unregister at the local stream upon disconnection
     * @returns eIasAvbProcOK
     */
    virtual IasAvbProcessingResult disconnect();

    /**
     * @brief notifies local video stream about activity state of client
     *
     * A change from inactive to active resets the ring buffer to optimal fill level.
     * In inactive state, the local stream will not report discontinuity events to the client.
     *
     * @param[in] active true if client is now active, false otherwise
     */
    virtual void setClientActive( bool active );

    /**
     * @brief shows stream connection status
     */
    inline bool isConnected() const;

    /**
     * @brief set the avbPacketPool pointer for payload pointer access
     */
    IasAvbProcessingResult setAvbPacketPool(IasAvbPacketPool * avbPacketPool);

    /**
     * @brief get the video format
     */
    inline IasAvbVideoFormat getFormat() const;

protected:

    /**
     *  @brief Constructor.
     */
    IasLocalVideoStream(DltContext &dltContext, IasAvbStreamDirection direction, IasLocalStreamType type,
        uint16_t streamId);

    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *  Derived from base class.
     */
    IasAvbProcessingResult init(IasAvbVideoFormat format, uint16_t numPackets, uint16_t maxPacketRate, uint16_t maxPacketSize, bool internalBuffers);

    inline ClientState getClientState() const;
    inline IasLocalVideoStreamClientInterface * getClient() const;

    //
    // Members shared with derived class
    //
    DltContext                    *mLog;
    const IasAvbStreamDirection   mDirection;
    const IasLocalStreamType      mType;
    const uint16_t                mStreamId;
    uint16_t                      mMaxPacketRate;
    uint16_t                      mMaxPacketSize;
    IasLocalVideoBuffer*          mLocalVideoBuffer;
    IasAvbVideoFormat             mFormat;

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLocalVideoStream(IasLocalVideoStream const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLocalVideoStream& operator=(IasLocalVideoStream const &other);

    //
    // Members
    //
    ClientState                 mClientState;
    IasLocalVideoStreamClientInterface * mClient;
};

inline bool IasLocalVideoStream::isInitialized() const
{
  return (0 != mMaxPacketSize);
}

inline IasLocalVideoBuffer * IasLocalVideoStream::getLocalVideoBuffer()
{
  return mLocalVideoBuffer;
}

inline IasAvbStreamDirection IasLocalVideoStream::getDirection() const
{
  return mDirection;
}

inline uint16_t IasLocalVideoStream::getMaxPacketRate() const
{
  return mMaxPacketRate;
}

inline uint16_t IasLocalVideoStream::getMaxPacketSize() const
{
  return mMaxPacketSize;
}

inline IasLocalStreamType IasLocalVideoStream::getType() const
{
  return mType;
}

inline uint16_t IasLocalVideoStream::getStreamId() const
{
  return mStreamId;
}

inline IasLocalVideoStream::ClientState IasLocalVideoStream::getClientState() const
{
  return mClientState;
}

inline IasLocalVideoStreamClientInterface * IasLocalVideoStream::getClient() const
{
  return mClient;
}

inline bool IasLocalVideoStream::isConnected() const
{
  return (eIasNotConnected != mClientState);
}

inline IasAvbVideoFormat IasLocalVideoStream::getFormat() const
{
  return mFormat;
}


struct IasLocalVideoStreamAttributes
{
public:
    /*!
     * @brief Default constructor.
     */
    IasLocalVideoStreamAttributes();

    /*!
     * @brief Copy constructor.
     */
    IasLocalVideoStreamAttributes(IasLocalVideoStreamAttributes const & iOther);

    /*!
     * \brief argument constructor
     * \param iDirection
     * \param iiType
     * \param iiStreamId
     * \param iiFormat
     * \param iiMaxPacketRate
     * \param iiMaxPacketSize
     * \param iiInternalBuffers
     */
    IasLocalVideoStreamAttributes(IasAvbStreamDirection direction, IasLocalStreamType type,
                                  uint16_t streamId, IasAvbVideoFormat format,
                                  uint16_t maxPacketRate, uint16_t maxPacketSize, bool internalBuffers);

    /*!
     * @brief Assignment operator.
     */
    IasLocalVideoStreamAttributes & operator = (IasLocalVideoStreamAttributes const & iOther);

    /*!
     * @brief Equality comparison operator.
     */
    bool operator == (IasLocalVideoStreamAttributes const & iOther) const;

    /*!
     * @brief Inequality comparison operator.
     */
    bool operator != (IasLocalVideoStreamAttributes const & iOther) const;


    inline IasAvbStreamDirection getDirection() const { return direction; }
    inline void setDirection(const IasAvbStreamDirection &value) { direction = value; }

    inline IasLocalStreamType getType() const { return type; }
    inline void setType(const IasLocalStreamType &value) { type = value; }

    inline uint16_t getStreamId() const { return streamId; }
    inline void setStreamId(const uint16_t &value) { streamId = value; }

    inline IasAvbVideoFormat getFormat() const { return format; }
    inline void setFormat(const IasAvbVideoFormat &value) { format = value; }

    inline uint16_t getMaxPacketRate() const { return maxPacketRate; }
    inline void setMaxPacketRate(const uint16_t &value) { maxPacketRate = value; }

    inline uint16_t getMaxPacketSize() const { return maxPacketSize; }
    inline void setMaxPacketSize(const uint16_t &value) { maxPacketSize = value; }

    inline bool getInternalBuffers() const { return internalBuffers; }
    inline void setInternalBuffers(bool value) { internalBuffers = value; }

private:
    IasAvbStreamDirection direction;
    IasLocalStreamType type;
    uint16_t streamId;
    IasAvbVideoFormat format;
    uint16_t maxPacketRate;
    uint16_t maxPacketSize;
    bool internalBuffers;
};

typedef std::vector<IasLocalVideoStreamAttributes> LocalVideoStreamInfoList;

} // namespace IasMediaTransportAvb

#endif /* IASLOCALVIDEOSTREAM_HPP_ */
