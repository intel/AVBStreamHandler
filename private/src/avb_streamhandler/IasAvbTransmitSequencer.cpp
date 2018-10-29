/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbTransmitSequencer.cpp
 * @brief   The definition of the IasAvbTransmitSequencer class.
 * @date    2013
 */

#include <time.h> // make sure we include the right timespec definition
#include "avb_streamhandler/IasAvbTransmitSequencer.hpp"

#include "avb_streamhandler/IasDiaLogger.hpp"

#include "avb_streamhandler/IasAvbAudioStream.hpp"
#include "avb_streamhandler/IasAvbVideoStream.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEventInterface.hpp"
// TO BE REPLACED #include "core_libraries/btm/ias_dlt_btm.h"

#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <cctype>
#include <sstream>


using std::tolower;

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbTransmitSequencer::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

#define TQAVCTRL      0x03570
#define TQAVHC(_n)    (0x0300C + ((_n) * 0x40))
#define TQAVCC(_n)    (0x03004 + ((_n) * 0x40))

#define TQAVCH_ZERO_CREDIT 0x80000000 /* not configured and always defaults to this value */
#define TQAVCC_LINKRATE    0x7735     /* not configured and always defaults to this value */
#define TQAVCC_QUEUEMODE   0x80000000 /* queue mode, 0=strict, 1=SR mode */
#define TQAVCTRL_TX_ARB    0x00000100 /* data transmit arbitration */

/*
 *  Constructor.
 */
IasAvbTransmitSequencer::IasAvbTransmitSequencer(DltContext &ctx)
  : mThreadControl(0u)
  , mTransmitThread(NULL)
  , mIgbDevice(NULL)
  , mQueueIndex(uint32_t(-1))
  , mClass(IasAvbSrClass::eIasAvbSrClassHigh)
  , mRequestCount(0)
  , mResponseCount(0)
  , mCurrentBandwidth(0u)
  , mCurrentMaxIntervalFrames(0u)
  , mMaxFrameSizeHigh(0u)
  , mUseShaper(false)
  , mShaperBwRate(100u)
  , mSequence()
  , mActiveStreams()
  , mDoReclaim(false)
  , mLock()
  , mEventInterface(NULL)
  , mLog(&ctx)
  , mWatchdog(NULL)
  , mFirstRun(true)
  , mBTMEnable(false)
  , mStrictPktOrderEn(true)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

IasAvbTransmitSequencer::Config::Config()
  : txWindowWidthInit(24u * 125000u)
  , txWindowWidth(txWindowWidthInit)
  , txWindowPitchInit(16u * 125000u)
  , txWindowPitch(txWindowPitchInit)
  , txWindowCueThreshold(8u * 125000u)
  , txWindowResetThreshold(32u * 125000u)
  , txWindowPrefetchThreshold(1000000000u)
  , txWindowMaxResetCount(1u)
  , txWindowMaxDropCount(8000u)
  , txDelay(100000u)
  , txMaxBandwidth(70000u)
{
  // do nothing
}


IasAvbTransmitSequencer::Diag::Diag()
  : sent(0u)
  , dropped(0u)
  , reordered(0u)
  , debugOutputCount(0u)
  , debugErrCount(0u)
  , debugSkipCount(0u)
  , debugTimingViolation(0u)
  , avgPacketSent(0.0f)
  , avgPacketReclaim(0.0f)
  , debugLastLaunchTime(0u)
  , debugLastStream(NULL)
{
  // do nothing
}


/*
 *  Destructor.
 */
IasAvbTransmitSequencer::~IasAvbTransmitSequencer()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasAvbTransmitSequencer::init(uint32_t queueIndex, IasAvbSrClass qavClass, bool doReclaim)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  const char * const suffix = IasAvbTSpec::getClassSuffix(qavClass);

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBootTimeMeasurement, mBTMEnable);

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }

  if ((queueIndex > 1u) || (static_cast<uint32_t>(qavClass) >= IasAvbTSpec::cIasAvbNumSupportedClasses))
  {
    result = eIasAvbProcInvalidParam;
  }

  if (eIasAvbProcOK == result)
  {
    mTransmitThread = new (nothrow) IasThread(this, std::string("AvbTxWrk") + suffix);
    if (NULL == mTransmitThread)
    {
      /**
       * @log Not enough memory to create the thread.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't create transmit thread!");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  if (eIasAvbProcOK == result)
  {
    mClass = qavClass;
    mQueueIndex = queueIndex;
    mDoReclaim = doReclaim;
    mIgbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
    AVB_ASSERT(NULL != mIgbDevice);

    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitWndWidth, mConfig.txWindowWidthInit);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitWndPitch, mConfig.txWindowPitchInit);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitCueThresh, mConfig.txWindowCueThreshold);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitResetThresh, mConfig.txWindowResetThreshold);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitResetMaxCount, mConfig.txWindowMaxResetCount);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitDropMaxCount, mConfig.txWindowMaxDropCount);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitPrefetchThresh, mConfig.txWindowPrefetchThreshold);
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitDelay, mConfig.txDelay );
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(std::string(IasRegKeys::cTxMaxBw) + suffix, mConfig.txMaxBandwidth );

    if ((mConfig.txWindowWidthInit < mConfig.txWindowPitchInit)
        || (mConfig.txWindowWidthInit < cMinTxWindowWidth)
        || (mConfig.txWindowPitchInit < cMinTxWindowPitch)
        )
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "bad TX window values: width =",
          mConfig.txWindowWidthInit,
          "pitch =", mConfig.txWindowPitchInit);
      result = eIasAvbProcInitializationFailed;
    }

    uint64_t val = 0u;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitUseShaper, val);

    mUseShaper = (0u != val);

    std::string optName = std::string(IasRegKeys::cDebugXmitShaperBwRate) + IasAvbTSpec::getClassSuffix(mClass);
    val = mShaperBwRate;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(optName, val);
    if (mShaperBwRate != val)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
          optName.c_str(), "option is for debugging purposes only!!!");

      if (100u < val)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
            optName.c_str(), "option should have value [0 - 100]");
      }
      else
      {
        mShaperBwRate = static_cast<uint32_t>(val);
      }
    }
  }

  if (eIasAvbProcOK == result)
  {
    uint64_t val = 0u;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cUseWatchdog, val);

    if ((0u != val) && (IasAvbStreamHandlerEnvironment::isWatchdogEnabled()))
    {
      result = eIasAvbProcInitializationFailed;

      //Get the watchdog manager pointer and create a watchdog entry but not register it.
      IasWatchdog::IasSystemdWatchdogManager* wdManager = NULL;
      wdManager = IasAvbStreamHandlerEnvironment::getWatchdogManager();

      if (NULL != wdManager)
      {
        uint32_t timeout = IasAvbStreamHandlerEnvironment::getWatchdogTimeout();
        std::string wdName = std::string("AvbTxWd") + IasAvbTSpec::getClassSuffix(mClass);

        mWatchdog = wdManager->createWatchdog(timeout, wdName);
        if (NULL != mWatchdog)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " supervised watchdog name:",
              wdName.c_str(), "timeout:", timeout, "ms");
          result = eIasAvbProcOK;
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "AvbStreamHandlerEnvironment could not getWatchdogManager");
      }
    }
  }

  // the flag ensures xmit packets being sorted in ascending launchtime order but cpu load may increase
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitStrictPktOrder, mStrictPktOrderEn);

  if (eIasAvbProcOK != result)
  {
    cleanup();
  }

  return result;
}

void IasAvbTransmitSequencer::cleanup()
{
  if (mTransmitThread != NULL && mTransmitThread->isRunning())
  {
    mTransmitThread->stop();
  }
  delete mTransmitThread;
  mTransmitThread = NULL;

  if (NULL != mWatchdog)
  {
    IasWatchdog::IasSystemdWatchdogManager* wdManager = NULL;
    wdManager = IasAvbStreamHandlerEnvironment::getWatchdogManager();
    if (NULL != wdManager)
    {
      (void) wdManager->destroyWatchdog(mWatchdog);
      mWatchdog = NULL;
    }
  }
}

IasAvbProcessingResult IasAvbTransmitSequencer::registerEventInterface( IasAvbStreamHandlerEventInterface * eventInterface )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (NULL == mTransmitThread) // Is transmit engine initialized?
  {
    result = eIasAvbProcNotInitialized; // Bad state
  }
  else if (NULL == eventInterface)
  {
    result = eIasAvbProcInvalidParam; // Invalid param
  }
  else if (NULL != mEventInterface) // Already registered ?
  {
    result = eIasAvbProcAlreadyInUse;
  }
  else
  {
    mEventInterface = eventInterface;
  }

  return result;
}

IasAvbProcessingResult IasAvbTransmitSequencer::unregisterEventInterface( IasAvbStreamHandlerEventInterface * eventInterface )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (NULL == mTransmitThread) // Transmit engine is not initialized
  {
    result = eIasAvbProcNotInitialized; // Bad state
  }
  else if ((NULL == eventInterface) || (mEventInterface != eventInterface))
  {
    result = eIasAvbProcInvalidParam; // Invalid param
  }
  else
  {
    mEventInterface = NULL;
  }

  return result;
}

IasAvbProcessingResult IasAvbTransmitSequencer::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  if (isInitialized())
  {
    IasThreadResult res = mTransmitThread->start(true);
    if ((res != IasResult::cOk) && (res != IasThreadResult::cThreadAlreadyStarted))
    {
      result = eIasAvbProcThreadStartFailed;
    }

    mRequestCount++;
    sync();

  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "not initialized!");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitSequencer::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  if (isInitialized())
  {
    if (mTransmitThread->isRunning())
    {
      if (mTransmitThread->stop() != IasResult::cOk)
      {
        result = eIasAvbProcThreadStopFailed;
      }

      /* signal interruption of transmission to streams, but set them back to active
       * right afterwards so they will be restarted when the engine is started again
       */
      for (AvbStreamSet::iterator it = mActiveStreams.begin(); it != mActiveStreams.end(); it++)
      {
        IasAvbStream *stream = *it;
        AVB_ASSERT(NULL != stream);
        stream->deactivate();
        stream->activate();
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "not initialized!");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


uint32_t IasAvbTransmitSequencer::reclaimPackets()
{
  uint32_t ret = 0u;
  igb_packet* packetList = NULL;

  if (mDoReclaim)
  {
    // check and return packets that are not used any longer
    // NOTE: this is done for all sequencers, not only for this one!
    igb_clean(mIgbDevice, &packetList);
    while (NULL != packetList)
    {
      IasAvbPacketPool::returnPacket(packetList);
      ret++;
      packetList = packetList->next;
    }
  }

  return ret;
}


IasResult IasAvbTransmitSequencer::beforeRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);
  mThreadControl = 0u;
  return IasResult::cOk;
}


IasResult IasAvbTransmitSequencer::run()
{
  IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
  AVB_ASSERT(NULL != ptp);
  uint64_t windowStart = 0u;
  size_t streamsToService = 0u;
  bool linkState = false;
  uint32_t linkStateWaitCount = 0u;
  uint64_t lastOversleep = 0u;
  uint32_t oversleepCount = 0u;

  /*
   * nextStreamToService either points to:
   * 1) nothing (i.e. mSequence.end()) when the list is empty
   * 2) a stream which does not currently have a packet to be sent (stream out of buffers, aka dry stream)
   * 3) the packet with the closest launch time
   */
  AvbStreamDataList::iterator nextStreamToService = mSequence.end();

  mConfig.txWindowWidth = mConfig.txWindowWidthInit;
  mConfig.txWindowPitch = mConfig.txWindowPitchInit;

  struct sched_param sparam;
  std::string policyStr = "fifo";
  int32_t priority = 1;

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPolicy, policyStr);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPriority, priority);

  int32_t policy = (policyStr == "other") ? SCHED_OTHER : (policyStr == "rr") ? SCHED_RR : SCHED_FIFO;
  sparam.sched_priority = priority;

  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Parameter of TransimitEngineThread: policy", policy,
      "/ prio", int32_t(sparam.sched_priority));

  int32_t errval = pthread_setschedparam(pthread_self(), policy, &sparam);
  if(0 == errval)
  {
    errval = pthread_getschedparam(pthread_self(), &policy, &sparam);
    if(0 == errval)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "TransmitEngineThread scheduling parameters set to: policy", policy,
          "/ prio", int32_t(sparam.sched_priority));
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error getting scheduler parameter:", strerror(errval));
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error setting scheduler parameter:", strerror(errval));
  }

  mDiag.debugLastLaunchTime = 0u;
  mDiag.debugLastResetMsgOutputTime = 0u;
  windowStart = ptp->getLocalTime();
  uint32_t lastEpoch = ptp->getEpochCounter();
  while (0u == (mThreadControl & cFlagEndThread))
  {
    if (0u != mThreadControl)
    {
      mLock.lock();
      // acknowledge restart
      mThreadControl &= ~cFlagRestartThread;
      // sleep for 500ms - wait until ptp daemon has recovered
      nssleep(500000000u);
      // reset all active streams
      for (AvbStreamSet::iterator it = mActiveStreams.begin(); it != mActiveStreams.end(); it++)
      {
        IasAvbStream *stream = *it;
        AVB_ASSERT(NULL != stream);
        stream->deactivate();
        stream->activate();
      }
      // tick back response counter to retrigger sequence update
      mResponseCount--;
      mLock.unlock();

      windowStart = ptp->getLocalTime();
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "TX worker thread restarted\n");
      mDiag.debugLastLaunchTime = 0u;
      mDiag.debugLastResetMsgOutputTime = 0u;
    }

    checkLinkStatus(linkState);
    uint64_t previousSleepTimestamp = ptp->getTsc();
    while (!mThreadControl)
    {
      bool oldLinkState = linkState;
      checkLinkStatus(linkState);
      updateSequence(nextStreamToService);

      if (!linkState)
      {
        // disable the watchdog while link is down
        if ((NULL != mWatchdog) && (mWatchdog->isRegistered()))
          (void) mWatchdog->unregisterWatchdog();
        const uint32_t cPollLinkPerSecond = 1u << 5; // 32
        if (0u == (linkStateWaitCount & (cPollLinkPerSecond - 1u)))
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "waiting for network link... (",
              (linkStateWaitCount / cPollLinkPerSecond), "s");
        }
        linkStateWaitCount++;
        (void) reclaimPackets();
        ::usleep(1000000u / cPollLinkPerSecond);
        continue;
      }
      else
      {
        linkStateWaitCount = 0u;
        if (oldLinkState != linkState)
        {
          // just make sure the watchdog is inactive before entering the long-sleep
          if ((NULL != mWatchdog) && (mWatchdog->isRegistered()))
            (void) mWatchdog->unregisterWatchdog();

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "link state has changed from",
              oldLinkState, "to", linkState);
          ::sleep(3);
          mThreadControl |= cFlagRestartThread;
        }
      }

      if (mThreadControl)
      {
        continue;
      }
      streamsToService = mSequence.size();

      if (0u != streamsToService)
      {
        // there is at least one active stream, enable the watchdog
        if ((NULL != mWatchdog) && (false == mWatchdog->isRegistered()))
        {
          if (IasResult::cOk != mWatchdog->registerWatchdog())
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " watchdog registration failure...");
            mThreadControl |= cFlagEndThread;
          }
          else
          {
            // reset() is required to start the timer
            (void) mWatchdog->reset();
          }
        }
      }
      else
      {
        // there is nothing to send, disable the watchdog
        if ((NULL != mWatchdog) && (mWatchdog->isRegistered()))
          (void) mWatchdog->unregisterWatchdog();
      }

      for (AvbStreamDataList::iterator it = mSequence.begin(); it != mSequence.end(); it++)
      {
        it->done = eNotDone;
      }

      /* iterate through our sequence while dynamically resorting it until all streams have delivered
       * all packets belonging to the current TX window
       *
       * Note: By design, this could lead to the same stream being serviced multiple times in a row!
       */
      while (!mThreadControl && (streamsToService > 0u))
      {
        DoneState done = serviceStream(windowStart, nextStreamToService);
        switch (done)
        {
        case eNotDone:
          // do nothing
          break;

        case eEndOfWindow:
        case eDry:
          streamsToService--;
          break;

        case eWindowAdjust:
        case eTxError:
        default:
          // abort cycle and sleep
          streamsToService = 0u;
        }
      }

      if (mStrictPktOrderEn)
      {
        // re-sort streams based on launch time
        (void) mSequence.sort();
        nextStreamToService = mSequence.begin();
      }

      // advance TX window and sleep until the new window is reached
      windowStart += mConfig.txWindowPitch;
      const uint64_t sleepUntil = ptp->ptpToSys(windowStart);

      // Before triggering the sleep, ensure windowStart is still aligned with PTP Clock
      uint32_t epoch = ptp->getEpochCounter();
      if (epoch != lastEpoch)
      {
          lastEpoch = epoch;
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ptp time warp detected - restarting TX worker thread");
          mThreadControl |= cFlagRestartThread;
          continue;
      }
      timespec tp;
      IasLibPtpDaemon::convertNsToTimespec(sleepUntil, tp);
      int rc = clock_nanosleep(IasLibPtpDaemon::cSysClockId, TIMER_ABSTIME, &tp, NULL);
      if (rc < 0)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "clock_nanosleep failure: ", strerror(rc));
        mThreadControl |= cFlagEndThread;
      }

      const uint64_t timestampNow =  ptp->getTsc();
      int64_t over = timestampNow - sleepUntil;
      if (over > int64_t(mConfig.txWindowWidth - mConfig.txWindowPitch))
      {
        oversleepCount++;
        // do not print out this warning more often than once a second
        if ((sleepUntil - lastOversleep) > 1000000000u)
        {
          lastOversleep = sleepUntil;
          IasAvbStreamHandlerEnvironment::notifySchedulingIssue(*mLog,  "TX worker thread slept too long!", over, mConfig.txWindowWidth - mConfig.txWindowPitch);
          if (oversleepCount > 1u)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, oversleepCount, "more oversleep events occurred since the last message");
          }
          oversleepCount = 0u;
        }
      }

      // try to re-claim buffers from igb
      const float reclaimed = float(reclaimPackets());

      logOutput(float(timestampNow - previousSleepTimestamp) * 1e-9f, reclaimed);
      previousSleepTimestamp = timestampNow;
    }

    /**
     * upon TX engine stop:
     * return all remaining packets to their streams
     */

    // wait some time for the buffers to return from igb
    nssleep(static_cast<uint32_t>(3u * mConfig.txWindowWidth));
    (void) reclaimPackets();

    // return the packets still held by the sequence
    for (AvbStreamDataList::iterator it = mSequence.begin(); it != mSequence.end(); it++)
    {
      if (NULL != it->packet)
      {
        IasAvbPacketPool::returnPacket(it->packet);
        it->packet = NULL;
      }
    }

    nextStreamToService = mSequence.end();
    mSequence.clear();

  }

  // unregister the watchdog before exiting the thread
  if ((NULL != mWatchdog) && mWatchdog->isRegistered())
  {
    /*
     * Note: registerWatchdog() and unregisterWatchdog() must be called by the thread
     * to be monitored. So we cannot call them from the start() nor the stop() method
     * since they are called by the main thread. Also the destructor cannot call
     * unregisterWatchdog() due to the same reason. That's why we need to unregister
     * the watchdog here before exiting the thread.
     */

    (void) mWatchdog->unregisterWatchdog();
  }

  return IasResult::cOk;
}


void IasAvbTransmitSequencer::checkLinkStatus(bool &linkState)
{
  // Check link state
  bool linkIsUp = IasAvbStreamHandlerEnvironment::isLinkUp();

  if (linkIsUp && !linkState)
  {
    // we also have to wait for the PTP daemon to become ready
    IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
    AVB_ASSERT(NULL != ptp);
    linkIsUp = ptp->isPtpReady();
  }

  // If link state has changed send a notification to registered device
  if (linkState != linkIsUp)
  {
    linkState = linkIsUp;
    if (NULL != mEventInterface)
    {
      mEventInterface->updateLinkStatus(linkState);
    }
  }
}

void IasAvbTransmitSequencer::updateSequence(AvbStreamDataList::iterator & nextStreamToService)
{
  // check for changes in the active streams set, indicated by an increased request counter

  const uint32_t numStreamsOld = static_cast<uint32_t>(mSequence.size());
  (void) numStreamsOld;
  bool change = false;

  sync();
  while ((mRequestCount - mResponseCount) > 0)
  {
    change = true;

    /*
     * sync active stream set with sequence list by first erasing obsolete objects from sequence list
     * and then adding the ones remaining in the (temporary copy of the) active list
     */

    mLock.lock();

    AvbStreamSet temp = mActiveStreams;
    for (AvbStreamDataList::iterator s = mSequence.begin(); s != mSequence.end(); /*in loop*/)
    {
      if (temp.find(s->stream) == temp.end())
      {
        // stream not found in active set anymore -> erase from sequence
        if (nextStreamToService == s)
        {
          nextStreamToService++;
          if (mSequence.end() == nextStreamToService)
          {
            nextStreamToService = mSequence.begin();
          }
        }
        if (NULL != s->packet)
        {
          IasAvbPacketPool::returnPacket(s->packet);
        }
        s = mSequence.erase(s);
        if (mSequence.size() == 0u)
        {
          nextStreamToService = mSequence.end();
        }
      }
      else
      {
        // take away from temp set so only the new ones remain
        temp.erase(s->stream);
        s++;
      }
    }

    // insert new streams into sequence
    for (AvbStreamSet::iterator it = temp.begin(); it != temp.end(); it++)
    {
      IasAvbStream * stream = *it;
      AVB_ASSERT( NULL != stream );
      StreamData newData;
      newData.done = eNotDone;
      newData.stream = stream;
      newData.packet = NULL;
      newData.launchTime = 0u;
      mSequence.push_front( newData );

      // this happens only when mSequence is empty
      if (mSequence.end() == nextStreamToService)
      {
        nextStreamToService = mSequence.begin();
      }

      sortByLaunchTime(nextStreamToService);
    }

    mLock.unlock();

    /*
     * respond to client after sequence list is updated. In case of destroy stream request
     * client might destroy stream once sequencer responded to the request.
     */
    mResponseCount++;
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "request/response:", mRequestCount, "/", mResponseCount);

    sync();
  }

  if (change)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "sync done, sequence size =", uint32_t(mSequence.size()));

    if (numStreamsOld > mSequence.size())
    {
      // less active streams, try if we can use the original TX timing
      mConfig.txWindowWidth = mConfig.txWindowWidthInit;
      mConfig.txWindowPitch = mConfig.txWindowPitchInit;
    }
  }
}

IasAvbTransmitSequencer::DoneState IasAvbTransmitSequencer::serviceStream(uint64_t windowStart, AvbStreamDataList::iterator & nextStreamToService)
{
  int32_t result;
  StreamData & current = *nextStreamToService;
  bool fetch = true;
  uint64_t streamId = 0;

  IasDiaLogger *diaLogger = IasAvbStreamHandlerEnvironment::getDiaLogger();

  if (NULL != current.stream)
  {
    streamId = uint64_t(current.stream->getStreamId());

    if (current.done != eNotDone)
    {
      std::stringstream ssStreamId;
      ssStreamId << "0x" << std::hex << streamId;
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "internal error: stream", ssStreamId.str(),
          "already done, cycle", mDiag.debugOutputCount);
    }

    // does the stream have a valid packet to send?
    if (NULL != current.packet)
    {
      AVB_ASSERT( 0u != current.launchTime );

      if (current.packet->attime > (windowStart + mConfig.txWindowWidth))
      {
        // stream does not need to be serviced within the current window
        current.done = eEndOfWindow;
        nextStreamToService = next(nextStreamToService);
        fetch = false;
      }
      else
      {
        // a dummy packet blocks service of this stream until the dummy launch time is reached
        if (current.packet->isDummyPacket())
        {
          {
            // need to dispose of the dummy packet and restart fetching for the stream
            IasAvbPacketPool::returnPacket(current.packet);
            current.packet = NULL;

            /*
             * Reset the watchdog timer as long as we can see a dummy packet.
             * Video stream prepares dummy packets when application does not send actual video data.
             * Audio stream prepares dummy packets when clock reference is not available typically
             * on the slave side. For the both cases we consider the Streamhandler is still alive.
             */
            if ((NULL != mWatchdog) && (mWatchdog->isRegistered()))
            {
              (void) mWatchdog->reset();
            }
          }
        }
        else
        {
          // send the packet
          current.packet->attime = current.launchTime + mConfig.txDelay;
          if (current.packet->attime < mDiag.debugLastLaunchTime)
          {
            IasAvbStream *prev = mDiag.debugLastStream;
            uint64_t prevId = prev ? prev->getStreamId() : uint64_t(-1);
            uint64_t currId = current.stream ? current.stream->getStreamId() : uint64_t(-1);
            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "TX timing violation:",
                prevId, mDiag.debugLastLaunchTime, currId, current.packet->attime);
            mDiag.debugTimingViolation++;
          }
          mDiag.debugLastLaunchTime = current.packet->attime;
          mDiag.debugLastStream = current.stream;

#if defined(PERFORMANCE_MEASUREMENT)
          if (IasAvbStreamHandlerEnvironment::isAudioFlowLogEnabled()) // latency analysis
          {
            uint32_t state = 0u;
            uint64_t logtime = 0u;
            (void) IasAvbStreamHandlerEnvironment::getAudioFlowLoggingState(state, logtime);

            uint8_t *pktBuf = (uint8_t*)(current.packet->getBasePtr());

            if ((3u == state) && (0x2 == pktBuf[18]))
            {
              uint16_t streamDataLen = ntohs(*(uint16_t*)&pktBuf[38]);
              const uint32_t cBufSize = sizeof(uint16_t) * 64u;
              static uint8_t zeroBuf[cBufSize];
              if (0 != zeroBuf[0])
              {
                (void) std::memset(zeroBuf, 0, cBufSize);
              }

              if (streamDataLen > cBufSize)
              {
                streamDataLen = cBufSize;
              }

              if ((0 != pktBuf[42]) ||
                  (0 != std::memcmp(&pktBuf[42], zeroBuf, streamDataLen)))
              {
                IasLibPtpDaemon * ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
                if (NULL != ptp)
                {
                  uint64_t tscNow = ptp->getTsc();
                  DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                              "latency-analysis(4): passed samples to MAC system time =", tscNow,
                              "delta =", (double)(tscNow - logtime) / 1e6, "ms");
                  IasAvbStreamHandlerEnvironment::setAudioFlowLoggingState(0u, logtime);
                }
              }
            }
          }
#endif

          result = current.packet->xmit(mIgbDevice, mQueueIndex);
          if (mFirstRun && mBTMEnable)
          {
            mFirstRun = false;
            // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb sending now",NULL);
          }
          uint32_t counterTx = 0u;
          switch (result)
          {
          case -EINVAL:
          case -ENXIO:
            // fatal errors, dispose of packet
            IasAvbPacketPool::returnPacket(current.packet);
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "igb_xmit error:", int32_t(result));
            fetch = false;
            current.done = eTxError;
            break;

          case 0:
            // success
            fetch = true;

            counterTx = current.stream->incFramesTx();
            (void) counterTx;
            mDiag.sent++;

            // Where to reset the counter
            if (NULL != diaLogger)
            {
              diaLogger->incTxCount();
            }

            // reset the watchdog timer when packet was successfully sent
            if ((NULL != mWatchdog) && mWatchdog->isRegistered())
            {
              (void) mWatchdog->reset();
            }

            break;

          case ENOSPC:
            {
              // Insufficient ring size: Calculate the required ring size for diagnostic purposes
              uint32_t frames = 0u;
              for (AvbStreamDataList::iterator it = mSequence.begin(); it != mSequence.end(); it++)
              {
                AVB_ASSERT(NULL != it->stream);
                frames += it->stream->getTSpec().getMaxIntervalFrames();
              }
              AVB_ASSERT(0u != mConfig.txWindowWidth);

              const uint64_t reqRingSize = uint64_t(IasAvbTSpec::getPacketsPerSecondByClass(mClass)) * uint64_t(frames)
                  * mConfig.txWindowWidth / uint64_t(1e9)
                  * 2u // two entries per packet
                  * 130u / 100u; // 30% margin

              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "TX ring buffer overflow detected! Try changing the ring buffer size.",
                  "TX window:", mConfig.txWindowWidth,
                  "active streams:", uint64_t(mSequence.size()),
                  "frames/interval:", frames,
                  "min ring size:", reqRingSize);

              if ((mUseShaper) && (100u != mShaperBwRate))
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Ignoring the TX ring buffer overflow error since the shaper is under debugging.");
              }
              else
              {
                mThreadControl |= cFlagRestartThread;
              }
            }
            break;

          default:
            // unknown or non-fatal errors, try again
            fetch = false;
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "igb_xmit returns\n", int32_t(result));
            break;
          }
        }
      }
    } // if packet to send

    uint32_t droppedOld = mDiag.dropped;
    uint64_t resetCnt = 0u;
    uint64_t maxResetCnt = mConfig.txWindowMaxResetCount;

    while (fetch)
    {
      // fetch new packet and re-sort the stream in the sequence, depending on the new packet's launch time
      fetch = false;
      bool isDry = (NULL == current.packet);
      current.packet = current.stream->preparePacket(windowStart + mConfig.txWindowPitch);

      if (NULL != current.packet)
      {
        const int64_t timeFromWindowStart = int64_t(current.packet->attime - windowStart);
        current.launchTime = current.packet->attime;

        if (timeFromWindowStart > int64_t(mConfig.txWindowWidth))
        {
          if ((0u != mConfig.txWindowPrefetchThreshold) &&
              (timeFromWindowStart > int64_t(mConfig.txWindowPrefetchThreshold)))
          {
            std::stringstream ssStreamId;
            ssStreamId << "0x" << std::hex << streamId;

            DltLogLevelType loglevel = DLT_LOG_VERBOSE;
            if ((mDiag.debugLastResetMsgOutputTime > windowStart) ||  // unlikely to happen but for fail-safe
                (windowStart - mDiag.debugLastResetMsgOutputTime > uint64_t(1e9)))  // don't output warn message more than once/sec
            {
              loglevel = DLT_LOG_WARN;
              mDiag.debugLastResetMsgOutputTime = windowStart;
            }

            // stream is way beyond, reset the stream to recalculate launch time
            DLT_LOG_CXX(*mLog, loglevel, LOG_PREFIX, "stream", ssStreamId.str(),
                "reset due to launch time lag (way beyond):", timeFromWindowStart,
                "windowStart =", windowStart, "launch =", current.packet->attime,
                "threshold =", mConfig.txWindowPrefetchThreshold);

            IasAvbPacketPool::returnPacket(current.packet);
            current.packet = NULL;

            // toggle activation of stream to force reinit of transmission time
            current.stream->deactivate();
            current.stream->activate();

            if (++resetCnt <= maxResetCnt)
            {
              fetch = true;
            }
            else
            {
              current.done = eDry; // to avoid an infinite loop
            }
          }
          else
          {
            // we're out of the tx window, done with this stream for now
            current.done = eEndOfWindow;
            nextStreamToService = next(nextStreamToService);
          }
        }
        else if (timeFromWindowStart < -int64_t(mConfig.txWindowResetThreshold))
        {
          std::stringstream ssStreamId;
          ssStreamId << "0x" << std::hex << streamId;
          /**
           * @log Stream is way behind, needs to be reset
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "stream", ssStreamId.str(),
              "reset due to launch time lag:", timeFromWindowStart, "windowStart = ", windowStart,
              "launch = ", current.packet->attime);

          IasAvbPacketPool::returnPacket(current.packet);
          current.packet = NULL;

          // toggle activation of stream to force reinit of transmission time
          current.stream->deactivate();
          current.stream->activate();

          if (++resetCnt <= maxResetCnt)
          {
            fetch = true;
          }
          else
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "too many stream resets (", maxResetCnt, ")");
            current.done = eDry; // to avoid an infinite loop
          }
        }
        else if (timeFromWindowStart < -int64_t(mConfig.txWindowCueThreshold))
        {
          // dispose of packet and fetch another one until tolerable launch time is reached
          uint8_t* const avtpBase8 = static_cast<uint8_t*>(current.packet->getBasePtr()) + ETH_HLEN + 4u;
          uint32_t* const avtpBase32 = reinterpret_cast<uint32_t*>(avtpBase8);

          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, "Dispose packet ",
            " Stream:", ntohl(avtpBase32[1]), " ", ntohl(avtpBase32[2]),
            " SeqNum:", int32_t(avtpBase8[2]),
            " TS:", ntohl(avtpBase32[3]),
            " due to untolerable launch time ", int64_t(timeFromWindowStart), " ", int64_t(mConfig.txWindowCueThreshold));

          IasAvbPacketPool::returnPacket(current.packet);
          current.packet = NULL;
          mDiag.dropped++;
          if ((mDiag.dropped - droppedOld) >= mConfig.txWindowMaxDropCount)
          {
            std::stringstream ssStreamId;
            ssStreamId << "0x" << std::hex << streamId;
            // this is just a sanity check in case the stream is unable to advance in time for whatever reason.
            // Shouldn't really happen. If it still does, reset the stream.
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "too many packets dropped (", mDiag.dropped,
                ", timeFromWindowStart =", timeFromWindowStart, "stream", ssStreamId.str());

            // @@DIAG error handling, not normal start/stop
            const bool error = true;
            current.stream->deactivate(error);
            current.stream->activate(error);
            current.done = eDry; // to avoid an infinite loop
          }
          else
          {
            fetch = true;
          }
        }
        else
        {
          // yay, we're inside the window!
          /* Note that the sort method also skips to the next stream already, but
           * that skipping does not consider the 'done' state of that stream.
           */
          sortByLaunchTime(nextStreamToService);
        }
      }
      else
      {
        if (!isDry)
        {
          std::stringstream ssStreamId;
          ssStreamId << "0x" << std::hex << streamId;
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "stream", ssStreamId.str() , "ran dry");
        }

        current.launchTime = 0u;
        current.done = eDry;
        nextStreamToService = next(nextStreamToService);
      }
    } // while fetch
  }

  // if the next stream is done already, try to find one that is not done yet
  AvbStreamDataList::iterator startPoint = nextStreamToService;
  while (nextStreamToService->done != eNotDone)
  {
    nextStreamToService = next(nextStreamToService);
    mDiag.debugSkipCount++;
    if (startPoint == nextStreamToService)
    {
      break;
    }
  }

  return current.done;
}

void IasAvbTransmitSequencer::logOutput(float elapsed, float reclaimed)
{
  // cheesy IIR "moving average" statistics
  mDiag.avgPacketSent = mDiag.avgPacketSent * 0.99f + 0.01f * float(mDiag.sent) / elapsed;
  mDiag.avgPacketReclaim = mDiag.avgPacketReclaim * 0.99f + 0.01f * reclaimed / elapsed;

  if (mDiag.debugOutputCount++ == 400u)
  {
    mDiag.debugOutputCount = 0u;

    if ((0u == mDiag.sent) && (mDiag.avgPacketSent < 0.1f))
    {
      mDiag.avgPacketSent = 0.0f;
    }

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Statistics: reorder:", mDiag.reordered,
        "dropped:", mDiag.dropped,
        "avg.sent:", mDiag.avgPacketSent);
    mDiag.reordered = 0u;
    mDiag.dropped = 0u;

    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "skipped:", mDiag.debugSkipCount,
        "violations:", mDiag.debugTimingViolation,
        "avg.reclaim:", mDiag.avgPacketReclaim,
        "sent/reclaim:", mDiag.avgPacketSent / mDiag.avgPacketReclaim
        );
    mDiag.debugSkipCount = 0u;
    mDiag.debugTimingViolation = 0u;
  }

  mDiag.sent = 0u;
}

void IasAvbTransmitSequencer::sortByLaunchTime(AvbStreamDataList::iterator & nextStreamToService)
{
  StreamData & current = *nextStreamToService;
  AvbStreamDataList::iterator backward = nextStreamToService;

  /* go backwards from the current stream to find packets that need to be sent earlier
   * NOTE: in most cases, this loop will abort immediately and no reordering takes place
   */
  do
  {
    backward = prev(backward);

    if ((backward->launchTime != 0u) && (current.launchTime > backward->launchTime))
    {
      break;
    }
  }
  while (nextStreamToService != backward);

  /* if nextStreamToService == backward the stream gets reordered to the position it already has and needs
   * to be serviced again immediately, so no action needs to be taken
   */
  if (nextStreamToService != backward)
  {
    // insert() inserts before the specified position, so we need to advance by one to insert after
    backward = next(backward);

    if (nextStreamToService != backward)
    {
      // move current stream to new position and skip nextStreamToService to the next one
      mSequence.insert(backward, current);
      nextStreamToService = mSequence.erase(nextStreamToService);
      // wrap around, if necessary
      if (mSequence.end() == nextStreamToService)
      {
        nextStreamToService = mSequence.begin();
      }

      mDiag.reordered++;
    }
    else
    {
      // just skip nextStreamToService to the next one
      nextStreamToService = next(nextStreamToService);
    }
  }
}


IasResult IasAvbTransmitSequencer::shutDown()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);
  mLock.lock();
  mThreadControl = cFlagEndThread;
  mLock.unlock();
  return IasResult::cOk;
}


IasResult IasAvbTransmitSequencer::afterRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);
  return IasResult::cOk;
}


IasAvbProcessingResult IasAvbTransmitSequencer::addStreamToTransmitList(IasAvbStream * stream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if ((NULL == stream) || (stream->getTSpec().getClass() != mClass))
  {
    result = eIasAvbProcInvalidParam;
  }
  if (eIasAvbProcOK == result)
  {
    if (!isInitialized())
    {
      result = eIasAvbProcNotInitialized;
    }
  }

  if (eIasAvbProcOK == result)
  {
    AVB_ASSERT(NULL != mTransmitThread);

    const uint32_t newTotal = mCurrentBandwidth + stream->getTSpec().getRequiredBandwidth();
    if (newTotal > mConfig.txMaxBandwidth)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "total bandwidth exceeded (",
          newTotal, "kBit/s)");
      result = eIasAvbProcNoSpaceLeft;
    }
    else
    {

      const uint32_t newMaxIntervalFrames = mCurrentMaxIntervalFrames + stream->getTSpec().getMaxIntervalFrames();
      const uint64_t reqRingSize = uint64_t(IasAvbTSpec::getPacketsPerSecondByClass(mClass)) * uint64_t(newMaxIntervalFrames)
          * mConfig.txWindowWidth / uint64_t(1e9)
          * 2u // two entries per packet
          * 130u / 100u; // 30% margin

      const uint32_t maxRingSize = IasAvbStreamHandlerEnvironment::getTxRingSize();

      if (reqRingSize > maxRingSize)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Required Tx ring size (", reqRingSize,
            " would exceed total Tx ring size(", maxRingSize, ")");
        result = eIasAvbProcNoSpaceLeft;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "::addStreamToTransmitList: increasing total bandwidth:",
            mCurrentBandwidth,
            "->",
            newTotal,
            "kBit/s"
            );

        mCurrentBandwidth = newTotal;
        mCurrentMaxIntervalFrames = newMaxIntervalFrames;

        if (mUseShaper)
        {
          if (IasAvbSrClass::eIasAvbSrClassHigh == mClass)  // Class A
          {
            if (mMaxFrameSizeHigh < stream->getTSpec().getMaxFrameSize())
            {
              // mMaxFrameSizeHigh is required to calculate the HiCredit of ClassB/C
              mMaxFrameSizeHigh = stream->getTSpec().getMaxFrameSize();
            }
          }

          updateShaper();
        }

        {
          mLock.lock();
          (void) mActiveStreams.insert( stream );
          mLock.unlock();
          mRequestCount++;
          sync();
        }
      }
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitSequencer::removeStreamFromTransmitList(IasAvbStream * stream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if ((NULL == stream) || (stream->getTSpec().getClass() != mClass))
  {
    result = eIasAvbProcInvalidParam;
  }

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }

  if (eIasAvbProcOK == result)
  {
    AVB_ASSERT(NULL != mTransmitThread);
    mLock.lock();
    (void) mActiveStreams.erase( stream );
    mLock.unlock();
    mRequestCount++;
    sync();

    if (mTransmitThread->isRunning())
    {
      // wait up to one second for the worker thread to confirm the change
      for (uint32_t i = 0; i < 100000u; i++)
      {
        ::usleep(10u);
        sync();
        if (mRequestCount == mResponseCount)
        {
          break;
        }
      }
      if (mRequestCount != mResponseCount)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "worker thread did not respond");

        // restore the stream
        mLock.lock();
        (void) mActiveStreams.insert( stream );
        mLock.unlock();
        mRequestCount++;
        sync();
        result = eIasAvbProcErr;
      }
    }

    if (eIasAvbProcOK == result)
    {
      const uint32_t bandwidth = stream->getTSpec().getRequiredBandwidth();
      AVB_ASSERT(bandwidth <= mCurrentBandwidth);
      mCurrentBandwidth -= bandwidth;

      const uint32_t maxIntervalFrames = stream->getTSpec().getMaxIntervalFrames();
      AVB_ASSERT(maxIntervalFrames <= mCurrentMaxIntervalFrames);
      mCurrentMaxIntervalFrames -= maxIntervalFrames;

      if (mUseShaper)
      {
        if (IasAvbSrClass::eIasAvbSrClassHigh == mClass)  // Class A
        {
          if (mMaxFrameSizeHigh <= stream->getTSpec().getMaxFrameSize())
          {
            mMaxFrameSizeHigh = 0u;
            for (AvbStreamSet::iterator it = mActiveStreams.begin(); it != mActiveStreams.end(); it++)
            {
              IasAvbStream *activeStream = *it;
              AVB_ASSERT(NULL != activeStream);
              if (mMaxFrameSizeHigh < activeStream->getTSpec().getMaxFrameSize())
              {
                mMaxFrameSizeHigh = activeStream->getTSpec().getMaxFrameSize();
              }
            }
          }
        }

        updateShaper();
      }
    }
  }

  return result;
}


void IasAvbTransmitSequencer::resetPoolsOfActiveStreams()
{
  mLock.lock();

  for (AvbStreamSet::iterator it = mActiveStreams.begin(); it != mActiveStreams.end(); it++)
  {
    IasAvbStream *stream = *it;
    AVB_ASSERT(NULL != stream);
    stream->resetPacketPool();
  }

  mLock.unlock();
}

void IasAvbTransmitSequencer::updateShaper()
{
  double bandWidth = 0.0;
  int32_t  linkSpeed = 0;
  int32_t  idleSlope = -1;
  uint32_t linkRate  = TQAVCC_LINKRATE;
  uint32_t maxInterferenceSize = cTxMaxInterferenceSize;

  uint32_t tqavhcReg   = 0u; // Tx Qav Hi Credit TQAVHC
  uint32_t tqavccReg   = 0u; // Tx Qav Credit Control TQAVCC
  uint32_t tqavctrlReg = 0u; // Tx Qav Control TQAVCTRL

  // get current link speed
  linkSpeed = IasAvbStreamHandlerEnvironment::getLinkSpeed();

  if (100 == linkSpeed)
  {
    // the percentage bandwith out of full line rate @ 100Mbps (mCurrentBandwidth = kBit/s)
    bandWidth = (mCurrentBandwidth * mShaperBwRate / 100) * 1000.0 / 100000000.0;
    idleSlope = static_cast<uint32_t>(bandWidth * 0.2 * (double)linkRate + 0.5);
  }
  else if (1000 == linkSpeed)
  {
    // the percentage bandwith out of full line rate @ 1Gbps (mCurrentBandwidth = kBit/s)
    bandWidth = (mCurrentBandwidth * mShaperBwRate / 100) * 1000.0 / 1000000000.0;
    idleSlope = static_cast<uint32_t>(bandWidth * 2.0 * (double)linkRate + 0.5);
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "unknown link speed", linkSpeed);
    AVB_ASSERT(false);
  }

  if (0 == idleSlope)
  {
    // reset the registers with default value
    tqavhcReg = TQAVCH_ZERO_CREDIT;
    tqavccReg = TQAVCC_QUEUEMODE; // no idle slope
  }
  else if ((0 < idleSlope) && (static_cast<uint32_t>(idleSlope) < linkRate))
  {
    // idleSlope must be smaller than linkRate = 0x7735 credits/byte.

    if (0u == mQueueIndex)
    {
      tqavhcReg = TQAVCH_ZERO_CREDIT + (idleSlope * maxInterferenceSize / linkRate);
    }
    else
    {
      uint32_t idleSlopeClassA = 0u;

      (void) igb_readreg(mIgbDevice, TQAVHC(0), &idleSlopeClassA);
      idleSlopeClassA ^= TQAVCC_QUEUEMODE;

      if ((0 == idleSlopeClassA) || (0xFFFFu == static_cast<uint16_t>(idleSlopeClassA)))
      {
        // idleSlope has not been set. i.e. Class A (Queue0) is not used.
        tqavhcReg = TQAVCH_ZERO_CREDIT + (idleSlope * maxInterferenceSize / linkRate);
      }
      else if (static_cast<int32_t>(linkRate - idleSlopeClassA) > 0)
      {
        /*
         * Add 43 bytes to mMaxFrameSizeHigh since it is the size of AVTP payload without media overhead nor header.
         * (43 = 8 bytes preamble + SFD, 14 bytes Ethernet header, 4 bytes VLAN tag, 4 bytes CRC, 12 bytes IPG, 1 byte SRP)
         */
        maxInterferenceSize = maxInterferenceSize + (mMaxFrameSizeHigh + 43u);
        linkRate = linkRate - idleSlopeClassA;
        tqavhcReg = TQAVCH_ZERO_CREDIT + (idleSlope * maxInterferenceSize / linkRate);
      }
    }

    tqavccReg = (TQAVCC_QUEUEMODE | idleSlope);
  }

  if ((0u != tqavhcReg) && (0u != tqavccReg))
  {
    // HiCredit
    (void) igb_writereg(mIgbDevice, TQAVHC(mQueueIndex), tqavhcReg);

    // QueueMode and IdleSlope
    (void) igb_writereg(mIgbDevice, TQAVCC(mQueueIndex), tqavccReg);

    // implicitly enable the Qav shaper
    (void) igb_readreg(mIgbDevice, TQAVCTRL, &tqavctrlReg);
    tqavctrlReg |= TQAVCTRL_TX_ARB;
    (void) igb_writereg(mIgbDevice, TQAVCTRL, tqavctrlReg);

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "set shaping params (",
        "Queue:", mQueueIndex,
        "Bandwidth:", mCurrentBandwidth * mShaperBwRate / 100, "kBit/s",
        "HiCredit:", tqavhcReg,
        "IdleSlope:", idleSlope,
        "");
  }
}


} // namespace IasMediaTransportAvb
