/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file    IasLibPtPDaemon.cpp
 *  @brief   Library for the PTP daemon.
 *  @date    2013
 */

#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasDiaLogger.hpp"

#include <unistd.h>
#include <sys/mman.h>   // For shared memory mapping
#include <fcntl.h>      // For O_* constants
#include <pthread.h>    // For mutex
#include <cstring>      // For memcpy
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <stdlib.h>
#include <signal.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

#include <dlt_cpp_extension.hpp>

#ifdef __ANDROID__
#include <shm_posix.h>
#endif

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLibPtpDaemon::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

#define TSAUXC    0x0B640
#define TSAUXC_SAMP_AUTO 0x00000008
#define AUXSTMPH0 0x0B660
#define AUXSTMPL0 0x0B65C

/**
 *  Constructor.
 */
IasLibPtpDaemon::IasLibPtpDaemon(std::string const &sharedMemoryName, uint32_t const &sharedMemorySize)
  : mInstanceName("IasLibPtpDaemon")
  , mInitialized(false)
  , mSharedMemoryFd(-1)
  , mMemoryOffsetBuffer(NULL)
  , mSharedMemoryName(sharedMemoryName)
  , mSharedMemorySize(sharedMemorySize)
  , mTscToLocalFactor(1.0)
  , mRawToLocalFactor(1.0)
  , mRawNormalFactorDeviation(0.005) // cNormalFactorDeviation
  , mAvgCoeff(1.0)
  , mRawAvgCoeff(1.0)
  , mLastTime(0u)
  , mLastLocalTimeforRaw(0u)
  , mLastTsc(0u)
  , mLastRaw(0u)
  , mEpochCounter(0u)
  , mClockHandle(-1)
  , mClockId(-1)
  , mAvgDelta(0.0f)
  , mLastTimeMutex()
  , mProcessId(0)
  , mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_PTP"))
  , mIgbDevice(NULL)
  , mLocalTimeUpdating(false)
  , mMaxCrossTimestampSamples(cMaxCrossTimestampSamples)
  , mSysTimeMeasurementThreshold(cSysTimeMeasurementThreshold)
  , mRawXtstampEn(eRawXtstampImplDisable)
  , mDiag()
  , mTscEpoch(0u)
  , mTscFreq(0u)
  , mRawToLocalTstampThreshold(cRawTimeMeasurementThreshold)
  , mRawToLocalFactors()
{
  DLT_LOG_CXX(*mLog,  DLT_LOG_VERBOSE, LOG_PREFIX);
}


IasLibPtpDaemon::Diag::Diag()
  : rawXCount(0u)
  , rawXFail(0u)
  , rawXMaxInt(0u)
  , rawXMinInt(0u)
  , rawXTotalInt(0u)
{
}


/**
 *  Destructor.
 */
IasLibPtpDaemon::~IasLibPtpDaemon()
{
  DLT_LOG_CXX(*mLog,  DLT_LOG_VERBOSE, LOG_PREFIX);

  cleanUp();
}


/**
 *  Initialization method.
 */
IasAvbProcessingResult IasLibPtpDaemon::init()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // If already initialized skip this
  if (mInitialized)
  {
    DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "Already initialized!");
  }
  else
  {
    // open shared memory provided by PTP daemon
    mSharedMemoryFd = shm_open(mSharedMemoryName.c_str(), O_RDWR, 0);
    if (-1 == mSharedMemoryFd)
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Couldn't open shared memory:", strerror(errno));
      result = eIasAvbProcInitializationFailed;
    }

    // Map the shared memory device into virtual memory
    if (eIasAvbProcOK == result)
    {
      mMemoryOffsetBuffer = static_cast<uint8_t*>(mmap(NULL, mSharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, mSharedMemoryFd, 0));
      if (MAP_FAILED == mMemoryOffsetBuffer)
      {
        DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "mmap() failed!");
        mMemoryOffsetBuffer = NULL;
        shm_unlink(SHM_NAME);
        result = eIasAvbProcInitializationFailed;
      }
    }

    if (eIasAvbProcOK == result)
    {

      std::string path;
      if (!IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cNwIfPtpDev, path))
      {
        getPtpDevice(path);
      }

      if (path.empty())
      {
        DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Failed to get ptp device path!");
        result = eIasAvbProcInitializationFailed;
      }
      else
      {
        mClockHandle = ::open( path.c_str(), O_RDONLY );
        if (mClockHandle < 0)
        {
          DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Failed to open PTP clock device",
              path.c_str(), ": Error=", errno, strerror(errno));

          DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Falling back to register-based access");
        }
        else
        {
          mClockId = ((~(clockid_t(mClockHandle)) << 3) | 3);
          AVB_ASSERT(mClockId != -1);
          DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "Using", path.c_str(), "to access ptp time / local time");
        }
      }
    }

    if (eIasAvbProcOK == result)
    {
      /*
       * mIgbDevice might be NULL for some unit test cases. IasLibPtpDaemon will run
       * in a fallback mode by using generic clock_gettime() instead of libigb API in
       * those cases.
       */
      mIgbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

      // initialize cross-timestamp parameters before getting initial cross-timestamp in calculateConversionCoeffs()
      IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cPtpXtstampThresh, mSysTimeMeasurementThreshold);
      IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cPtpXtstampLoopCount, mMaxCrossTimestampSamples);
      IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRawXTimestamp, mRawXtstampEn);

      if (mRawXtstampEn)
      {
        if (eRawXtstampImplRev2 == mRawXtstampEn)
        {
          mRawToLocalTstampThreshold = 300u;
          mRawNormalFactorDeviation = 0.094 * 1e-6;
        }

        IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRawXtstampThresh, mRawToLocalTstampThreshold);

        uint64_t val = 0;
        if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRawDeviationUnlock, val))
        {
          mRawNormalFactorDeviation = double(val) * 1e-7; // 0.1 ppm (e.g. clockdomain.raw.deviation.unlock=10 means 1 ppm)
        }

        val = 0;
        if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRawRatioToPtp, val))
        {
          mRawToLocalFactor = double(val) * 1e-7;
        }

        std::ostringstream strInitFactor;
        strInitFactor << std::setprecision(7) << std::setiosflags(std::ios::fixed) << mRawToLocalFactor;

        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "raw xtstamp:", (mRawXtstampEn) ? "on," : "off,", "rev:", mRawXtstampEn,
                                                     "clock unlock:", mRawNormalFactorDeviation * 1e6, "ppm,",
                                                     "initial ratio to ptp:", strInitFactor.str());

        if (1.0 != mRawToLocalFactor)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "raw rate ratio smoothing stabilized:", strInitFactor.str());
          mRawAvgCoeff = 0.1;
        }

        if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRawTscFreq, mTscFreq))
        {
          mTscFreq /= 1000; // convert to kHz
        }
        else
        {
          result = detectTscFreq();
        }

        if (0u != mTscFreq)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "tsc frequency:", mTscFreq, "kHz");
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "unable to get tsc frequency");
          result = eIasAvbProcErr; // if user-specified freq was 0
        }
      }

      if (eIasAvbProcOK == result)
      {
        result = calculateConversionCoeffs();
      }
    }

    // Check the result
    if (eIasAvbProcOK == result)
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "PTP proxy successfully initialized!");
      mInitialized = true;
    }
    else
    {
      cleanUp();
    }
  }
  return result;
}

void IasLibPtpDaemon::getPtpDevice(std::string & path)
{
  path.clear();
  struct ethtool_ts_info info;
  struct ifreq ifr;
  const std::string* nwIfName = IasAvbStreamHandlerEnvironment::getNetworkInterfaceName();
  int32_t sd;

  // already opened or error querying network interface name
  if (NULL == nwIfName)
  {
    DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "No network interface name specified!");
  }
  else
  {
    sd = socket(PF_UNIX, SOCK_DGRAM, 0);

    if (sd < 0)
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Error opening socket for ptp device selection:", errno, strerror(errno));
    }
    else
    {
      memset(&info, 0, sizeof info);
      info.cmd = ETHTOOL_GET_TS_INFO;

      memset(&ifr, 0, sizeof ifr);
      strncpy(ifr.ifr_name, nwIfName->c_str(), (sizeof ifr.ifr_name) - 1u);
      ifr.ifr_name[(sizeof ifr.ifr_name) - 1u] = '\0';
      ifr.ifr_data = &info;

      if (ioctl(sd, SIOCETHTOOL, &ifr) < -1)
      {
        DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Error querying ptp device for:",
            nwIfName->c_str(), ": Error=", errno, strerror(errno));
      }
      else
      {
        char buf[16];
        snprintf(buf, sizeof buf, "%d", info.phc_index);
        buf[(sizeof buf) - 1u] = '\0';
        path = "/dev/ptp";
        path += buf;
        DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "Device path set to", path.c_str());
      }

      close(sd);
    }
  }
}


IasAvbProcessingResult IasLibPtpDaemon::calculateConversionCoeffs()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (-1 == mClockId)
  {
    ret = eIasAvbProcInitializationFailed;
  }
  else
  {
    // just init the "last time" members and set the initial conversion rate to 1.0
    mLastTsc = getTsc();
    mLastRaw = getRaw();
    struct timespec tp;
    if (clock_gettime(mClockId, &tp) < 0)
    {
      ret = eIasAvbProcInitializationFailed;
    }
    else
    {
      mLastTime = convertTimespecToNs(tp);
      mLastLocalTimeforRaw = mLastTime;
      mTscToLocalFactor = 1.0;
      mRawToLocalFactor = 1.0;
      mAvgCoeff = 1.0;
      mRawAvgCoeff = 1.0;

      if (mRawXtstampEn) // raw specific code
      {
        const uint64_t cRawInitXtstampMaxTrial = (eRawXtstampImplRev2 == mRawXtstampEn) ? (std::numeric_limits<uint64_t>::max()) : (10);
        for (uint64_t i = 0; i < cRawInitXtstampMaxTrial; i++)
        {
          if (getIgbTime(mLastLocalTimeforRaw, mLastRaw, cRawClockId) == eIasAvbProcOK)
          {
            break;
          }
          if ((i + 1) == cRawInitXtstampMaxTrial)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "raw-x-tstamp: initial precision cross-timestaming failed.",
                                                         "timestamp jitters may happen at the beginning of tx streams.");

            // fall-back for only at init phase
            uint64_t raw1 = 0u;
            uint64_t raw2 = 0u;
            struct timespec tp;
            raw1 = getRaw();
            clock_gettime(mClockId, &tp);
            raw2 = getRaw();
            mLastRaw = (raw1 >> 1) + (raw2 >> 1);
            mLastLocalTimeforRaw = convertTimespecToNs(tp);
          }
        }
        mRawToLocalFactors.clear();
      }
    }
  }

  return ret;
}


void IasLibPtpDaemon::cleanUp()
{
  // Unmap shm device
  if (NULL != mMemoryOffsetBuffer)
  {
    munmap(mMemoryOffsetBuffer, mSharedMemorySize);
    mMemoryOffsetBuffer = NULL;
  }

  // Close shared memory
  if (-1 != mSharedMemoryFd)
  {
    close(mSharedMemoryFd);
    mSharedMemoryFd = -1;
  }

  if (-1 != mClockHandle)
  {
    mClockId = -1;
    close(mClockHandle);
    mClockHandle = -1;
  }

  mProcessId = 0;

  mInitialized = false;
}

uint64_t IasLibPtpDaemon::rawToPtp(const uint64_t sysTime) const
{
  (void) mLastTimeMutex.lock();
  const uint64_t offset1 = mLastRaw;
  const double factor = mRawToLocalFactor;
  const uint64_t offset2 = mLastLocalTimeforRaw;
  (void) mLastTimeMutex.unlock();

  if (!mRawXtstampEn)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "cross timestamps for RawClockDomain is disabled. "
                                                  "check if the 'clockdomain.raw.xtstamp' key is set.");
  }

  DLT_LOG_CXX(*mLog,  DLT_LOG_VERBOSE, LOG_PREFIX, "lastRaw", offset1,
      "factor", factor,
      "lastTime", offset2);

  return uint64_t(int64_t(double(int64_t(sysTime - offset1)) * factor)) + offset2;
}


uint64_t IasLibPtpDaemon::sysToPtp(const uint64_t sysTime) const
{
  (void) mLastTimeMutex.lock();
  const uint64_t offset1 = mLastTsc;
  const double factor = mTscToLocalFactor;
  const uint64_t offset2 = mLastTime;
  (void) mLastTimeMutex.unlock();

  DLT_LOG_CXX(*mLog,  DLT_LOG_VERBOSE, LOG_PREFIX, "lastTsc", offset1,
      "factor", factor,
      "lastTime", offset2);

  return uint64_t(int64_t(double(int64_t(sysTime - offset1)) * factor)) + offset2;
}


uint64_t IasLibPtpDaemon::ptpToSys(const uint64_t ptpTime) const
{
  (void) mLastTimeMutex.lock();
  const uint64_t offset1 = mLastTime;
  const double factor = bool(mTscToLocalFactor) ? 1 / mTscToLocalFactor : 0.0;
  const uint64_t offset2 = mLastTsc;
  (void) mLastTimeMutex.unlock();

  DLT_LOG_CXX(*mLog,  DLT_LOG_VERBOSE, LOG_PREFIX, "lastTime", offset1,
      "factor", factor,
      "lastTsc", offset2);

  return uint64_t(int64_t(double(int64_t(ptpTime - offset1)) * factor)) + offset2;
}


uint64_t IasLibPtpDaemon::getLocalTime()
{
  (void) mLastTimeMutex.lock();
  const uint64_t offset1 = mLastTsc;
  const double factor = mTscToLocalFactor;
  const uint64_t offset2 = mLastTime;

  bool doUpdate = false;

  uint64_t ret = uint64_t(int64_t(double(int64_t(getTsc() - offset1)) * factor));

  if ((ret > cUpdateThreshold) && !mLocalTimeUpdating)
  {
    /*
     * prevent concurrent calls to getRealLocalTime()
     *
     * The getRealLocalTime() will lock the IGB device to read the I210 clock from its registers.
     * This lock would block threads using the libigb API such as the TX sequencer.
     * Prevent calling getRealLocalTime() if the updating is already in progress on other thread
     * so that we can avoid unnecessary blocking.
     */
    doUpdate = true;
    mLocalTimeUpdating = true;
  }

  (void) mLastTimeMutex.unlock();

  DLT_LOG_CXX(*mLog,  DLT_LOG_VERBOSE, LOG_PREFIX, "lastTsc", offset1,
      "factor", factor,
      "lastTime", offset2);

  if (doUpdate)
  {
    ret = getRealLocalTime();

    (void) mLastTimeMutex.lock();
    mLocalTimeUpdating = false;
    (void) mLastTimeMutex.unlock();
  }
  else
  {
    ret += offset2;
  }

  return ret;
}

bool IasLibPtpDaemon::isPtpReady()
{
  bool ptpReadyState = false;
  uint8_t* pData;
  gPtpTimeData* pPtpTimeData;
  uint32_t syncCount   = 0;
  uint32_t pdelayCount = 0;
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cPtpPdelayCount, pdelayCount);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cPtpSyncCount, syncCount);

  if (NULL != mMemoryOffsetBuffer)
  {
    pData = mMemoryOffsetBuffer + sizeof(pthread_mutex_t);
    pPtpTimeData = reinterpret_cast<gPtpTimeData*>(pData);
    mProcessId = pPtpTimeData->process_id; // save the process id of PTP daemon to be able to send a signal to it

    if ((pPtpTimeData->port_state == PTP_MASTER) && (pPtpTimeData->pdelay_count >= pdelayCount))
    {
      // @@DIAG for master devices, this indicates we've reached "AVB sync state" - BUT NEEDS CONFIRMATION
      ptpReadyState = true;
    }
    else if ((pPtpTimeData->port_state == PTP_SLAVE) && (pPtpTimeData->sync_count >= syncCount))
    {
      // @@DIAG for slave devices, this indicates we've reached "AVB sync state"
      ptpReadyState = true;
    }
    else
    {
      ptpReadyState = false;
    }
  }

  if (ptpReadyState)
  {
    IasDiaLogger* diaLogger = IasAvbStreamHandlerEnvironment::getDiaLogger();
    const int32_t* socket = IasAvbStreamHandlerEnvironment::getStatusSocket();
    if ((NULL != diaLogger) && (NULL != socket))
    {
      diaLogger->triggerAvbSyncPacket(*socket);
    }
  }

  return ptpReadyState;
}

IasAvbProcessingResult IasLibPtpDaemon::triggerStorePersistenceData() const
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (0 != mProcessId)
  {
    DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "Sending 'hangup' signal to PID =", mProcessId);

    if (-1 == kill(mProcessId, SIGHUP)) // send signal 'Hangup' (-1) to PTP daemon
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "System call 'kill' failed error=", errno);
    }
  }
  else
  {
    ret = eIasAvbProcErr;
    DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "Process ID for PTP daemon is not set!");
  }
  return ret;
}

uint64_t IasLibPtpDaemon::getRealLocalTime(const bool force)
{
  uint64_t ret;
  uint64_t lt = 0u;
  uint64_t tsc1 = 0u;
  uint64_t tsc2 = 0u;
  uint64_t raw1 = 0u;
  uint64_t raw2 = 0u;
  uint64_t localTimeForRaw = 0u;
  uint32_t attempt;

  // lower bound for smoothing filter coefficient
  static const double cSmoothBound = 0.1;

  // multiplicative step for changing the filter coefficient
  static const double cSmoothStep = 0.99;

  // acceptable factor deviation
  static const double cNormalFactorDeviation = 0.005;

  (void) mLastTimeMutex.lock();
  for (ret = mLastTime, attempt = 1u; ret == mLastTime; attempt++)
  {
    if (mClockId != -1)
    {
      if (eIasAvbProcOK != getIgbTime(lt, tsc1, cSysClockId))
      {
        // get ptp time with clock_gettime() as a fallback
        struct timespec tp;
        tsc1 = getTsc();
        int32_t r = clock_gettime(mClockId, &tp);
        tsc2 = getTsc();

        // calculate arithmetic mean of kernel time before and after reading I210 clock to reduce error
        tsc1 = (tsc1 >> 1) + (tsc2 >> 1);

        if (r < 0)
        {
          DLT_LOG_CXX(*mLog,  DLT_LOG_ERROR, LOG_PREFIX, "Failed to get time from PTP clock",
              mClockId, ": Error=", errno, strerror(errno));
        }
        else
        {
          lt = convertTimespecToNs(tp);
        }
      }
      else
      {
        // success
      }
    }
    else
    {
      ret = 0u;
    }

    if (0u != mLastTime)
    {
      int64_t delta = int64_t(lt - mLastTime);
      int64_t deltaTsc = tsc1 - mLastTsc;

      if (delta < 0)
      {
        /**
         * @log Negative local time change
         */
        DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "#", attempt,
            "negative local time change!", delta, 1e9f/mAvgDelta,
            "avg read access/sec) last:", mLastTime, "now:",
            lt);
        attempt++;
      }
      else
      {
        const int64_t phaseError = delta - int64_t(double(deltaTsc) * mTscToLocalFactor);
        // ignore phase error until smoothing has stabilized except if phase error is too high
        if (((mAvgCoeff <= cSmoothBound) && (llabs(phaseError) > cEpochChangeThreshold)) ||
                (llabs(phaseError) > (cEpochChangeThreshold * 10)))
        {
          mEpochCounter++;

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "#", attempt,
              "detected epoch change, phase error =", phaseError,
              "new epoch =", mEpochCounter, 1e9f/mAvgDelta,
              "avg read access/sec) last:", mLastTime, "now:", lt);

          mLastTime = 0u;
          ret = lt;
        }
        else
        {
          // success
          mAvgDelta = mAvgDelta * 0.9f + float(delta) * 0.1f;
          ret = lt;
        }
      }
    }
    else
    {
      ret = lt;
    }

    if ((ret == mLastTime) && (attempt >= 3u))
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "Unable to read reliable local time after",
          attempt-1u, "attempts (", 1e9f / mAvgDelta, "avg read access/sec)");
      DLT_LOG_CXX(*mLog,  DLT_LOG_INFO, LOG_PREFIX, "assuming ptp epoch change");
      mEpochCounter++;
      mLastTime = 0u;
      ret = 0u;
    }
  } // for

  // if mLastTime is not valid, (re)initialize conversion coeffs to 1.0 and "last time" members
  if (0u == mLastTime)
  {
    mLastTime = ret;
    mLastTsc = tsc1;
    mAvgCoeff = 1.0;
    mTscToLocalFactor = 1.0;

    if (mRawXtstampEn)
    {
      // raw specific code
      if (eIasAvbProcOK != getIgbTime(localTimeForRaw, raw1, cRawClockId))
      {
        // fall-back for only init phase
        struct timespec tp;
        raw1 = getRaw();
        clock_gettime(mClockId, &tp);
        raw2 = getRaw();
        raw1 = (raw1 >> 1) + (raw2 >> 1);
        localTimeForRaw = convertTimespecToNs(tp);
      }
      mLastLocalTimeforRaw = localTimeForRaw;
      mLastRaw = raw1;
      mRawAvgCoeff = 1.0;
      mRawToLocalFactor = 1.0;
      mRawToLocalFactors.clear();
    }
  }
  // otherwise update conversion coefficients only if enough time exceeded
  else if ((force) || ((lt - mLastTime) > cUpdateThreshold))
  {
    double newFactor = double(int64_t(lt - mLastTime)) / double(int64_t(tsc1 - mLastTsc));
    double factorDelta = (newFactor > mTscToLocalFactor) ?
      (newFactor - mTscToLocalFactor) : (mTscToLocalFactor - newFactor);

    static const double tolerance = 0.5;
    if ((newFactor >= 1.0/(1.0 + tolerance)) && (newFactor <= (1.0 + tolerance)))
    {
      // use variable smoothing to stabilize value
      mTscToLocalFactor = mAvgCoeff * newFactor + (1.0 - mAvgCoeff) * mTscToLocalFactor;
      if (mAvgCoeff > cSmoothBound)
      {
        mAvgCoeff *= cSmoothStep;
      }

      DLT_LOG_CXX(*mLog, (factorDelta > cNormalFactorDeviation) ? DLT_LOG_WARN : DLT_LOG_DEBUG, LOG_PREFIX,
          "coeff update: ",
          "lt", lt,
          "tsc", tsc1,
          "dt", int64_t(lt - mLastTime),
          "dtsc", int64_t(tsc1 - mLastTsc),
          "newFactor", newFactor,
          "avg", mTscToLocalFactor,
          "ac", mAvgCoeff
          );

    }
    else
    {
      DLT_LOG_CXX(*mLog,  DLT_LOG_WARN, LOG_PREFIX, "time update: new factor out of range:", newFactor);
    }

    mLastTime = ret;
    mLastTsc = tsc1;

    if (mRawXtstampEn)
    {
      // raw specific code
      if (eIasAvbProcOK != getIgbTime(localTimeForRaw, raw1, cRawClockId))
      {
        // unable to get precision cross time-stamps, free wheel with current factors
      }
      else
      {
        double newRawFactor = double(int64_t(localTimeForRaw - mLastLocalTimeforRaw)) / double(int64_t(raw1 - mLastRaw));
        double factorRawDelta = (newRawFactor > mRawToLocalFactor) ? (newRawFactor - mRawToLocalFactor)
                                                                    : (mRawToLocalFactor - newRawFactor);
        DltLogLevelType loglevel = (factorRawDelta > mRawNormalFactorDeviation) ? DLT_LOG_WARN : DLT_LOG_DEBUG;

        std::ostringstream strNewRawFactor;
        std::ostringstream strAvbRawFactor;
        strNewRawFactor << std::setprecision(7) << std::setiosflags(std::ios::fixed) << newRawFactor;

        bool doUpdate = true;
        bool hardRawDeviationCheckEn = (mRawNormalFactorDeviation != cNormalFactorDeviation) ? true : false;
        static const uint64_t cRawClockUnlockCountMax = (eRawXtstampImplRev2 == mRawXtstampEn) ? (uint64_t(1e9) / cUpdateThreshold) * 10
                                                                            : (uint64_t(1e9) / cUpdateThreshold) * 5;  // 1s/125ms = 8 cycles * 5

        if (hardRawDeviationCheckEn)
        {
          if (mRawAvgCoeff <= cSmoothBound)
          {
            // rate ratio value should not drastically change once smoothing has stabilized enough
            if (mRawNormalFactorDeviation < fabs(factorRawDelta))
            {
              // rate ratio delta exceeded the threshold, it's likely caused by the precision constraint of cross-timestamps
              if (++mDiag.rawXUnlockCount < cRawClockUnlockCountMax)
              {
                // skip rate ratio update since the derived value is not reliable
                doUpdate = false;
              }
              else
              {
                // but accept the new ratio for fail-safe if the deviation lasted longer than 5 seconds
                mDiag.rawXUnlockCount = 0;

                if (eRawXtstampImplRev2 == mRawXtstampEn)
                {
                  strAvbRawFactor << std::setprecision(7) << std::setiosflags(std::ios::fixed) << mRawToLocalFactor;

                  DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "reset factor:",
                              "newFactor", strNewRawFactor.str(), "avg", strAvbRawFactor.str());
                  mRawAvgCoeff = 1.0;
                  mRawToLocalFactor = 1.0;
                  mRawToLocalFactors.clear();
                }
              }
            }
          }
          else
          {
            // lower dlt log level while smoothing has not stabilized yet since rate ratio would change over the threshold
            loglevel = DLT_LOG_DEBUG;
          }
        }

        if ((doUpdate) && (newRawFactor >= 1.0/(1.0 + tolerance)) && (newRawFactor <= (1.0 + tolerance)))
        {
          mRawToLocalFactor = mRawAvgCoeff * newRawFactor + (1.0 - mRawAvgCoeff) * mRawToLocalFactor;

          if ((mRawAvgCoeff > cSmoothBound) && (eRawXtstampImplRev2 == mRawXtstampEn))
          {
            if (uint32_t(mRawToLocalFactors.size()) < cRawInitFactorSampleCount)
            {
              mRawToLocalFactors.push_back(newRawFactor);
              const uint32_t numFactors = uint32_t(mRawToLocalFactors.size());
              if (numFactors == cRawInitFactorSampleCount)
              {
                /*
                 * collect cRawInitFactorSampleCount of factors and use the median as the initial reliable factor value
                 * afterwards restrict the bound of variable in order to stabilize outgoing AAF/CRF timestamp intervals.
                 */
                std::sort(mRawToLocalFactors.begin(), mRawToLocalFactors.end());
                mRawToLocalFactor = (mRawToLocalFactors[numFactors/2 - 1] + mRawToLocalFactors[numFactors/2]) / 2.0;
                mRawAvgCoeff = cSmoothBound;

                strAvbRawFactor.clear();
                strAvbRawFactor << std::setprecision(7) << std::setiosflags(std::ios::fixed) << mRawToLocalFactor;

                DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "raw rate ratio smoothing stabilized:", strAvbRawFactor.str());
              }
            }
          }

          strAvbRawFactor.clear();
          strAvbRawFactor << std::setprecision(7) << std::setiosflags(std::ios::fixed) << mRawToLocalFactor;

          if (mRawAvgCoeff > cSmoothBound)
          {
            mRawAvgCoeff *= cSmoothStep;

            if ((hardRawDeviationCheckEn) && (mRawAvgCoeff <= cSmoothBound))
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "raw rate ratio smoothing stabilized:", strAvbRawFactor.str());
            }
          }

          DLT_LOG_CXX(*mLog, loglevel, LOG_PREFIX,
              "coeff update raw: ",
              "lt", localTimeForRaw,
              "raw", raw1,
              "dt", int64_t(localTimeForRaw - mLastLocalTimeforRaw),
              "draw", int64_t(raw1 - mLastRaw),
              "newFactor", strNewRawFactor.str(),
              "avg", strAvbRawFactor.str(),
              "ac", mRawAvgCoeff
              );

          if (hardRawDeviationCheckEn)
          {
            mLastLocalTimeforRaw = localTimeForRaw;
            mLastRaw = raw1;
            mDiag.rawXUnlockCount = 0;
          }
        }
        else
        {
          loglevel = DLT_LOG_INFO;
          if (hardRawDeviationCheckEn)
          {
            /*
             * suppress the duplicated info message because if deviation could not be resolved
             * until the next update timing, we can know it with the "coeff update raw:" message.
             */
            loglevel = DLT_LOG_DEBUG;
          }

          DLT_LOG_CXX(*mLog, loglevel, LOG_PREFIX, "raw time update: new factor out of range:", strNewRawFactor.str());
        }

        if (!hardRawDeviationCheckEn)
        {
          /*
           * Basically we should not update time values here if the calculated rate ratio is not reliable.
           * But to keep the default behavior without setting the '-k clockdomain.raw.deviation.unlock'
           * same as the former code as much as possible, we update time values here by default to avoid
           * causing regressions to a certain user who has started evaluation with the former code.
           */
          mLastLocalTimeforRaw = localTimeForRaw;
          mLastRaw = raw1;
        }
      }
    }
  }
  else
  {
    // do nothing, wait until cUpdateThreashold is reached
  }

  (void) mLastTimeMutex.unlock();

  return ret;
}

IasAvbProcessingResult IasLibPtpDaemon::getIgbTime(uint64_t &ptpTime, uint64_t &sysTime, const clockid_t clockId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  uint64_t sysTimeMeasurementInterval = 0u;
  uint64_t sysTimeMeasurementIntervalMin = std::numeric_limits<uint64_t>::max();
  const uint64_t cXtstampThreshold = (cSysClockId == clockId) ? mSysTimeMeasurementThreshold : mRawToLocalTstampThreshold;

  ptpTime = 0u;
  sysTime = 0u;

  for (uint64_t i = 0u; i < mMaxCrossTimestampSamples; i++)
  {
    /*
     * The value of cMaxCrossTimestampSamples is 3 by default.
     *
     * This method gets the ptp/monotonic cross-timestamp maximum 3 times then returns the most accurate one.
     * More iteration could provide higher accuracy. But since it has to lock the IGB device to access the registers
     * and the lock might block the TX sequencer which calls igb_xmit(), the number of iteration should be limited.
     *
     * It took one third cpu time to read the I210 clock from the registers compared to the general approach
     * using the clock_gettime(). Then we may do the iteration at least 3 times to get better accuracy without
     * increasing cpu load. (approx time needed on MRB: direct register access 3 us, clock_gettime 10 us)
     */

    if ((NULL == mIgbDevice) || (0 != igb_lock(mIgbDevice)) ||
        ((cSysClockId != clockId) && (cRawClockId != clockId)) )
    {
      result = eIasAvbProcErr;
      break;
    }
    else
    {
      uint64_t sys1 = 0u;
      uint64_t sys2 = 0u;

      uint32_t tsauxcReg = 0u;
      uint32_t stmph0Reg = 0u;
      uint32_t stmpl0Reg = 0u;

      (void) igb_readreg(mIgbDevice, TSAUXC, &tsauxcReg);
      tsauxcReg |= TSAUXC_SAMP_AUTO;

      // clear the value stored in AUXSTMPH/L0
      (void) igb_readreg(mIgbDevice, AUXSTMPH0, &stmph0Reg);

      sys1 = (cSysClockId == clockId) ? getTsc() : getRaw();

      // set the SAMP_AUT0 flag to latch the SYSTIML/H registers
      (void) igb_writereg(mIgbDevice, TSAUXC, tsauxcReg);

      // memory fence to avoid reading the registers before writing the SAMP_AUT0 flag
      __asm__ __volatile__("mfence;"
                  :
                  :
                  : "memory");

      sys2 = (cSysClockId == clockId) ? getTsc() : getRaw();

      // read the stored values
      (void) igb_readreg(mIgbDevice, AUXSTMPH0, &stmph0Reg);
      (void) igb_readreg(mIgbDevice, AUXSTMPL0, &stmpl0Reg);

      (void) igb_unlock(mIgbDevice);

      sysTimeMeasurementInterval = sys2 - sys1;
      if (sysTimeMeasurementInterval < sysTimeMeasurementIntervalMin)
      {
        sysTime = (sys1 >> 1) + (sys2 >> 1);
        ptpTime = stmph0Reg * uint64_t(1000000000u) + stmpl0Reg;
        sysTimeMeasurementIntervalMin = sysTimeMeasurementInterval;

        if (sysTimeMeasurementIntervalMin <= cXtstampThreshold)
        {
          // immediately exit the loop once we get cross-timestamp with the target accuracy
          break;
        }
      }
    }
  }

  if (cRawClockId == clockId)
  {
    mDiag.rawXCount++;
    if (cXtstampThreshold < sysTimeMeasurementIntervalMin)
    {
      mDiag.rawXFail++;
      result = eIasAvbProcErr;
    }

    // statistics for analysis
    if (mDiag.rawXMaxInt < sysTimeMeasurementInterval)
    {
      mDiag.rawXMaxInt = sysTimeMeasurementInterval;
    }
    if ((0u == mDiag.rawXMinInt) || (sysTimeMeasurementInterval < mDiag.rawXMinInt))
    {
      mDiag.rawXMinInt = sysTimeMeasurementInterval;
    }
    mDiag.rawXTotalInt += sysTimeMeasurementInterval;

    const double rawXSuccessRate = double(mDiag.rawXCount - mDiag.rawXFail)/double(mDiag.rawXCount);
    const double rawXAvgInterval = double(mDiag.rawXTotalInt) / double(mDiag.rawXCount);
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "raw-x-tstamp diag: success rate avg =",
                rawXSuccessRate, "interval avg =", rawXAvgInterval, "max =", mDiag.rawXMaxInt, "min =", mDiag.rawXMinInt);
  }

  return result;
}

IasAvbProcessingResult IasLibPtpDaemon::detectTscFreq(void)
{
  IasAvbProcessingResult result = eIasAvbProcErr;

  uint32_t eax = 0u;
  uint32_t ebx = 0u;
  uint32_t ecx = 0u;
  uint32_t edx = 0u;

  uint32_t family = 0u;
  uint32_t model  = 0u;
  uint32_t crystalFreq = 0u;

  const uint8_t cCpuModelAtomGoldmont = 0x5C; // Apollo Lake

  // EAX=1H CPU model
  asm volatile
      ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
       : "0" (1), "2" (0));

  family = (eax >> 8) & 0xF;
  model = eax >> 4 & 0xF;

  if((0x6 == family) || (0xF == family))
  {
    model |= ((eax >> 16) & 0xF) << 4;
  }

  switch(model)
  {
    case cCpuModelAtomGoldmont:
      crystalFreq = 19200000; // 19.2 MHz
      break;
    default:
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "raw-x-tstamp: unsupported CPU model number", model);
      break;
  }

  if ((0u != crystalFreq) && (0x15 <= eax)) // cpuid level
  {
    // EAX=15H TSC/ART ratio
    asm volatile
        ("cpuid" : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
         : "0" (0x15), "2" (0));

    if (0u != eax)
    {
      // TSC_Value = (ART_Value * CPUID.15H:EBX[31:0])/CPUID.15H:EAX[31:0] + K; (K is ignored here)
      mTscFreq = ((uint64_t)crystalFreq * ebx) / eax;
      mTscFreq /= 1000; // convert to kHz
      if (0u != mTscFreq)
      {
        result = eIasAvbProcOK;
      }
    }
  }

  return result;
}


} // namespace IasMediaTransportAvb

