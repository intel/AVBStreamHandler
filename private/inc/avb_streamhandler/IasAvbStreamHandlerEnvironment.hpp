/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbStreamHandlerEnvironment.hpp
 * @brief   The definition of the IasAvbStreamHandlerEnvironment class.
 * @details This environment provides access to igb device and PTP proxy.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBSTREAMHANDLERENVIRONMENT_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBSTREAMHANDLERENVIRONMENT_HPP

#include "media_transport/avb_streamhandler_api/IasAvbConfigRegistryInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbClockDriverInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbRegistryKeys.hpp"
#include "IasAvbTypes.hpp"
#include <map>
#include <dlt.h>

extern "C" {
#include "igb.h"
}

namespace IasMediaTransportAvb {

class IasLibPtpDaemon;
class IasLibMrpDaemon;
class IasDiaLogger;

//@{
/**
 * @brief internal registry keys
 *
 * These are not part of the registry interface, since this interface is public
 * and we don't want to disclose all possible keys to the user.
 * ATTENTION: all characters in -k Sting must be in lower case!
 */
namespace IasRegKeys {
static const char cBootTimeMeasurement[] = "debug.boottime.enable"; // bool
static const char cAudioSaturate[] = "audio.tx.saturate"; // bool
static const char cAudioTstampBuffer[] = "audio.tstamp.buffer"; // time-aware buffer (0 = disable, 1 = fail-safe, 2 = hard)
static const char cAudioBaseFillMultiplier[] = "audio.basefill.multiplier"; // threshold to allow read access to the local audio buffer (default 15)
static const char cAudioBaseFillMultiplierTx[] = "audio.basefill.multiplier.tx"; // overwrite cAudioBaseFillMultiplier for xmit streams
static const char cCrfRxHoldoff[] = "crf.rx.holdoff"; // ms
static const char cAudioMaxBend[] = "clock.maxbend"; // ppm
static const char cAudioBendRate[] = "clock.bendrate"; // 1-999
static const char cBendCtrlStream[] = "clock.bend.stream"; // streamId
static const char cClockCtrlUpperLimit[] = "clockdriver.control.upperlimitppm"; // ppm
static const char cClockCtrlLowerLimit[] = "clockdriver.control.lowerlimitppm"; // ppm
static const char cClockCtrlWaitInterval[] = "clockdriver.control.waitintervalusec"; // us
static const char cClockCtrlHoldOff[] = "clockdriver.control.holdoffusec"; // us
static const char cClockCtrlGain[] = "clockdriver.control.gain"; // 1e-9/sample (@48kHz)
static const char cClockCtrlCoeff1[] = "clockdriver.control.coeff1"; // 1e-6
static const char cClockCtrlCoeff2[] = "clockdriver.control.coeff2"; // 1e-6
static const char cClockCtrlCoeff3[] = "clockdriver.control.coeff3"; // 1e-6
static const char cClockCtrlCoeff4[] = "clockdriver.control.coeff4"; // 1e-6
static const char cClockCtrlLockCount[] = "clockdriver.control.lockcount"; // num cycles
static const char cClockCtrlLockThres[] = "clockdriver.control.lockthres"; // ppm
static const char cClockCtrlEngage[] = "clockdriver.control.engage"; // 1=on (default), 0=off
static const char cClkRecoverFrom[] = "clock.recover.from"; // streamId
static const char cClkRecoverUsing[] = "clock.recover.using"; // clockId
static const char cClkSwTimeConstant[] = "clockdomain.sw.timeconstant"; // ms
static const char cClkSwDeviationUnlock[] = "clockdomain.sw.deviation.unlock";
static const char cClkSwDeviationLongterm[] = "clockdomain.sw.deviation.longterm";
static const char cClkSwLockTreshold1[] = "clockdomain.sw.lock.threshold1"; // ppm
static const char cClkSwLockTreshold2[] = "clockdomain.sw.lock.threshold2"; // ppm
static const char cClkHwTimeConstant[] = "clockdomain.hw.timeconstant"; // ms
static const char cClkHwDeviationUnlock[] = "clockdomain.hw.deviation.unlock";
static const char cClkHwDeviationLongterm[] = "clockdomain.hw.deviation.longterm";
static const char cClkHwLockTreshold1[] = "clockdomain.hw.lock.threshold1"; // ppm
static const char cClkHwLockTreshold2[] = "clockdomain.hw.lock.threshold2"; // ppm
static const char cClkRxTimeConstant[] = "clockdomain.rx.timeconstant"; // ms
static const char cClkRxDeviationUnlock[] = "clockdomain.rx.deviation.unlock";
static const char cClkRxDeviationLongterm[] = "clockdomain.rx.deviation.longterm";
static const char cClkRxLockTreshold1[] = "clockdomain.rx.lock.threshold1"; // ppm
static const char cClkRxLockTreshold2[] = "clockdomain.rx.lock.threshold2"; // ppm
static const char cClkRawXTimestamp[] = "clockdomain.raw.xtstamp"; // cross timestamps support for RawClkDomain 1=enable, 0=disable (default)
static const char cClkRawDeviationUnlock[] = "clockdomain.raw.deviation.unlock"; // ppm
static const char cClkRawRatioToPtp[] = "clockdomain.raw.ratio.ptp"; // initial clock rate ratio to ppm (= ptp-clkrate/raw-clkrate * 10,000,000)
static const char cClkRawTscFreq[] = "clockdomain.raw.tscfreq"; // Hz
static const char cClkRawXtstampThresh[] = "clockdomain.raw.xtstamp.threshold"; // ns
static const char cCompatibilityAudio[] = "compatibility.audio"; // "SAF" or "d6_1722a"
static const char cCompatibilityVideo[] = "compatibility.video"; // "D5_1722a"
static const char cDebugDumpRegistry[] = "debug.dumpregistry";
static const char cDebugClkDomainIntvl[] = "debug.clockdomain.debug.interval"; // s
static const char cDebugLogLevelPrefix[] = "debug.loglevel."; // prefix to be concatenated with context short name
static const char cDebugBufFName[] = "debug.buffill.fname";
static const char cDebugXmitShaperBwRate[] = "debug.transmit.shaper.bwrate."; // % of bandwidth to be limited (for debugging purposes only)
static const char cDebugNwIfTxRingSize[] = "debug.network.txring";
static const char cDebugAudioFlowLogEnable[] = "debug.audio.flow.log.enable";
static const char cXmitDelay[] = "transmit.timing.delay"; // ns
static const char cRxValidationMode[] = "receive.validation.mode";
static const char cRxValidationThreshold[] = "receive.validation.threshold";
static const char cRxIgnoreStreamId[] = "receive.ignore.streamid";
static const char cRxCycleWait[] = "receive.cyclewait"; // ns
static const char cRxIdleWait[] = "receive.idlewait"; // ns
static const char cRxSocketRxBufSize[] = "receive.socket.buffersize"; // bytes
static const char cRxDiscardAfter[] = "receive.discard.after"; // ns
static const char cRxDiscardOverrun[] = "receive.discard.overrun"; // 1=on, 0=off(default)
static const char cRxClkUpdateInterval[] = "receive.clock.updateinterval"; // us
static const char cRxExcessPayload[] = "receive.excess.payload"; // samples
static const char cRxRecoverIgbReceiver[] = "receive.recover.igb.receiver"; // 1=on (default), 0=off
static const char cXmitWndWidth[] = "transmit.window.width"; // ns
static const char cXmitWndPitch[] = "transmit.window.pitch"; // ns
static const char cXmitCueThresh[] = "transmit.window.threshold.cue"; // ns
static const char cXmitResetThresh[] = "transmit.window.threshold.reset"; // ns
static const char cXmitPrefetchThresh[] = "transmit.window.threshold.prefetch"; // ns (default 1000000000 = 1 sec, 0=off)
static const char cXmitResetMaxCount[] = "transmit.window.maxcount.reset"; // allowable max reset count per stream in a transmit window
static const char cXmitDropMaxCount[] = "transmit.window.maxcount.drop"; // allowable max number of dropped packages in a transmit window
static const char cXmitUseShaper[] = "transmit.shaper.enable"; // 0=disabled
static const char cXmitUseWatchdog[] = "transmit.watchdog.enable"; // 0=disabled
static const char cXmitStrictPktOrder[] = "transmit.pktorder.enable"; // 1=on (default), 0=off
static const char cXmitClkUpdateInterval[] = "transmit.clock.updateinterval"; // us
static const char cPtpPdelayCount[] = "ptp.pdelaycount"; //
static const char cPtpSyncCount[] = "ptp.synccount"; //
static const char cPtpLoopSleep[] = "ptp.loopsleep"; // ns
static const char cPtpLoopCount[] = "ptp.loopcount"; //
static const char cPtpXtstampThresh[] = "ptp.xtstamp.threshold"; // ns
static const char cPtpXtstampLoopCount[] = "ptp.xtstamp.loopcount"; //
static const char cVideoInNumPackets[] = "video.in.numpackets"; // video in ringbuffer size // TODO:???
static const char cVideoOutNumPackets[] = "video.out.numpackets"; // video out ringbuffer size // TODO:???
static const char cXmitVideoPoolsize[] = "transmit.video.poolsize"; // pool size for avb video transmit streams
static const char cXmitAafPoolsize[] = "transmit.aaf.poolsize"; // pool size for avb audio transmit streams
static const char cXmitCrfPoolsize[] = "transmit.crf.poolsize"; // pool size for avb clock reference transmit streams
static const char cAudioClockTimeout[] = "audio.clock.timeout"; // master time update timeout for AVB Audio TX in ns
static const char cAlsaClockTimeout[] = "alsa.clock.timeout"; // master time update timeout for ALSA in ns
static const char cAlsaClockCycle[] = "alsa.clock.cycle"; // adjust cycle of clock control loop in ns
static const char cAlsaClockGain[] = "alsa.clock.gain"; // base gain value of clock control loop in 1/1000
static const char cAlsaClockUnlock[] = "alsa.clock.unlock"; // unlock threshold for clock control loop in ns
static const char cAlsaClockResetThresh[] = "alsa.clock.threshold.reset"; // ns of oversleep to reinit control loop
static const char cAlsaDevicePrefill[] = "alsa.device.prefill."; // (UInt32) Number of Alsa periods the shm buffer of an Alsa capture device is prefilled. Has to be appended by device name.
static const char cAlsaDeviceBasePrefill[] = "alsa.device.baseprefill"; // default prefill level for all capture devices which can be overrode by cAlsaDevicePrefill
static const char cAlsaPrefillBufResetThresh[] = "alsa.prefill.threshold.bufreset."; // number of continuous buffer reset count to trigger the pre-filling on the running state
static const char cAlsaSmartXSwitch[] = "alsa.smartx.switch"; // SmartXbar Switch Matrix is used? 1=yes (default), 0=no
static const char cAlsaSyncRxReadStart[] = "alsa.sync.rx.read.start"; // skip time-expired samples when ALSA starts initial reading from rx streams
static const char cDiagnosticPacketDmac[] = "diagnosticpacket.dmac"; // (UInt64, default:0x011BC50AC000) sets the destination MAC address for the diagnostic packet specified in AutoCDS.
static const char cIgbAccessTimeoutCnt[] = "igb.access.to.cnt"; // Timeout:cIgbAccessSleep (in us: 100 ms) * cIgbAccessTimeoutCnt
static const char cApiMutex[] = "api.control.mutex"; // switch API mutex 1=enable (default), 0=off
}
//@}

class IasAvbStreamHandlerEnvironment : private virtual IasAvbConfigRegistryInterface,
    public virtual IasAvbRegistryQueryInterface
{
    friend class IasAvbStreamHandler;

  private:
    // for friend(s) only

    /**
     *  @brief Structure for long and short names of DLT contexts.
     */
    struct DltCtxNamesStruct
    {
        char shortName[5];
        char longName[64];
    };

    /**
     *  @brief Array of long and short names of DLT contexts.
     */
    static const DltCtxNamesStruct dltContextNames[];

    /**
     *  @brief Constructor.
     */
    explicit IasAvbStreamHandlerEnvironment(DltLogLevelType dltLogLevel);

    /**
     *  @brief Destructor
     */
    virtual ~IasAvbStreamHandlerEnvironment();

  public:

    // Operations

    static inline const std::string *getNetworkInterfaceName();
    static inline IasLibPtpDaemon *getPtpProxy();
    static inline IasLibMrpDaemon *getMrpProxy();
    static inline device_t *getIgbDevice();
    static inline IasAvbClockDriverInterface *getClockDriver();
    static inline const IasAvbMacAddress *getSourceMac();
    static inline IasDiaLogger* getDiaLogger();
    static inline const int32_t* getStatusSocket();
    static inline int32_t getLinkSpeed();
    static inline uint32_t getTxRingSize();
    static inline bool isLinkUp();
    static inline bool isTestProfileEnabled();

    template<class T>
    static inline bool getConfigValue(const std::string &key, T &value);
    static bool getConfigValue(const std::string &key, std::string &value);
    static bool doGetConfigValue(const std::string &key, uint64_t &value);

    static DltContext &getDltContext(const std::string &dltContextName);

    static void notifySchedulingIssue(DltContext &dltContext, const std::string &text, const uint64_t elapsed,
                                      const uint64_t limit);

    /**
     * @brief number of context entries in dltContextNames
     */
    static const size_t numDltContexts;

    //{@
    /// @brief IasAvbRegistryQueryInterface implementation
    virtual bool queryConfigValue(const std::string &key, uint64_t &value) const;
    virtual bool queryConfigValue(const std::string &key, std::string &value) const;
    //@}

    /**
     * @brief returns whether being under the control of systemd's watchdog
     */
    static inline bool isWatchdogEnabled();

    /**
     * @brief returns watchdog timeout value in milliseconds
     */
    static inline uint32_t getWatchdogTimeout();

    /**
     * @brief returns the instance of the WatchdogManager
     */
    // TO BE REPLACED static inline IasWatchdog::IasIWatchdogManagerInterface *getWatchdogManager();

#if defined(PERFORMANCE_MEASUREMENT)
    static inline bool isAudioFlowLogEnabled();
    static inline void setAudioFlowLoggingState(uint32_t state, uint64_t timestamp);
    static inline void getAudioFlowLoggingState(uint32_t &state, uint64_t &timestamp);
#endif

  private:

    typedef std::map<std::string, uint64_t> RegistryMapNumeric;
    typedef std::map<std::string, std::string> RegistryMapTextual;

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbStreamHandlerEnvironment(IasAvbStreamHandlerEnvironment const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbStreamHandlerEnvironment &operator=(IasAvbStreamHandlerEnvironment const &other);

    // operations only accessible to stream handler

    //{@
    /// @brief IasAvbConfigRegistryInterface implementation
    virtual IasAvbResult setConfigValue(const std::string &key, uint64_t value);
    virtual IasAvbResult setConfigValue(const std::string &key, const std::string &value);
    //@}

    void setDefaultConfigValues();
    bool validateRegistryEntries();
    IasAvbProcessingResult setTxRingSize();
    IasAvbProcessingResult createPtpProxy();
    IasAvbProcessingResult createMrpProxy();
    IasAvbProcessingResult createIgbDevice();
    IasAvbProcessingResult querySourceMac();
    bool queryLinkState();
    int32_t queryLinkSpeed();
    IasAvbProcessingResult loadClockDriver(const std::string &filePath);
    void emergencyShutdown();
    IasAvbProcessingResult registerDltContexts();
    IasAvbProcessingResult unregisterDltContexts();

    IasAvbProcessingResult openRawSocket();
    IasAvbProcessingResult createDiaLogger();

    IasAvbProcessingResult createWatchdog();
    void destroyWatchdog();

    //
    // Members
    //
    static IasAvbStreamHandlerEnvironment* mInstance;

    std::string mInterfaceName;

    IasLibPtpDaemon* mPtpProxy;
    IasLibMrpDaemon* mMrpProxy;
    device_t* mIgbDevice;
    IasAvbMacAddress mSourceMac;
    int32_t mStatusSocket;
    RegistryMapNumeric mRegistryNumeric;
    RegistryMapTextual mRegistryTextual;
    bool mRegistryLocked;
    bool mTestingProfileEnabled;
    IasAvbClockDriverInterface* mClockDriver;
    DltContext* mDltContexts; // array containing all DLT contexts used by other modules
    DltContext* mLog; // own DLT context
    DltLogLevelType mDltLogLevel; // selected DLT log level
    static DltContext* mDltCtxDummy; /* this is a dummy context to be handed over
     if requested one could not be found */
    void* mLibHandle;
    IasDiaLogger* mDiaLogger;

    uint32_t mTxRingSize;

    // Diagnostic counter helpers
    short mLastLinkState;
    // helper flag for testing emergencyShutdown
    bool mArmed;

    // watchdog related members
    bool mUseWatchdog;
    uint32_t mWdTimeout;
    // TO BE REPLACED Ias::IasCommonApiMainLoop* mWdMainLoop;
    // TO BE REPLACED IasThread* mWdThread;
    // TO BE REPLACED IasWatchdog::IasIWatchdogManagerInterface* mWdManager;
    // TO BE REPLACED std::shared_ptr<CommonAPI::MainLoopContext> mWdMainLoopContext;

#if defined(PERFORMANCE_MEASUREMENT)
    int32_t  mAudioFlowLogEnabled;
    uint32_t mAudioFlowLoggingState;
    uint64_t mAudioFlowLoggingTimestamp;
#endif
};

inline const std::string* IasAvbStreamHandlerEnvironment::getNetworkInterfaceName()
{
  std::string* ret = NULL;
  if (NULL != mInstance)
  {
    /* since the network ifname is used quite often, we cache it in a member variable
     * to avoid frequent map lookups.
     */
    if (mInstance->mInterfaceName.empty())
    {
      (void) getConfigValue(IasRegKeys::cNwIfName, mInstance->mInterfaceName);
    }
    ret = &mInstance->mInterfaceName;
  }
  return ret;
}

inline IasLibPtpDaemon* IasAvbStreamHandlerEnvironment::getPtpProxy()
{
  IasLibPtpDaemon* ret = NULL;
  if (NULL != mInstance)
  {
    ret = mInstance->mPtpProxy;
  }
  return ret;
}

inline IasLibMrpDaemon* IasAvbStreamHandlerEnvironment::getMrpProxy()
{
  IasLibMrpDaemon* ret = NULL;
  if (NULL != mInstance)
  {
    ret = mInstance->mMrpProxy;
  }
  return ret;
}

inline device_t* IasAvbStreamHandlerEnvironment::getIgbDevice()
{
  device_t* ret = NULL;
  if (NULL != mInstance)
  {
    ret = mInstance->mIgbDevice;
  }
  return ret;
}

inline IasAvbClockDriverInterface* IasAvbStreamHandlerEnvironment::getClockDriver()
{
  IasAvbClockDriverInterface* ret = NULL;
  if (NULL != mInstance)
  {
    ret = mInstance->mClockDriver;
  }
  return ret;
}

inline const IasAvbMacAddress* IasAvbStreamHandlerEnvironment::getSourceMac()
{
  const IasAvbMacAddress* ret = NULL;
  if (NULL != mInstance)
  {
    ret = &mInstance->mSourceMac;
  }
  return ret;
}

inline IasDiaLogger* IasAvbStreamHandlerEnvironment::getDiaLogger()
{
  IasDiaLogger* ret = NULL;
  if (NULL != mInstance)
  {
    ret = mInstance->mDiaLogger;
  }
  return ret;
}

inline const int32_t* IasAvbStreamHandlerEnvironment::getStatusSocket()
{
  int32_t * ret = NULL;
  if (NULL != mInstance)
  {
    ret = &mInstance->mStatusSocket;
  }
  return ret;
}

inline bool IasAvbStreamHandlerEnvironment::isLinkUp()
{
  AVB_ASSERT(NULL != mInstance);

  return mInstance->queryLinkState();
}

inline bool IasAvbStreamHandlerEnvironment::isTestProfileEnabled()
{
  bool ret = false;

  if (NULL != mInstance)
  {
    ret = mInstance->mTestingProfileEnabled;
  }

  return ret;
}

template<class T>
inline bool IasAvbStreamHandlerEnvironment::getConfigValue(const std::string &key, T &value)
{
  uint64_t val = 0u;
  bool ret = doGetConfigValue(key, val);
  if (ret)
  {
    value = static_cast<T>(val);
  }
  return ret;
}

inline int32_t IasAvbStreamHandlerEnvironment::getLinkSpeed()
{
  AVB_ASSERT(NULL != mInstance);

  return mInstance->queryLinkSpeed();
}

inline uint32_t IasAvbStreamHandlerEnvironment::getTxRingSize()
{
  AVB_ASSERT(NULL != mInstance);

  return mInstance->mTxRingSize;
}

inline bool IasAvbStreamHandlerEnvironment::isWatchdogEnabled()
{
  bool ret = false;

  if (NULL != mInstance)
  {
    ret = mInstance->mUseWatchdog;
  }

  return ret;
}

inline uint32_t IasAvbStreamHandlerEnvironment::getWatchdogTimeout()
{
  uint32_t ret = 0u;

  if (NULL != mInstance)
  {
    ret = mInstance->mWdTimeout;
  }

  return ret;
}

/* TO BE REPLACED
inline IasWatchdog::IasIWatchdogManagerInterface* IasAvbStreamHandlerEnvironment::getWatchdogManager()
{
  IasWatchdog::IasIWatchdogManagerInterface* ret = NULL;

  if (NULL != mInstance)
  {
    ret = mInstance->mWdManager;
  }

  return ret;
}
*/

#if defined(PERFORMANCE_MEASUREMENT)
inline bool IasAvbStreamHandlerEnvironment::isAudioFlowLogEnabled()
{
  bool ret = false;

  if (NULL != mInstance)
  {
    if (-1 == mInstance->mAudioFlowLogEnabled)
    {
      bool val = false;
      (void) getConfigValue(IasRegKeys::cDebugAudioFlowLogEnable, val);
      mInstance->mAudioFlowLogEnabled = static_cast<int32_t>(val);
    }
    ret = static_cast<bool>(mInstance->mAudioFlowLogEnabled);
  }

  return ret;
}

inline void IasAvbStreamHandlerEnvironment::setAudioFlowLoggingState(uint32_t state, uint64_t timestamp)
{
  if (NULL != mInstance)
  {
    mInstance->mAudioFlowLoggingState = state;
    mInstance->mAudioFlowLoggingTimestamp = timestamp;
  }
}

inline void IasAvbStreamHandlerEnvironment::getAudioFlowLoggingState(uint32_t &state, uint64_t &timestamp)
{
  if (NULL != mInstance)
  {
    state = mInstance->mAudioFlowLoggingState;
    timestamp = mInstance->mAudioFlowLoggingTimestamp;
  }
  else
  {
    state = 0u;
    timestamp = 0u;
  }
}
#endif // PERFORMANCE_MEASUREMENT

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBSTREAMHANDLERENVIRONMENT_HPP */
