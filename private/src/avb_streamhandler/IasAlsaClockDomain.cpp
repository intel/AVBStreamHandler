/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 * @brief   The definition of the IasAlsaClockDomain class.
 * @date    2018
 */

#include <time.h> // make sure we're using the correct struct timespec definition
#include "avb_streamhandler/IasAlsaClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include <dlt/dlt_cpp_extension.hpp>

//ABu: ToDo: This class is not used so far - check if needed and how timing must be calculated - at the moment this is just a copy of RawClockDomain
namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAlsaClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAlsaClockDomain::IasAlsaClockDomain()
  : IasAvbClockDomain(IasAvbStreamHandlerEnvironment::getDltContext("_MCD"), eIasAVbClockDomainAlsa)
  , mInstanceName("IasAlsaClockDomain")
  , mStartTime(0u)
  , mLastUpdate(0u)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  setEventRate(      48000u  );
  setInitialValue(   1.0     );
  setLockThreshold1( 100000u );
  setLockThreshold2( 100000u );
  updateRateRatio(   1.0     );     // initialize
  updateRateRatio(   1.0     );     // lock

  /* NOTE: clamping the rate ratio at 1.0 is actually wrong, but since the rate ratio
   * is not really used anymore, we should be able to get away with it.
   */

  AVB_ASSERT( eIasAvbLockStateLocked == getLockState() )
}


/*
 *  Destructor.
 */
IasAlsaClockDomain::~IasAlsaClockDomain()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

/**
 * This function simulates how a 48kHz clock advances synchronously to CLOCK_MONOTONIC_RAW time
 */
void IasAlsaClockDomain::onGetEventCount()
{
  // This is just copied from IASAvbRawClockDomain - it must be matched to Alsa clock!


//  static const uint64_t cGranularity = 125000u; //ns, should be evenly dividable by 48kHz period length
//
//  IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
//  // do not assert ptp != NULL since it might be in some unit tests
//  if (NULL != ptp)
//  {
//    /* The following is only a very rough cross-time-stamping between CLOCK_MONOTONIC_RAW and PTP.
//     * Any jitter should average out over time. However, if the quality turns out to be insufficient,
//     * the routine needs to be improved.
//     */
//    const uint64_t rawNow = getRawTime();
//    const uint64_t ptpNow = ptp->getLocalTime();
//    const uint64_t remain = (rawNow % cGranularity);
//
//    if (0u == mStartTime)
//    {
//      mStartTime = rawNow - remain;
//    }
//
//    if ((rawNow - mLastUpdate) > cGranularity)
//    {
//      mLastUpdate = rawNow - remain;
//      const uint64_t events = (mLastUpdate - mStartTime) / 62500 * 3; // *48kHz/1e9ns
//      /* we simplify by subtracting remain from PTP time, although, theoretically,
//       * we would need to convert it to the PTP time domain first.
//       */
//      setEventCount(events, ptpNow - remain);
//    }
//  }
}


} // namespace IasMediaTransportAvb
