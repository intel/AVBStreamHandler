/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamHandler.hpp
 * @brief   The definition of the IasAvbStreamHandler class.
 * @details This is THE avb stream handler.
 * @date    2013
 */

#ifndef IASAVBSTREAMHANDLER_HPP_
#define IASAVBSTREAMHANDLER_HPP_

#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <map>

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerInterface.hpp"
#include "avb_streamhandler/IasAvbStream.hpp"
#include "IasAvbStreamHandlerEventInterface.hpp"
#include "avb_streamhandler/IasAvbTypes.hpp"


namespace IasMediaTransportAvb {

class IasAvbReceiveEngine;
class IasAvbTransmitEngine;
class IasAlsaEngine;
class IasVideoStreamInterface;
class IasTestToneStream;
class IasLibPtpDaemon;
class IasAvbClockDomain;
class IasAvbStreamHandlerEnvironment;
class IasLocalAudioStream;
class IasLocalVideoStream;
class IasAvbClockController;
class IasAvbStreamHandler;

/**
 * @brief Avb Control service API directly used by streamhandler
 */
class IAS_DSO_PUBLIC IasAvbStreamHandlerControllerInterface
{
 protected:
  //@{
  /// Implementation object cannot be created or destroyed through this interface
  IasAvbStreamHandlerControllerInterface() {}
  /* non-virtual */ ~IasAvbStreamHandlerControllerInterface() {}
  //@}

  public:
    /**
     * @brief Initialize AvbController.
     *
     * @param[in] api - pointer to streamhandler API
     *    it is expected that api->registerClient(this) will be called from within init
     * @returns eIasAvbResultOk upon sucess, an error code otherwise
     */
    virtual IasAvbResult init(IasAvbStreamHandler *api) = 0;

    /**
     * @brief Cleanup any resources that were allocated with init
     *        called upon AvbController destroy (befor unregister)
     * @returns eIasAvbResultOk upon sucess, an error code otherwise
     */
    virtual IasAvbResult cleanup() = 0;

    /**
     * @brief Register service in service manager (make it accesible for clients)
     * @param[in] instanceName service name
     * @returns eIasAvbResultOk upon sucess, an error code otherwise
     */
    virtual IasAvbResult registerService(std::string const instanceName) = 0;

    /**
     * @brief Unregister service from service manager
     * @returns eIasAvbResultOk upon sucess, an error code otherwise
     */
    virtual IasAvbResult unregisterService() = 0;

};

/**
 * @brief callback API for events reported by the stream handler
 */
class IAS_DSO_PUBLIC IasAvbStreamHandlerClientInterface
{
  protected:
    //@{
    /// Implementation object cannot be created or destroyed through this interface
    IasAvbStreamHandlerClientInterface() {}
    /* non-virtual */ ~IasAvbStreamHandlerClientInterface() {}
    //@}

  public:

    /**
     * @brief update status of received stream
     *
     * @param[in] streamId Id of the stream
     * @param[in] state    new status of the stream
     */
    virtual void updateStreamStatus( uint64_t streamId, IasAvbStreamState status ) = 0;

    /**
     * @brief link state notification
     *
     * @param[in] ifUp true if network interface is up and running
     */
    virtual void updateLinkStatus( bool ifUp ) = 0;
};


class IAS_DSO_PUBLIC IasAvbStreamHandler : public IasAvbStreamHandlerInterface
                                         , public IasAvbStreamHandlerEventInterface
{
  public:

    /**
     * @brief Constructor.
     */
    explicit IasAvbStreamHandler(DltLogLevelType dltLogLevel);

    /**
     * @brief Destructor.
     */
    virtual ~IasAvbStreamHandler();

    IasAvbProcessingResult init( const std::string& configName, bool runSetup, int32_t setupArgc, char** setupArgv, const char* argv0 = "");
    IasAvbProcessingResult start( bool resume = false );
    IasAvbProcessingResult stop( bool suspend = false );
    void emergencyStop();
    /**
     * @brief activate API mutex handling except it is switched off by command line with -k option
     */
    void activateMutexHandling();

    inline bool isInitialized() const;
    inline bool isStarted() const;

    inline void sleep_ns(uint32_t ns);

    IasAvbProcessingResult triggerStorePersistenceData();


    /**
     * @brief register client for event callbacks
     *
     * @param[in] client pointer to instance implementing the callback interface
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult registerClient( IasAvbStreamHandlerClientInterface * client );

    /**
     * @brief delete registration of client for event callbacks
     *
     * @param[in] client pointer to registered client
     * @returns eIasAvbResultOk upon success, an error code otherwise
     */
    virtual IasAvbResult unregisterClient( IasAvbStreamHandlerClientInterface * client );


    //@{
    //
    /// @brief Implementation of IasAvbStreamHandlerInterface.
    /// for a detailed description of each function, see IasAvbStreamHandlerInterface.
    //

    virtual IasAvbResult createReceiveAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
        AvbStreamId streamId, MacAddress destMacAddr);

    virtual IasAvbResult createTransmitAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
        IasAvbAudioFormat format, uint32_t clockId, IasAvbIdAssignMode assignMode, AvbStreamId & streamId,
        MacAddress & destMacAddr, bool active);

    virtual IasAvbResult destroyStream(AvbStreamId streamId);

    virtual IasAvbResult createAlsaStream(IasAvbStreamDirection direction, uint16_t numberOfChannels, uint32_t sampleFreq,
                                          IasAvbAudioFormat format, uint32_t clockId, uint32_t periodSize, uint32_t numPeriods,
                                          uint8_t channelLayout, bool hasSideChannel, std::string const &deviceName,
                                          uint16_t &streamId, IasAlsaDeviceTypes alsaDeviceType, uint32_t sampleFreqASRC);

    virtual IasAvbResult createTestToneStream(uint16_t numberOfChannels,
        uint32_t sampleFreq, IasAvbAudioFormat format, uint8_t channelLayout, uint16_t &streamId );

    virtual IasAvbResult destroyLocalStream(uint16_t streamId);

    virtual IasAvbResult setStreamActive(AvbStreamId streamId, bool active);

    virtual IasAvbResult connectStreams(AvbStreamId networkStreamId, uint16_t localStreamId);

    virtual IasAvbResult disconnectStreams(AvbStreamId networkStreamId);

    virtual IasAvbResult setChannelLayout(uint16_t localStreamId, uint8_t channelLayout);

    virtual IasAvbResult setTestToneParams(uint16_t localStreamId, uint16_t channel, uint32_t signalFrequency,
        int32_t level, IasAvbTestToneMode mode, int32_t userParam );

    virtual IasAvbResult deriveClockDomainFromRxStream(AvbStreamId rxStreamId, uint32_t & clockId);

    virtual IasAvbResult setClockRecoveryParams(uint32_t masterClockId, uint32_t slaveClockId, uint32_t driverId);

    virtual IasAvbResult getAvbStreamInfo(AudioStreamInfoList &audioStreamInfo, VideoStreamInfoList &videoStreamInfo,
                                          ClockReferenceStreamInfoList &clockRefStreamInfo);

    virtual IasAvbResult getLocalStreamInfo(LocalAudioStreamInfoList &audioStreamInfo, LocalVideoStreamInfoList &videoStreamInfo);

    virtual IasAvbResult createTransmitVideoStream(IasAvbSrClass srClass, uint16_t maxPacketRate, uint16_t maxPacketSize, IasAvbVideoFormat format,
        uint32_t clockId, IasAvbIdAssignMode assignMode, uint64_t &streamId, uint64_t &dmac, bool active);

    virtual IasAvbResult createReceiveVideoStream(IasAvbSrClass srClass, uint16_t maxPacketRate, uint16_t maxPacketSize, IasAvbVideoFormat format,
        uint64_t streamId, uint64_t destMacAddr);

    virtual IasAvbResult createLocalVideoStream(IasAvbStreamDirection direction, uint16_t maxPacketRate, uint16_t maxPacketSize,
        IasAvbVideoFormat format, const std::string &ipcName, uint16_t &streamId );

    virtual IasAvbResult createTransmitClockReferenceStream(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type,
                                                            uint16_t crfStampsPerPdu, uint16_t crfStampInterval,
                                                            uint32_t baseFreq, IasAvbClockMultiplier pull,
                                                            uint32_t clockId, IasAvbIdAssignMode assignMode,
                                                            uint64_t &streamId, uint64_t &dmac, bool active);

    virtual IasAvbResult createReceiveClockReferenceStream(IasAvbSrClass srClass, IasAvbClockReferenceStreamType type,
                                                           uint16_t maxCrfStampsPerPdu, uint64_t streamId,
                                                           uint64_t dmac, uint32_t &clockId);
    //@}

    //@{
    //
    /// @brief Implementation of callback API that catches link and stream status from transmit
    ///        and receive engines. For a detailed description of functions, see IasAvbStreamHandlerEventInterface.
    //
    virtual void updateLinkStatus(const bool linkIsUp);

    virtual void updateStreamStatus(uint64_t streamId, IasAvbStreamState status);
    //@}

  private:

    enum State
    {
      eIasDead,
      eIasInitialized,
      eIasStarted
    };

    void cleanup();

    /**
     * @brief locks API Mutex if mApiMutexEnable is set.
     */
    void lockApiMutex();

    /**
     * @brief unlocks API Mutex if mApiMutexEnable is set.
     */
    void unlockApiMutex();

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbStreamHandler(IasAvbStreamHandler const &other);


    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbStreamHandler& operator=(IasAvbStreamHandler const &other);


    //
    // Private Types
    //
    typedef std::map<uint32_t, IasAvbClockDomain*> AvbClockDomains;
    typedef std::vector<IasAvbClockController*> AvbClockControllers;
    typedef std::map<uint16_t, IasTestToneStream*> TestToneStreamMap;

    //
    // Helper Methods
    //
    IasLocalAudioStream * getLocalAudioStreamById(uint16_t id);
    IasLocalVideoStream * getLocalVideoStreamById(uint16_t id);
    IasAvbClockDomain * getClockDomainById(uint32_t id);
    uint16_t getNextLocalStreamId();
    bool isLocalStreamIdInUse(uint16_t streamId);
    static IasAvbResult mapResultCode(IasAvbProcessingResult code);
    IasAvbProcessingResult createTransmitEngine();
    IasAvbProcessingResult createReceiveEngine();

    //
    // Constants
    //
    static const uint32_t cRxClockDomainIdStart = 1000u; ///< first id to be assigned to dynamically generated clock domains

    //
    // Member Variables
    //

    State                               mState;
    IasAvbReceiveEngine*                mAvbReceiveEngine;
    IasAvbTransmitEngine*               mAvbTransmitEngine;
    IasAlsaEngine*                      mAlsaEngine;
    IasVideoStreamInterface*            mVideoStreamInterface;
    TestToneStreamMap                   mTestToneStreams;
    IasAvbStreamHandlerEnvironment*     mEnvironment;
    IasAvbStreamHandlerClientInterface* mClient;
    AvbClockDomains                     mAvbClockDomains;
    uint16_t                              mNextLocalStreamId;
    uint32_t                              mNextClockDomainId;
    AvbClockControllers                 mClockControllers;
    DltContext*                         mLog;            // context for Log & Trace
    DltLogLevelType                     mDltLogLevel;       // selected DLT log level
    void*                               mConfigPluginHandle;
    bool                                mPreConfigurationInProgress;
    bool                                mBTMEnable;
    bool                                mApiMutexEnable; // 0=Off, 1=Enabled
    bool                                mApiMutexEnableConfig; // needed for command line -k option
    std::mutex                          mApiMtx;   // API mutex used for all API functions
};

inline bool IasAvbStreamHandler::isInitialized() const
{
  return (mState >= eIasInitialized);
}


inline bool IasAvbStreamHandler::isStarted() const
{
  return (mState >= eIasStarted);
}

inline void IasAvbStreamHandler::sleep_ns( uint32_t ns )
{
  // sleep some time and try again
  struct timespec req;
  struct timespec rem;
  req.tv_nsec = ns;
  req.tv_sec = 0;
  nanosleep(&req, &rem);
}



} // namespace

#endif // IASAVBSTREAMHANDLER_HPP_
