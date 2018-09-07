/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbReceiveEngine.hpp
 * @brief   The definition of the IasAvbReceiveEngine class.
 * @details This engine starts a worker thread which receives incoming
 *          AVB packets.
 * @date    2013
 */

#ifndef IASAVBRECEIVEENGINE_HPP_
#define IASAVBRECEIVEENGINE_HPP_


#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_streamhandler/IasAvbStream.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/IasIRunnable.hpp"
#include <mutex>
#include <linux/if_ether.h>

namespace IasMediaTransportAvb {

class IasLocalAudioStream;
class IasLocalVideoStream;
class IasAvbClockDomain;
class IasAvbStreamHandlerEventInterface;

class IasAvbReceiveEngine : private IasMediaTransportAvb::IasIRunnable
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAvbReceiveEngine();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbReceiveEngine();

    /**
     *  @brief Initialize method.
     *
     *  Pass component specific initialization parameter.
     *  Derived from base class.
     */
    IasAvbProcessingResult init();

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbReceiveEngine(IasAvbReceiveEngine const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbReceiveEngine& operator=(IasAvbReceiveEngine const &other);

    /**
     * @brief Starts the worker thread.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult start();

    /**
     * @brief Stops the worker thread.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult stop();

    /**
     * @brief Creates an AvbAudioStream. This stream can be used to be connected
     *        to an local audio stream.
     *
     * @param[in] srClass           Stream reservation class to be used
     * @param[in] maxNumberChannels The maximum number of channels the stream can hold
     * @param[in] sampleFreq        The frequency of the underlying samples
     * @param[in] format            The format of the data (IEC61883, SAF16, ...)
     * @param[in,out] streamId      The id of the AVB stream.
     * @param[in,out] destMacAddr   The destination MAC address
     * @param[in] preconfigured     Indicates whether the stream has been created during setup (true) or runtime (false)
     *
     * @returns   eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult createReceiveAudioStream(IasAvbSrClass srClass,
                                                    uint16_t const maxNumberChannels,
                                                    uint32_t const sampleFreq,
                                                    IasAvbAudioFormat const format,
                                                    const IasAvbStreamId & streamId,
                                                    const IasAvbMacAddress & destMacAddr,
                                                    bool preconfigured);

    /**
     * @brief Creates an AvbVideoStream. This stream can be used to be connected
     *        to an local video stream.
     *
     * @param[in] srClass           Stream reservation class to be used
     * @param[in] maxPacketRate     Maximum number of packets that will be transmitted per second
     * @param[in] maxPacketSize     Maximum size of a packet in bytes
     * @param[in] format            Format of the data (IEC61883, RTP, ...)
     * @param[in,out] streamId      The id of the AVB stream.
     * @param[in,out] destMacAddr   The destination MAC address
     * @param[in] preconfigured     Indicates whether the stream has been created during setup (true) or runtime (false)
     *
     * @returns   eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult createReceiveVideoStream(IasAvbSrClass srClass,
                                                    uint16_t const maxPacketRate,
                                                    uint16_t const maxPacketSize,
                                                    IasAvbVideoFormat const format,
                                                    const IasAvbStreamId & streamId,
                                                    const IasAvbMacAddress & destMacAddr,
                                                    bool preconfigured);

    /**
     * @brief Creates an AvbClockReferenceStream. This stream cannot be connected to local streams.
     *
     * @param[in] srClass             Stream reservation class to be used
     * @param[in] type                The type of the clock reference stream as defined in IEEE1722-rev1 11.2.6
     * @param[in] maxCrfStampsPerPdu  Maximum number of stamps to be expected in a PDU.
     * @param[in,out] streamId        The id of the AVB stream.
     * @param[in,out] destMacAddr     The destination MAC address
     *
     * @returns   eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult createReceiveClockReferenceStream(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type,
                                                             uint16_t maxCrfStampsPerPdu, const IasAvbStreamId & streamId,
                                                             const IasAvbMacAddress & destMacAddr);

    /**
     * @brief Destroys an already created AVB stream.
     *
     * @param[in] streamId The id of the AVB stream.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult destroyAvbStream(IasAvbStreamId const & streamId);

    /**
     * @brief Connects an AVB audio stream to a local audio stream.
     *
     * @param[in] avbStreamId The id of the AVB (audio) stream.
     * @param[in] localStream A pointer to the local audio stream instance
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult connectAudioStreams(const IasAvbStreamId & avbStreamId, IasLocalAudioStream * localStream);

    /**
     * @brief Connects an AVB video stream to a local audio stream.
     *
     * @param[in] avbStreamId The id of the AVB (audio) stream.
     * @param[in] localStream A pointer to the local audio stream instance
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult connectVideoStreams(const IasAvbStreamId & avbStreamId, IasLocalVideoStream * localStream);

    /**
     * @brief Disconnects an AVB stream from a local audio/video stream.
     *
     * @param[in] avbStreamId The id of the AVB stream.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult disconnectStreams(const IasAvbStreamId & avbStreamId);

    /**
     * @brief Opens the receive raw socket on the device.
     *
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult openReceiveSocket();

    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();

    /**
     * @brief register interface for event callbacks
     *
     * @param[in] interface pointer to instance implementing the callback interface
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbProcessingResult registerEventInterface( IasAvbStreamHandlerEventInterface * eventInterface );

    /**
     * @brief delete registration of interface for event callbacks
     *
     * @param[in] interface pointer to registered client
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbProcessingResult unregisterEventInterface( IasAvbStreamHandlerEventInterface * eventInterface );

    /**
     * @brief finds AVB receive stream by id
     *
     * @returns pointer to stream object or NULL if not found
     */
    inline IasAvbStream * getStreamById( IasAvbStreamId streamId );

    /**
     * @brief checks if the given stream id is valid (available)
     *
     * @returns 'true' if id is valid, otherwise 'false'
     */
    inline bool isValidStreamId(const IasAvbStreamId & avbStreamId) const;

    /**
     * @brief Populates the provided list structures with the info for one or all streams.
     *
     * Note: for the audioStreamInfo clockId field a value of '-1' is used as a debug hint either the clock domain
     * was uninitialised or that the clock domain was unavailable within the audio stream instance. A legal value of
     * '-1' would only occur if we have generated max uint32_t of dynamic clock domains instances, which is an unlikely
     * scenario.
     *
     * @param[in] id of the one stream to provide info for, otherwise value 0 to indicate all stream info.
     * @param[out] audioStreamInfo vector to populate with audio stream data.
     * @param[out] videoStreamInfo vector to populate with video stream data.
     * @param[out] clockRefStreamInfo vector to populate with clock reference stream data.
     *
     * @returns 'true' if a non-zero id is specified and found, otherwise 'false'.
     *
     */
    bool getAvbStreamInfo(const IasAvbStreamId &id, AudioStreamInfoList &audioStreamInfo,
                          VideoStreamInfoList &videoStreamInfo, ClockReferenceStreamInfoList &clockRefStreamInfo) const;

    /**
     *  @brief shutdown IGB in an emergency
     */
    void emergencyShutdown();

  private:

    struct StreamData
    {
      IasAvbStream* stream;
      IasAvbStreamState lastState;
      uint64_t lastTimeDispatched;
    };

    typedef std::map<IasAvbStreamId, StreamData> AvbStreamMap;

#if defined(DIRECT_RX_DMA)
    static const size_t cReceiveFilterDataSize = 128u;  // flexible filter maximum data length
    static const size_t cReceiveFilterMaskSize = 16u;   // flexible filter maximum mask length
    static const size_t cReceivePoolSize       = 256u;  // The number of RX packet buffers
    static const size_t cReceiveBufferSize     = 2048u; // fixed value by libigb

    enum RxQueueId
    {
      eRxQueue0 = 0u,
      eRxQueue1
    };

    enum RxFilterId
    {
      eRxFilter0 = 0u,
      eRxFilter1,
      eRxFilter2,
      eRxFilter3,
      eRxFilter4,
      eRxFilter5,
      eRxFilter6,
      eRxFilter7
    };

    typedef std::vector<IasAvbPacket*> PacketList;
#else
    static const size_t cReceiveBufferSize = ETH_FRAME_LEN + 4u; // consider VLAN TAG
#endif /* DIRECT_RX_DMA */
    ///
    /// Inherited from IasRunnable
    ///

    /**
     * Function called before the run function
     *
     * @returns Value indicating success or failure
     */
    virtual IasResult beforeRun();

    /**
     * The actual run function does the processing
     *
     *
     * Stay inside the function until all processing is finished or shutDown is called
     * If this call returns an error value, this error value is reported via the return value of \ref IasThread::start()
     * In case of an error, the thread still needs to be shutdown explicitly through calling \ref IasThread::stop()
     *
     * @returns Value indicating success or failure
     *
     * @note This value can be accessed through \ref IasThread::getRunThreadResult
     */
    virtual IasResult run();

    /**
     * ShutDown code
     * called when thread is going to be terminated
     *
     * Exit the \ref run function when this function is called
     *
     * @returns Value indicating success or failure
     *
     */
    virtual IasResult shutDown();

    /**
     * Function called after the run function
     *
     * @returns Value indicating success or failure
     *
     * If this call returns an error value and run() was successful, it is reported via the return value of IasThread::stop()
     */
    virtual IasResult afterRun();

    /**
     * @brief close receive socket
     */
    inline void closeSocket();

    /**
     * @brief dispatch received packet to AvbStream
     * @returns true if packet has been marked valid by AvbStream
     */
    bool dispatchPacket(StreamData &streamData, const void* packet, size_t length, uint64_t now);

    /**
     * @brief checks for a change in stream status and notifies client
     * @returns true if AvbStream is in valid state
     */
    bool checkStreamState(StreamData &streamData);

    /**
     * @brief checks if streamId is already in use
     * @returns eIasAvbProcOK streamId is still available
     * @returns eIasAvbProcAlreadyInUse streamId is already in use
     */
    IasAvbProcessingResult checkStreamIdInUse(const IasAvbStreamId & streamId) const;

    /**
     * @brief lock access to the stream list
     */
    inline void lock();

    /**
     * @brief unloack access to the stream list
     */
    inline void unlock();


#if defined(DIRECT_RX_DMA)
    /**
     * @brief Starts the I210's receive engine.
     * @returns eIasAvbProcOK on success, otherwise an error will be returned.
     */
    IasAvbProcessingResult startIgbReceiveEngine();

    /**
     * @brief Stop the I210's receive engine.
     */
    void stopIgbReceiveEngine();
#endif /* DIRECT_RX_DMA */

    /**
     * @brief Accept multicast packets arriving at the MAC address
     */
    IasAvbProcessingResult bindMcastAddr(const IasAvbMacAddress &mCastMacAddr, bool bind = true);

    /**
     * @brief Drop multicast packets arriving at the MAC address
     */
    inline IasAvbProcessingResult unbindMcastAddr(const IasAvbMacAddress &mCastMacAddr);

    //
    // Member Variables
    //

    std::string const       mInstanceName;
    bool               mEndThread;
    IasThread   * mReceiveThread;
    AvbStreamMap       mAvbStreams;
    std::mutex         mLock;
    IasAvbStreamHandlerEventInterface * mEventInterface;
    int32_t              mReceiveSocket;
    uint8_t            * mReceiveBuffer;
    bool               mIgnoreStreamId;
    DltContext        *mLog;           // context for Log & Trace

#if defined(DIRECT_RX_DMA)
    device_t         * mIgbDevice;
    IasAvbPacketPool * mRcvPacketPool;
    PacketList         mPacketList;
    bool               mRecoverIgbReceiver;
#endif /* DIRECT_RX_DMA */
    int32_t              mRcvPortIfIndex;
};

inline void IasAvbReceiveEngine::closeSocket()
{
  if (-1 != mReceiveSocket)
  {
    close(mReceiveSocket);
    mReceiveSocket = -1;
  }
}

inline IasAvbStream * IasAvbReceiveEngine::getStreamById( IasAvbStreamId streamId )
{
  IasAvbStream *ret = NULL;

  AvbStreamMap::iterator it = mAvbStreams.find(streamId);
  if (it != mAvbStreams.end())
  {
    ret = it->second.stream;
  }

  return ret;
}

inline bool IasAvbReceiveEngine::isValidStreamId(const IasAvbStreamId & avbStreamId) const
{
  return (mAvbStreams.end() != mAvbStreams.find(avbStreamId));
}

inline IasAvbProcessingResult IasAvbReceiveEngine::unbindMcastAddr(const IasAvbMacAddress &mCastMacAddr)
{
  return bindMcastAddr(mCastMacAddr, false);
}

inline void IasAvbReceiveEngine::lock()
{
  mLock.lock();
}

inline void IasAvbReceiveEngine::unlock()
{
  mLock.unlock();
}


} // namespace IasMediaTransportAvb

#endif /* IASAVBRECEIVEENGINE_HPP_ */
