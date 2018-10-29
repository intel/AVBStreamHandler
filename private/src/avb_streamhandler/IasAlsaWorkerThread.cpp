/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaWorkerThread.cpp
 * @brief   This file contains the implementation of the Alsa WorkerThread.
 * @details
 * @date    2015
 */

#include <time.h> // make sure we're using the correct struct timespec definition
#include "avb_streamhandler/IasAlsaWorkerThread.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"

#include <math.h>
#include <algorithm>
#include <sstream>
#include <cmath>

#include "avb_streamhandler/IasAlsaStreamInterface.hpp"

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAlsaWorkerThread::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"


uint32_t IasAlsaWorkerThread::sInstanceCounter = 0u;


/*
 *  Constructor.
 */
IasAlsaWorkerThread::IasAlsaWorkerThread(DltContext& dltContext)
  : mThisInstance(sInstanceCounter++)
  , mLog(&dltContext)
  , mKeepRunning(false)
  , mThread(NULL)
  , mClockDomain(NULL)
  , mWatchdog(NULL)
  , mAlsaPeriodSize(0u)
  , mSampleFrequency(0u)
  , mServiceCycle(0u)
  , mLock()
  , mDbgCount(0u)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


/**
 *  Destructor.
 */
IasAlsaWorkerThread::~IasAlsaWorkerThread()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();
}


/**
 *  Initialization method.
 */
IasAvbProcessingResult IasAlsaWorkerThread::init(IasAlsaStreamInterface *alsaStream, uint32_t alsaPeriodSize, uint32_t sampleFrequency, IasAvbClockDomain * const clockDomain)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (NULL == mThread)
  {
    if ((NULL != alsaStream) && (NULL != clockDomain) && (0u != alsaPeriodSize) && (0u != sampleFrequency))
    {
      std::stringstream threadName;
      threadName << "AvbAlsaWrk" << mThisInstance;
      mThread = new (nothrow) IasThread(this, threadName.str());
      if (mThread == NULL)
      {
        /**
         * @log Not enough memory: Unable to create a new Worker Thread.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't create worker thread!");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        mAlsaStreams.clear(); // if not cleared already clear it here to start from a well defined state
        mAlsaPeriodSize = alsaPeriodSize;
        mSampleFrequency = sampleFrequency;
        mClockDomain = clockDomain;
        result = addAlsaStream(alsaStream);
        if (eIasAvbProcOK == result)
        {
          /**
           * @log Alsa stream successfully added to the Alsa worker thread stream list.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Clock domain (",
                      uint16_t(mClockDomain->getType()), ") and Stream (",
                      alsaStream->getStreamId(), ") added");

          uint64_t val = 0u;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cUseWatchdog, val);
          if (val && IasAvbStreamHandlerEnvironment::isWatchdogEnabled())
          {
            IasWatchdog::IasSystemdWatchdogManager* wdManager = NULL;
            wdManager = IasAvbStreamHandlerEnvironment::getWatchdogManager();

            if (wdManager)
            {
              uint32_t timeout = IasAvbStreamHandlerEnvironment::getWatchdogTimeout();
              std::string wdName = std::string("AvbAlsaWd");

              mWatchdog = wdManager->createWatchdog(timeout, wdName);
              if (mWatchdog)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " supervised watchdog name:",
                            wdName.c_str(), "timeout:", timeout, "ms");
                result = eIasAvbProcOK;
              }
              else
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not create ",wdName.c_str(), "watchdog");
                result = eIasAvbProcInitializationFailed;
              }
            }
            else
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "AvbStreamHandlerEnvironment could not getWatchdogManager");
	      result = eIasAvbProcInitializationFailed;
            }
          }
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "addAlsaStream failed:", uint32_t(result));
        }
      }
    }
    else
    {
      /**
       * @log The Alsa stream and/or the clock domain parameters were null.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid parameter!");
      result = eIasAvbProcInvalidParam;
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Worker thread is already initialized!");
    result = eIasAvbProcInitializationFailed;
  }

  return result;
}


/**
 *  Start method.
 */
IasAvbProcessingResult IasAlsaWorkerThread::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mThread != NULL)
  {
    if (!mThread->isRunning()) // If thread isn't already running start it
    {
      mKeepRunning = true;
      IasThreadResult ret = mThread->start(true);
      if (IAS_FAILED(ret))
      {
        /**
         * @log Thread start failed: The alsa worker thread couldn't be started and returned a runtime error.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't start Alsa worker thread! Error =",
          ret.toString());
        result = eIasAvbProcThreadStartFailed;
      }
    }
  }
  else
  {
    /**
     * @log The Alsa worker thread was NULL.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Worker thread can't be started since it's not available!");
    result = eIasAvbProcThreadStartFailed;
  }

  // notify local streams that the worker is ready for data handling
  if (result == eIasAvbProcOK)
  {
    AlsaStreamList::iterator it;
    for (it = mAlsaStreams.begin(); it != mAlsaStreams.end(); it++)
    {
      AVB_ASSERT(NULL != *it);
      (*it)->setWorkerActive(true);
    }
  }

  return result;
}


/**
 *  Stop method.
 */
IasAvbProcessingResult IasAlsaWorkerThread::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  mLock.lock();

  AlsaStreamList::iterator it;
  for (it = mAlsaStreams.begin(); it != mAlsaStreams.end(); it++)
  {
    AVB_ASSERT(NULL != *it);
    (*it)->setWorkerActive(false);
  }

  mLock.unlock();

  if (mThread != NULL)
  {
    if (mThread->isRunning())
    {
      IasThreadResult ret = mThread->stop(); // Note: shutdown() is called and thread will join by default
      if (IAS_SUCCEEDED(ret))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Worker thread has stopped.");
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stopping worker thread failed");
        result = eIasAvbProcThreadStopFailed;
      }
    }
  }
  else
  {
    /**
     * @log Alsa worker thread is NULL.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Worker thread can't be stopped since it's not available!");
    result = eIasAvbProcThreadStopFailed;
  }

  return result;
}


/**
 *  Cleanup method.
 */
void IasAlsaWorkerThread::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (NULL != mThread)
  {
    if (mThread->isRunning())
    {
      mThread->stop(); // joins by default
    }
    delete mThread;
    mThread = NULL;
  }

  if (mWatchdog)
  {
    IasWatchdog::IasSystemdWatchdogManager* wdManager = NULL;
    wdManager = IasAvbStreamHandlerEnvironment::getWatchdogManager();

    if (wdManager)
    {
      (void) wdManager->destroyWatchdog(mWatchdog);
      mWatchdog = NULL;
    }
  }

  mKeepRunning = false;
  mClockDomain = NULL;
  mAlsaPeriodSize = 0u;
  mSampleFrequency = 0u;
  mAlsaStreams.clear();
}


bool IasAlsaWorkerThread::checkParameter(IasAvbClockDomain * const clockDomain,
                                                uint32_t alsaPeriodSize, uint32_t sampleFrequency) const
{
  bool ret = false;

  if (mClockDomain == clockDomain)
  {
    ret = checkParameter(alsaPeriodSize, sampleFrequency, mAlsaPeriodSize, mSampleFrequency);
  }

  return ret;
}

bool IasAlsaWorkerThread::checkParameter(uint32_t alsaPeriodSize, uint32_t sampleFrequency, uint32_t periodBase, uint32_t frequencyBase)
{
  bool ret = false;

  const uint64_t p = uint64_t(alsaPeriodSize) * uint64_t(frequencyBase);
  const uint64_t q = uint64_t(periodBase) * uint64_t(sampleFrequency);

  /* The period time (period size / sample frequency) of the new stream has to be
   * an integer multiple of the period time the thread is working on.
   */
  if ((0u != q) && (p/q >= 1) && (0u == p % q))
  {
    ret = true;
  }

  return ret;
}



IasResult IasAlsaWorkerThread::beforeRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Worker thread ", mThread->getName(),
              "is starting...");
  return IasResult::cOk;
}


IasResult IasAlsaWorkerThread::run()
{
  IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();

  const uint32_t sleepEstimate = uint32_t((uint64_t(mAlsaPeriodSize) * uint64_t(1000000000u)) / uint64_t(mSampleFrequency));

  uint64_t cTimeout         = 1000000000u; // 1 second
  uint64_t cAdjustCycle     =    5000000u; // how often is the sleepInterval adjusted
  double cGain              =          .1; // adjustment in ns per 48kHz sample deviation
  uint32_t cThreshold       =      50000u; // reinit ("unlock") if deviation gets larger than this
  uint32_t maxSleepInterval =          0u; // reinit if overslept more than this ns
  // NOTE: 5ns is 1ppm at 48kHz/256 period size

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaClockTimeout, cTimeout);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaClockCycle, cAdjustCycle);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaClockUnlock, cThreshold);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaClockResetThresh, maxSleepInterval);
  uint32_t val = 0u;
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaClockGain, val))
  {
    cGain = double(val) / 1e3;
  }

  struct timespec tp;
  uint64_t now              = 0u;
  uint32_t sleepInterval    = 0u;
  bool initInterval         = true;
  uint64_t slaveTime        = 0u;
  uint64_t slaveTimePtp     = 0u;
  uint64_t timestamp        = 0u;
  int64_t slaveCount        = 0u;
  int64_t offset            = 0;
  int64_t lastCountMaster   = 0;
  int64_t lastTimeMaster    = 0;
  uint64_t lastMasterUpdate = 0u;
  uint64_t lastAdjustment   = 0u;
  uint64_t lastDebugOut     = 0u;
  double deviation          = 0.0;
  double masterRate         = 0.0;
  int64_t debugTimeOffset   = 0;
  int rc                    = 0;    // POSIX return codes

  struct sched_param sparam;
  std::string policyStr = "fifo";   // these values get overwritten by the default settings (fifo, prio=20) or
  int32_t priority      = 1;        // other values are specified in commnand line via '-k' option

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPolicy, policyStr);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPriority, priority);

  const int32_t policy  = (policyStr == "other") ? SCHED_OTHER : (policyStr == "rr") ? SCHED_RR : SCHED_FIFO;
  sparam.sched_priority = priority;

  rc = pthread_setschedparam(pthread_self(), policy, &sparam);
  if(0 != rc)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error setting scheduler parameter: ", strerror(rc));
  }

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, mThread->getName().c_str(),
          mThisInstance, "is running. Sample Freq =", mSampleFrequency,
          "kHz. Period size =", mAlsaPeriodSize);

  uint64_t sleepUntilPtp = 0u;
  bool isRefClkAvail     = false;
  uint64_t lastOversleep = 0u;
  uint32_t lastEpoch = (nullptr != ptp) ? ptp->getEpochCounter() : 0u;

  while (mKeepRunning)
  {
    rc = clock_gettime(IasLibPtpDaemon::cSysClockId, &tp);
    if (rc < 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, mThisInstance, " Failure reading clock: ",
              strerror(rc));
      mKeepRunning = false;
      break;
    }
    now = IasLibPtpDaemon::convertTimespecToNs(tp);

    // if we're more than a full period late, output a warning message
    const uint64_t over = now - slaveTime;
    if (!initInterval && (over > sleepInterval))
    {
      // do not print out this warning more often than once a period time
      if ((slaveTime - lastOversleep) > sleepInterval)
      {
        std::stringstream text;
        text << mThisInstance << " Alsa Engine worker thread slept too long";
        IasAvbStreamHandlerEnvironment::notifySchedulingIssue(*mLog, text.str(), (sleepInterval + over), sleepInterval);
      }

      // if we're more than specified maxSleepInterval late, re-initialize the control loop
      const uint64_t cMaxSleepInterval = maxSleepInterval ? maxSleepInterval : sleepInterval;
      if (over > cMaxSleepInterval)
      {
        /*
         * Resetting slaveTime will cause buffer underrun. Keep current slaveTime as long as overslept
         * time is less than the specified interval so that it can avoid buffer underrun by producing
         * an adequate amount of samples which can compensates delayed sample count.
         */
        slaveTime = 0u;
        initInterval = true;
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, mThread->getName(),
                    "overslept more than", (double)cMaxSleepInterval/1e6, "ms, reinitializing");
      }

      lastOversleep = slaveTime;
    }

    uint64_t masterTime = 0u;
    int64_t masterCount = int64_t(mClockDomain->getEventCount(masterTime));
    const uint32_t eventRate = mClockDomain->getEventRate();
    if (0u != eventRate)
    {
      masterCount = (masterCount * mSampleFrequency) / eventRate;
    }
    else
    {
      if (0u != lastMasterUpdate)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, mThisInstance,
                " Event Rate == 0 while receiving packets!");
      }
    }

    if (masterCount < lastCountMaster)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, mThisInstance, "negative master clock count change!");
      initInterval = true;
      isRefClkAvail = false;
    }

    if (initInterval)
    {
      initInterval = false;
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, mThisInstance,
              " initializing the sleep interval to ", sleepEstimate);

      /* The initial interval is only an estimate. Since we're in a closed control loop,
       * it'll center in on its own. Also, we don't have to mess with rounding issues.
       */
      sleepInterval = sleepEstimate;
      offset = slaveCount - masterCount;
      if (0u == slaveTime)
      {
        slaveTime = now;
      }
      lastAdjustment = slaveTime;
      debugTimeOffset = (masterTime - slaveTime);
      masterRate = double(mSampleFrequency) / 1e9;
      deviation = 0.0; // only to make debug output less confusing

      if (NULL != ptp)
      {
        slaveTimePtp = ptp->sysToPtp(slaveTime);
      }

      sleepUntilPtp    = 0u;
      lastMasterUpdate = 0u; // trigger offset reset after the 'deviation out of bounds' error
    }
    else
    {
      double timeOffset = 0.0;

      /* A single watchdog timer reset is sufficient.
         It should get kicked within our Wd interval */
      if (mWatchdog)
      {
        if (mWatchdog->isRegistered())
        {
          (void) mWatchdog->reset();
        }
        else
        {
          if (IasResult::cOk != mWatchdog->registerWatchdog())
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " watchdog registration failure...");
            mKeepRunning = false;
          }
          else
          {
            (void) mWatchdog->reset();
          }
        }
      }
      // NOTE: ptp might be NULL in unit test context
      if (NULL != ptp)
      {
        slaveTimePtp = ptp->sysToPtp(slaveTime);
        timeOffset = double(int64_t(slaveTimePtp - masterTime));
      }

      if ((slaveTime - lastAdjustment) >= cAdjustCycle)
      {
        lastAdjustment = slaveTime;

        /* Compare sample count values, extrapolated to the same point in time:
         * The point in time is defined as the last targeted wake-up time of this thread,
         * when the next period of samples is due to be processed. The time must be converted
         * from local system time to PTP time in order to make it comparable to the master time stamp.
         */

        if (0u == lastTimeMaster)
        {
          lastTimeMaster = masterTime;
        }

        const int64_t deltaTM = int64_t(masterTime - lastTimeMaster);
        if (0 == deltaTM)
        {
          /**
           * @log The current master time and the last master time are equal.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "[wt", mThisInstance, "::run] no master update",
                  lastMasterUpdate, "span", now - lastMasterUpdate);

          if ((0u != lastMasterUpdate) && ((now - lastMasterUpdate) > cTimeout))
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, mThisInstance,
                    "timeout for reference clock update");
            lastMasterUpdate = 0u;
            lastTimeMaster   = 0u;
            isRefClkAvail    = false;
            // no other error handling here, the only thing we can do is to let the ALSA interface freewheel
          }
        }
        else
        {
          if (0u == lastMasterUpdate)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, mThisInstance,
                    " reference clock available, resetting offset");
            // offset = 'actual slave count' - 'ideal slave count'
            offset = slaveCount - (uint64_t)((double)masterCount + (timeOffset * masterRate));
          }
          else
          {
            masterRate = double(masterCount - lastCountMaster) / double(deltaTM);
          }
          lastMasterUpdate = now;
          isRefClkAvail = true;
        }

        if (0u != lastMasterUpdate)
        {
          // calculate ideal slave count based on master rate and compare with actual slave count
          // NOTE: this expression is rearranged a bit to avoid converting back and forth between float and int
          deviation = double((slaveCount - masterCount) - offset) - (timeOffset * masterRate);

          if (fabs(deviation) > cThreshold)
          {
            /**
             * @log The deviation has exceeded the threshold.
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, mThisInstance,
                    "deviation out of bounds:", deviation);
            initInterval  = true;
            isRefClkAvail = false;
            // increment diag counter for all affected streams
            for (auto it = mAlsaStreams.begin(); it != mAlsaStreams.end(); ++it)
            {
              IasLocalAudioStreamDiagnostics * audioDiag = (*it)->getDiag();
              audioDiag->setDeviationOutOfBounds(audioDiag->getDeviationOutOfBounds() + 1);
            }
          }
          else
          {
            if (0 < deltaTM)
            {
              sleepInterval += int32_t(deviation * cGain);
            }
            else
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, mThisInstance, "Skip sleepInterval update, deltaTM=",
                          deltaTM);
            }
          }
        }
      }
    }
    lastCountMaster = masterCount;
    lastTimeMaster = masterTime;

    DltLogLevelType loglevel = DLT_LOG_VERBOSE;
    if ((now - lastDebugOut) >= 1000000000u)
    {
      lastDebugOut = now;
      loglevel = DLT_LOG_DEBUG;
    }

    DLT_LOG_CXX(*mLog, loglevel, LOG_PREFIX, mThisInstance,
        "<< m=", masterCount,
        "/", masterTime,
        "s=", slaveCount,
        "/", slaveTime,
        "d=", deviation,
        "i=", int32_t(sleepInterval-sleepEstimate),
        "drift=", (masterTime - slaveTime) - debugTimeOffset,
        "r=", int32_t(initInterval),
        "o=", offset,
        "p=", slaveTimePtp,
        "mr=", masterRate*1e9-48000.0,
        ">>");

    // set timestamp 0 if reference clock is not available, it will let the ALSA interface freewheel
    timestamp = isRefClkAvail ? slaveTimePtp : 0u;

    // process one period of samples
    process(timestamp);

    slaveCount += mAlsaPeriodSize;
    slaveTime += sleepInterval;

    if (!initInterval && (0u != timestamp)) // if ptp time is reliable
    {
      // determine next wake-up time in ptp by cumulatively adding sleepInterval to the first timestamp
      sleepUntilPtp = (0u == sleepUntilPtp) ? (timestamp + sleepInterval) : (sleepUntilPtp + sleepInterval);
      // convert ptp time to tsc time
      // NOTE: ptp might be NULL in unit test context / calm down static code analysis
      if (NULL != ptp)
      {
         slaveTime = ptp->ptpToSys(sleepUntilPtp);
      }
    }
    else
    {
      sleepUntilPtp = 0u;
    }

    uint32_t epoch = (nullptr != ptp) ? ptp->getEpochCounter() : 0u;
    if (epoch != lastEpoch)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "ptp time warp detected - restarting", mThread->getName(), "thread");

      lastEpoch = epoch;
      slaveTime = 0u;
      initInterval = true;
      isRefClkAvail = false;
      continue;
    }

    tp.tv_sec = time_t(slaveTime / uint64_t(1000000000u));
    tp.tv_nsec = long(slaveTime - (uint64_t(int64_t(tp.tv_sec)) * uint64_t(1000000000u)));
    rc = clock_nanosleep(IasLibPtpDaemon::cSysClockId, TIMER_ABSTIME, &tp, NULL);
    if (rc < 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, mThisInstance,
                    "clock_nanosleep failure: ", strerror(rc));
      mKeepRunning = false;
    }
  }

  if (mWatchdog && mWatchdog->isRegistered())
    (void) mWatchdog->unregisterWatchdog();

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, mThisInstance,
                    "Worker thread ", mThread->getName(), "has stopped");

  return IasResult::cOk;
}


IasResult IasAlsaWorkerThread::shutDown()
{
  mKeepRunning = false;

  return IasResult::cOk;
}


IasResult IasAlsaWorkerThread::afterRun()
{
  mKeepRunning = false;
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Worker thread ", mThread->getName(), "ended.");

  return IasResult::cOk;
}


IasAvbProcessingResult IasAlsaWorkerThread::addAlsaStream(IasAlsaStreamInterface *alsaStream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "not initialized!");
    result = eIasAvbProcNotInitialized;
  }
  else
  {
    AVB_ASSERT(0u != mAlsaPeriodSize);
    AVB_ASSERT(0u != mSampleFrequency);

    if ((NULL != alsaStream) && (0u != alsaStream->getPeriodSize()) && (0u != alsaStream->getSampleFrequency()))
    {
      mLock.lock();

      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Adding Alsa stream (", alsaStream->getStreamId(),
                   ") to the worker thread");

      if (std::find(mAlsaStreams.begin(), mAlsaStreams.end(), alsaStream) != mAlsaStreams.end())
      {
        // Stream is already in the list
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid parameter, stream with ID =",
                    alsaStream->getStreamId(), "is already on the list");
        result = eIasAvbProcInvalidParam;
      }
      else
      {
        // Add stream to the stream list

        // calculate number of cycles that have to pass before this stream is serviced
        const uint64_t p = uint64_t(alsaStream->getPeriodSize() * uint64_t(mSampleFrequency));
        const uint64_t q = uint64_t(mAlsaPeriodSize) * uint64_t(alsaStream->getSampleFrequency());

        /* The period time (period size / sample frequency) of the new stream has to be
         * an integer multiple of the period time the thread is working on.
         * This should have been verified already by calling checkParameter().
         */
        if (0u == p % q)
        {
          alsaStream->setCycle(uint32_t(p / q));
          // insert at the beginning of the list to ensure first device is always serviced last
          mAlsaStreams.insert(mAlsaStreams.begin(), alsaStream);

          (void) alsaStream->setWorkerActive(mThread->isRunning());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "period time mismatch!");
          result = eIasAvbProcInvalidParam;
        }
      }

      mLock.unlock();
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid Alsa Stream!");
      result = eIasAvbProcInvalidParam;
    }
  }

  return result;
}


IasAvbProcessingResult IasAlsaWorkerThread::removeAlsaStream(const IasAlsaStreamInterface * const alsaStream, bool &lastStream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (NULL != alsaStream)
  {
    mLock.lock();

    // Search the stream list and remove the specified stream from the list
    AlsaStreamList::iterator it = std::find(mAlsaStreams.begin(), mAlsaStreams.end(), alsaStream);
    if (mAlsaStreams.end() != it)
    {
      mAlsaStreams.erase(it);
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Stream ", alsaStream->getStreamId(), "removed");

      // Check if this was the last stream.
      if (0 == mAlsaStreams.size())
      {
        // Inform the calling function that this was the last stream so it can stop the worker thread.
        lastStream = true;
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Last stream removed");
      }
      else
      {
        lastStream = false;
      }
    }
    else
    {
      /**
       * @log Invalid param: The streamId associated with the ALSA stream parameter provided is not contained in the stream list. Cannot remove it.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stream", alsaStream->getStreamId(), "to remove not found!");
      result = eIasAvbProcInvalidParam;
    }

    mLock.unlock();
  }
  else
  {
    /**
     * @log Invalid param: AlsaStream == NULL
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid parameter! (alsaStream == NULL!)");
    result = eIasAvbProcInvalidParam;
  }

  return result;
}


bool IasAlsaWorkerThread::streamIsHandled(uint16_t streamId)
{
  bool retval = false;

  AlsaStreamList::iterator it;
  for (it = mAlsaStreams.begin(); it != mAlsaStreams.end(); it++)
  {
    AVB_ASSERT(NULL != *it);
    if (streamId == (*it)->getStreamId())
    {
      retval = true;
      break;
    }
  }

  return retval;
}


void IasAlsaWorkerThread::process(uint64_t timestamp)
{
  mLock.lock();

  for (AlsaStreamList::iterator it = mAlsaStreams.begin(); it != mAlsaStreams.end(); it++)
  {
    IasAlsaStreamInterface *stream = *it;
    AVB_ASSERT(NULL != stream);

    if (nullptr != stream)
    {
      // if cycle has been set to >1 for the stream, only (periodSize / cycle) samples will be processed
      // Transfer audio data from local audio buffer to shared memory or vice versa
      if (100u < mDbgCount)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "servicing stream ", stream->getStreamId());
      }
      stream->copyJob(timestamp);

      // Inform the registered client about status changes
      stream->updateBufferStatus();
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "(stream == NULL!");
    }
  }

  mServiceCycle++;

  if (100u < mDbgCount)
  {
    mDbgCount = 0u;
  }
  mDbgCount++;

  mLock.unlock();
}


} // namespace IasMediaTransportAvb


// *** EOF ***
