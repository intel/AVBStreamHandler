/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaEngine.hpp
 * @brief   This class contains all methods to handle the Alsa audio data interface
 * @details This engine administers Alsa streams and their associated worker threads
 *          which take care about the timing generation for the Alsa devices (play-
 *          back and capture).
 * @date    2015
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAENGINE_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAENGINE_HPP

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAlsaStreamInterface.hpp"
#include "avb_streamhandler/IasAlsaVirtualDeviceStream.hpp"
#include "avb_streamhandler/IasAlsaHwDeviceHandler.hpp"

namespace IasMediaTransportAvb {


class IasAlsaWorkerThread;
class IasAvbClockDomain;

class /*IAS_DSO_PUBLIC*/ IasAlsaEngine
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAlsaEngine();


    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAlsaEngine();


    /**
     *  @brief Initialize method.
     *
     */
    virtual IasAvbProcessingResult init();


    /**
     * @brief Starts operation.
     */
    IasAvbProcessingResult start();


    /**
     *  @brief Clean up all allocated resources.
     */
    void cleanup();


    /**
     * @brief Stops operation.
     */
    IasAvbProcessingResult stop();


    /**
     * @brief Returns a local audio stream element referenced by its stream id
     *
     * @param[in] streamId ID of the stream
     *
     * @returns a pointer to the local audio stream
     */
    IasLocalAudioStream * getLocalAudioStream(uint16_t streamId);


    /**
     * @brief Creates a Alsa stream buffer container
     *
     * @param[in] direction Direction ('to network' or 'from network') of the stream
     * @param[in] numChannels Number of audio channels
     * @param[in] sampleFrequency Sample frequency of the containing audio samples
     * @param[in] format format that is beeing used (e.g. SAF)
     * @param[in] periodSize Size of one Alsa period (e.g. 256)
     * @param[in] numPeridos size of the IPC buffer in periods
     * @param[in] channelLayout Layout of the channels within this stream
     * @param[in] hasSideChannel 'true' if this channel contains side channel information
     * @param[in] deviceName Name of the Alsa device
     * @param[in] streamId ID of the stream
     * @param[in] clockDomain Specifies the Clock domain to be used
     * @param[in] alsaDeviceType switch between virtual (AlsaPlugin) or HW Alsa Device with/without (ASRC)
     * @param[in] sampleFreqASRC sample frequency for ASRC in Hz
     *
     *
     * @returns 'eIasAvbProcOK' on success or an appropriate error value otherwise
     */
    IasAvbProcessingResult createAlsaStream(IasAvbStreamDirection direction, uint16_t numChannels, uint32_t sampleFrequency,
                                            IasAvbAudioFormat format, uint32_t periodSize, uint32_t numPeriods,
                                            uint8_t channelLayout,bool hasSideChannel, std::string deviceName,
                                            uint16_t streamId, IasAvbClockDomain * const clockDomain, IasAlsaDeviceTypes alsaDeviceType, uint32_t sampleFreqASRC);


    /**
     * @brief Destroys an Alsa stream buffer container
     *
     * @param[in] streamId Id of the stream to be destroyed
     * @param[in] forceDestruction If set to 'true' the stream will be destroyed regardless if it is connected or not
     *
     * @returns 'eIasAvbProcOK' on success or an appropriate error value otherwise
     */
    IasAvbProcessingResult destroyAlsaStream(uint16_t streamId, bool forceDestruction);

    /**
     * @brief Get parameters of all available alsa streams
     * @param streamId Id of specific stream searched for; 0 if all streams should be returned
     * @param infoList Output list of stream attributes
     * @return 'true' if a non-zero id is specified and found, otherwise 'false'
     */
    bool getLocalStreamInfo(const uint16_t &streamId, LocalAudioStreamInfoList &infoList) const;


  private:
    /**
     *  @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAlsaEngine(IasAlsaEngine const &other); //lint !e1704


    /**
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAlsaEngine& operator=(IasAlsaEngine const &other); //lint !e1704


    /**
     *  @brief Helper function to locate the worker thread handling the stream
     */
    IasAlsaWorkerThread *getWorkerThread(uint16_t streamId);


    /**
     *  @brief Assigns the specified Alsa stream to a matching worker thread
     *
     *  @param[in] alsaStream Pointer to the worker thread object
     *  @param[in] clockDomain Pointer to the clock domain used with this stream
     *  @param[in] periodSize Size of
     *  @param[in] sampleFrequency
     *
     * @returns 'eIasAvbProcOK' on success or an appropriate error value otherwise
     */
    IasAvbProcessingResult assignToWorkerThread(IasAlsaStreamInterface *alsaStream, IasAvbClockDomain * const clockDomain);


    /**
     *  @brief Removes the specified stream from the worker thread that handles the stream
     */
    IasAvbProcessingResult removeFromWorkerThread(const IasAlsaStreamInterface * const stream);


    static const uint32_t cMinNumberAlsaBuffer;

    //
    // Member variables
    //
    typedef std::vector<IasAlsaWorkerThread*> WorkerThreadList;
    //typedef std::map<uint16_t, IasAlsaStreamInterface*> AlsaStreamMap;
    typedef std::map<uint16_t, IasAlsaVirtualDeviceStream*> AlsaViDevStreamMap;
    typedef std::map<uint16_t, IasAlsaHwDeviceHandler*> AlsaHwDevStreamMap;

    DltContext          *mLog;
    bool                mIsInitialized;     // Indicates whether the engine is initialized or not
    bool                mIsStarted;         // Indicates whether the engine is started or not
    WorkerThreadList    mWorkerThreads;     // List of worker threads created and administered by this instance
    AlsaViDevStreamMap  mAlsaViDevStreams;  // list of Alsa streams created by this instance (virtual Alsa devices)
    AlsaHwDevStreamMap  mAlsaHwDevStreams;  // list of Alsa streams from external Alsa HW Device
    std::mutex          mLock;
};

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_ALSAENGINE_HPP */




