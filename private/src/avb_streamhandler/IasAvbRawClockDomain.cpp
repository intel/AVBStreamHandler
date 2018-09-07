/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 * @brief   The definition of the IasAvbRawClockDomain class.
 * @date    2016
 */

#include <time.h> // make sure we're using the correct struct timespec definition
#include "avb_streamhandler/IasAvbRawClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include <dlt/dlt_cpp_extension.hpp>


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbRawClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbRawClockDomain::IasAvbRawClockDomain()
  : IasAvbClockDomain(IasAvbStreamHandlerEnvironment::getDltContext("_MCD"), eIasAvbClockDomainRaw)
  , mInstanceName("IasAvbRawClockDomain")
  , mStartTime(0u)
  , mLastUpdate(0u)
  , mLastPtp(0u)
  , mLastRaw(0u)
  , mRawClockDomainLock()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  setEventRate(48000u);
  setInitialValue( 1.0 );
  setLockThreshold1( 100000u );
  setLockThreshold2( 100000u );
  updateRateRatio( 1.0 ); // initialize
  updateRateRatio( 1.0 ); // lock

  /* NOTE: clamping the rate ratio at 1.0 is actually wrong, but since the rate ratio
   * is not really used anymore, we should be able to get away with it.
   */

  AVB_ASSERT( eIasAvbLockStateLocked == getLockState() )
}


/*
 *  Destructor.
 */
IasAvbRawClockDomain::~IasAvbRawClockDomain()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

/**
 * This function simulates how a 48kHz clock advances synchronously to CLOCK_MONOTONIC_RAW time
 */
void IasAvbRawClockDomain::onGetEventCount()
{
  static const uint64_t cGranularity = 125000u; //ns, should be evenly dividable by 48kHz period length

  mRawClockDomainLock.lock();

  IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
  // do not assert ptp != NULL since it might be in some unit tests
  if (NULL != ptp)
  {
    const uint64_t rawNow = ptp->getRaw();
    const uint64_t remain = (rawNow % cGranularity);

    if (0u == mStartTime)
    {
      mStartTime = rawNow - remain;
      ptp->getRealLocalTime(true); // update rate ratio before the first call of rawToPtp().
    }

    if ((rawNow - mLastUpdate) > cGranularity)
    {
      mLastUpdate = rawNow - remain;
      const uint64_t events = (mLastUpdate - mStartTime) / 62500 * 3; // *48kHz/1e9ns
      const uint64_t ptpTime = ptp->rawToPtp(mLastUpdate);
      const double ratio = ((0 == mLastRaw) || (0 == mLastPtp)) ? 1.0 : double(ptp->rawToPtp(rawNow) - mLastPtp) / double(rawNow - mLastRaw);
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "events", events, "ptpTime=", ptpTime % 0x100000000, "lastUpdate=", mLastUpdate % 0x100000000, "ratio2=", ratio);
      setEventCount(events, ptpTime);
      updateRateRatio(ratio);
    }
    mLastRaw = rawNow;
    mLastPtp = ptp->rawToPtp(rawNow);
  }

  mRawClockDomainLock.unlock();
}


} // namespace IasMediaTransportAvb
