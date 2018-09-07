/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbSwClockDomain.cpp
 * @brief   This is the implementation of the IasAvbSwClockDomain class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbSwClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include <dlt/dlt_cpp_extension.hpp>


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbSwClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbSwClockDomain::IasAvbSwClockDomain()
  : IasAvbClockDomain(IasAvbStreamHandlerEnvironment::getDltContext("_SCD"), eIasAvbClockDomainSw )
  , mInstanceName("IasAvbSwClockDomain")
  , mResetPending( true )
  , mLastTsc( 0u )
  , mPtpProxy( IasAvbStreamHandlerEnvironment::getPtpProxy() )
  , mFactorLong(50.0)
  , mFactorUnlock(0.5)
  , mThreshold1(1000000u)
  , mThreshold2(100u)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  uint64_t val = 20000u;
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkSwTimeConstant, val);
  // pass time constant to base class for later use
  setFilter(double(val) * 0.001, 1u);

  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkSwDeviationLongterm, val))
  {
    mFactorLong = double(val) * 0.001;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkSwDeviationUnlock, val))
  {
    mFactorUnlock = double(val) * 0.001;
  }
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkSwLockTreshold1, mThreshold1);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkSwLockTreshold2, mThreshold2);
}

/*
 *  Destructor.
 */
IasAvbSwClockDomain::~IasAvbSwClockDomain()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

void IasAvbSwClockDomain::advance(uint64_t events, const uint32_t elapsed, uint32_t elapsedTSC)
{
  AVB_ASSERT(NULL != mPtpProxy);

  if (mResetPending)
  {
    mResetPending = false;

    setEventCount(0u, mPtpProxy->getLocalTime());
  }
  else
  {
    incrementEventCount(events, mPtpProxy->getLocalTime());
#if IASAVB_USE_TSC
    AVB_ASSERT( NULL != mPtpProxy );
    elapsedTSC *= mPtpProxy->getTscTo1AsFactor();
#endif
    updateRateRatio( double(elapsedTSC) / double(elapsed) );
  }
}


void IasAvbSwClockDomain::reset(uint32_t avgCallsPerSec)
{
  mResetPending = true;
  setFilter(getTimeConstant(), avgCallsPerSec);
  setDerivationFactors(mFactorLong, mFactorUnlock);
  setLockThreshold1(mThreshold1);
  setLockThreshold2(mThreshold2);
}


} // namespace IasMediaTransportAvb
