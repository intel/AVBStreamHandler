/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbPtpClockDomain.cpp
 * @brief   The definition of the IasAvbPtpClockDomain class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include <dlt/dlt_cpp_extension.hpp>


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbPtpClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbPtpClockDomain::IasAvbPtpClockDomain()
  : IasAvbClockDomain(IasAvbStreamHandlerEnvironment::getDltContext("_PCD"), eIasAvbClockDomainPtp)
  , mInstanceName("IasAvbPtpClockDomain")
  , mStartTime(0u)
  , mLastUpdate(0u)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  setEventRate(48000u);
  setInitialValue( 1.0 );
  setLockThreshold1( 100000u );
  setLockThreshold2( 100000u );
  updateRateRatio( 1.0 ); // initialize
  updateRateRatio( 1.0 ); // lock

  AVB_ASSERT( eIasAvbLockStateLocked == getLockState() )
}


/*
 *  Destructor.
 */
IasAvbPtpClockDomain::~IasAvbPtpClockDomain()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

/**
 * This function simulates how a 48kHz clock advances synchronously to PTP time
 */
void IasAvbPtpClockDomain::onGetEventCount()
{
  static const uint64_t cGranularity = 125000u; //ns, should be evenly dividable by 48kHz period length
  IasLibPtpDaemon* ptp = IasAvbStreamHandlerEnvironment::getPtpProxy();
  // do not assert ptp != NULL since it might be in some unit tests
  if (NULL != ptp)
  {
    const uint64_t now = ptp->getPtpTime();
    if (0u == mStartTime)
    {
      mStartTime = now - (now % cGranularity);
    }

    if ((now - mLastUpdate) > cGranularity)
    {
      mLastUpdate = now - (now % cGranularity);
      const uint64_t events = (mLastUpdate - mStartTime) / 62500 * 3; // *48kHz/1e9ns
      setEventCount(events, mLastUpdate);
    }
  }
}


} // namespace IasMediaTransportAvb
