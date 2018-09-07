/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLibPtPDaemon.hpp
 * @brief   Library class for the PTP daemon.
 * @date    2013
 */


#ifndef IASLIBPTPDAEMON_HPP_
#define IASLIBPTPDAEMON_HPP_

extern "C"
{
  #include "igb.h"
}

#include "avb_streamhandler/IasAvbTypes.hpp" // internal AVB type declarations

#include <stdint.h>    // just make sure that types used in ipcdef.hpp are known
#include <sys/types.h>
#include <time.h>

#include "ipcdef.hpp"
#include "linux_ipc.hpp"

#include <mutex>
#include <dlt.h>

namespace IasMediaTransportAvb {

/**
 * @class IasLibPtpDaemon
 * @brief This class is the implementation of the PTP daemon library.
 *
 * This library class can be used to access the PTP daemon to obtain
 * PTP related time information.
 */
class IasLibPtpDaemon
{
  public:

    /**
     * @brief clock ID to be used whenever dealing with the local system time
     *
     * Note that, unfortunately,  CLOCK_MONOTONIC_RAW cannot be used
     * as it is not supported by some functions (e.g. clock_nanosleep)
     */
    static const clockid_t cSysClockId = CLOCK_MONOTONIC;
    static const clockid_t cRawClockId = CLOCK_MONOTONIC_RAW;

    /**
     *  @brief Constructor.
     *
     *  @param[in] sharedMemoryName   Name of shared memory to access
     *  @param[in] sharedMemorySize   Size of shared memory
     */
    IasLibPtpDaemon(std::string const & sharedMemoryName, uint32_t const & sharedMemorySize);

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasLibPtpDaemon();

    /**
     *  @brief Allocate resources.
     *
     */
    IasAvbProcessingResult init();

    /**
     *  @brief Allocate resources.
     *
     * Convenience function to remain compatible with previous usage - to be removed in future versions
     */
    IasAvbProcessingResult init(void* dummy)
    {
      (void) dummy;
      return init();
    }

    /**
     *  @brief Cleanup allocated resources.
     */
    void cleanUp();

    /**
     * @brief Returns the high-precision CPU clock counter as 64 bit value.
     * This function returns the monotonic clock of the Linux kernel, which on x86
     * systems should be the TSC clock of the boot core. The ptp daemon is needed
     * to convert the counter into something that is meaningful to the application.
     *
     * @return TSC time value.
     */
    static inline uint64_t getTsc();

    /**
     * @brief Returns the high-precision CPU clock counter as 64 bit value.
     * This function returns the monotonic raw clock of the Linux kernel, which on x86
     * systems should be the TSC clock of the boot core. The ptp daemon is needed
     * to convert the counter into something that is meaningful to the application.
     *
     * @return raw clock time value.
     */
    inline uint64_t getRaw();

    /**
     * @brief Converts timestamp in timespec representation into nanoseconds value
     * @param[in] tp to convert
     *
     * @return time in nanoseconds
     */
    static inline uint64_t convertTimespecToNs(const struct timespec &tp);

    /**
     * @brief Converts nanoseconds value into timespec representation
     * @param[in] value to convert
     * @param[out] tp result
     */
    static inline void convertNsToTimespec(uint64_t ns, struct timespec &tp);


    /**
     * @brief Returns the local timer value.
     *
     * The value is extrapolated using tsc until a time of cUpdateThreshold has elapsed.
     * Then, the real local time from I210 is read.
     *
     * @return Local  timer value.
     */
    uint64_t getLocalTime();

    /**
     * @brief Returns the local Springville timer value.
     *
     * @return Local Springville timer value.
     */
    uint64_t getRealLocalTime(const bool force = false);

    /**
     * @brief Returns the 802.1AS timer value.
     *
     * Note: the current implementation assumes this to be identical with the local time!
     *
     * @return timer value.
     */
    inline uint64_t getPtpTime();

    /**
     * @brief converts time stamp in system time to PTP time stamp
     *
     * "system time" refers to the clock whose ID can be queried with getSysClockId()
     *
     * @param[in] sysTime system time stamp
     * @return time stamp converted from system time to PTP time
     */
    uint64_t sysToPtp(const uint64_t sysTime) const;

    /**
     * @brief converts time stamp in system time (clock monotonic raw) to PTP time stamp
     *
     * @param[in] sysTime system time stamp (raw)
     * @return time stamp converted from system time to PTP time
     */
    uint64_t rawToPtp(const uint64_t sysTime) const;

    /**
     * @brief converts time stamp in PTP time to system time
     *
     * "system time" refers to the clock whose ID can be queried with getSysClockId()
     *
     * @param[in] ptpTime PTP time stamp
     * @return time stamp converted from PTP time to system time
     */
    uint64_t ptpToSys(const uint64_t ptpTime) const;

    /**
     * @brief Returns if Ptp daemon is stable for transmitting packets.
     */
    bool isPtpReady();

    /**
     * @brief Returns the epoch counter
     *
     * Every time the PTP proxy detects a deviation between extrapolated time
     * and updated Springville time that exceeds a certain threshold, the epoch
     * of the Springville clock is assumed to have changed (i.e. the PTP daemon
     * has done a phase adjustment on the clock). Then, the epoch counter is
     * incremented. The application should monitor the counter and shoudl restart
     * any flows that are based on the epoch.
     *
     * @return epoch counter
     */
    inline uint32_t getEpochCounter() const;

    /**
     * @brief Sends a hang-up signal to the PTP daemon to signal that it should
     * store its persistence data now.
     *
     * @return eIasAvbProcOK on success, otherwise an error is returned
     */
    IasAvbProcessingResult triggerStorePersistenceData() const;

    /**
     * @brief returns the clock ID that is used to get the local system time
     */
    inline clockid_t getSysClockId() const;

    /**
     * @brief Returns the real tsc tick value.
     *
     * @return rdtsc value.
     */
    inline uint64_t getRealTsc() const;

  private:

    struct Diag
    {
      Diag();
      uint64_t rawXCount;
      uint64_t rawXFail;
      uint64_t rawXMaxInt;
      uint64_t rawXMinInt;
      uint64_t rawXTotalInt;
      uint64_t rawXunlock;
      uint64_t rawXUnlockCount;
    };

    enum RawXtstampImplRev
    {
      eRawXtstampImplDisable, // non-raw mode
      eRawXtstampImplRev1,
      eRawXtstampImplRev2,
      eRawXtstampImplRevInvalid
    };

    /**
     * @brief deviation threshold for detecting an epoch change
     */
    static const int64_t cEpochChangeThreshold = 2000000u; // 1ms

    /**
     * @brief refresh time for tsc->local conversion coefficients in ns
     */
    static const uint64_t cUpdateThreshold = 125000000u;

    /**
     * @brief maximum measurement samples of system and ptp time values
     */
    static const uint64_t cMaxCrossTimestampSamples = 3u;

    /**
     * @brief target value for system time measurement interval in ns
     */
    static const uint64_t cSysTimeMeasurementThreshold = 3000u;

    /**
     * @brief target value for raw time measurement interval in ns
     */
    static const uint64_t cRawTimeMeasurementThreshold = 3000u;

    /**
     * @brief number of factor samples to use estimate the initial reliable factor value
     */
    static const uint64_t cRawInitFactorSampleCount = 20u;

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasLibPtpDaemon(IasLibPtpDaemon const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasLibPtpDaemon& operator=(IasLibPtpDaemon const &other); //lint !e1704


    /**
     * @brief get ptp device path from network interface name
     */
    void getPtpDevice(std::string & path);

    /**
     * @brief calibrate the conversion coeffs for TSC to local
     *
     * @returns eIasAvbProcOK on success, error code otherwise
     */
    IasAvbProcessingResult calculateConversionCoeffs();

    /**
     * @brief returns the I210 clock read from its registers and monotonic clock
     *
     * This method is an alternative to igb_gettime(). Since the IGB API does
     * iteration up to 32 times to get cross timestamp, which is too many for us.
     * Even worse it holds the lock of the IGB device during the entire iteration.
     * This method offers a lightweight similar functionality.
     */
    IasAvbProcessingResult getIgbTime(uint64_t &ptpTime, uint64_t &sysTime, const clockid_t clockId);

    /**
     * @brief detect tsc frequency from cpu model
     *
     * @return eIasAvbProcOK on success, error code otherwise
     */
    IasAvbProcessingResult detectTscFreq(void);

    ///
    /// Member Variables
    ///

    std::string const     mInstanceName;
    bool                  mInitialized;
    int32_t               mSharedMemoryFd;
    uint8_t*              mMemoryOffsetBuffer;
    std::string const     mSharedMemoryName;
    uint32_t const        mSharedMemorySize;
    double                mTscToLocalFactor;
    double                mRawToLocalFactor;
    double                mRawNormalFactorDeviation;
    double                mAvgCoeff;
    double                mRawAvgCoeff;
    uint64_t              mLastTime;
    uint64_t              mLastLocalTimeforRaw;
    uint64_t              mLastTsc;
    uint64_t              mLastRaw;
    uint64_t              mLastLastTime;
    uint64_t              mLastLastTsc;
    uint64_t              mLastLastRaw;
    uint32_t              mEpochCounter;
    int32_t               mClockHandle;
    clockid_t             mClockId;
    float               mAvgDelta;
    mutable std::mutex    mLastTimeMutex;
    pid_t                 mProcessId; // process id of ptp daemon
    DltContext            *mLog;

    device_t              *mIgbDevice;
    bool                  mLocalTimeUpdating;
    uint64_t              mMaxCrossTimestampSamples;
    uint64_t              mSysTimeMeasurementThreshold;
    uint64_t              mRawXtstampEn;
    Diag                  mDiag;
    uint64_t                mTscEpoch;
    uint64_t                mTscFreq;
    uint64_t                mRawToLocalTstampThreshold;
    std::vector<double>  mRawToLocalFactors;
}; // class IasLibPtpDaemon

inline uint64_t IasLibPtpDaemon::getTsc()
{
  struct timespec tp;
  (void) clock_gettime(cSysClockId, &tp);
  return convertTimespecToNs(tp);
}

inline uint64_t IasLibPtpDaemon::getRaw()
{
  /*
   * cRawEpochNs is dummy epoch time in nanoseconds to provide non-zero master time at start-up.
   * AVB audio stream recognizes zero master time as an unavailable master clock.
   */
  static const uint64_t cRawEpochNs = 125000u;

  const uint64_t now = getRealTsc();

  if (0u == mTscEpoch)
  {
    mTscEpoch = now;
  }

  const uint64_t raw = uint64_t(double(now - mTscEpoch) * (1e6 / double(mTscFreq))) + cRawEpochNs;

  return raw;
}

inline uint64_t IasLibPtpDaemon::convertTimespecToNs(const struct timespec &tp)
{
    return (uint64_t(int64_t(tp.tv_sec)) * uint64_t(1000000000u)) +
            uint64_t(int64_t(tp.tv_nsec));
}

inline void IasLibPtpDaemon::convertNsToTimespec(uint64_t ns, struct timespec &tp)
{
  const uint64_t cBillion = 1e9;
  const uint64_t sec = ns / cBillion;
  tp.tv_sec = time_t(int64_t(sec));
  tp.tv_nsec = __syscall_slong_t(int64_t(ns - (sec * cBillion)));
}

inline uint32_t IasLibPtpDaemon::getEpochCounter() const
{
  return mEpochCounter;
}

inline uint64_t IasLibPtpDaemon::getPtpTime()
{
  return getLocalTime();
}

inline uint64_t IasLibPtpDaemon::getRealTsc() const
{
  uint32_t low = 0u;
  uint32_t high = 0u;

  __asm__ __volatile__ ("rdtsc" : "=a"(low), "=d"(high));

  return (uint64_t(high) << 32) | uint64_t(low);
}



}  // namespace IasMediaTransport

#endif /* IASLIBPTPDAEMON_HPP_ */
