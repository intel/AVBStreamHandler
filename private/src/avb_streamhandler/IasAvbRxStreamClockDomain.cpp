/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbRxStreamClockDomain.cpp
 * @brief   The implementation of the IasAvbRxStreamClockDomain class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include <dlt/dlt_cpp_extension.hpp>

/*
static const sven_guid_t guid ={ 0x8887160a, 0xc965, 0x463b, { 0x9f, 0x43, 0x1e, 0xfe, 0x9f, 0xdf, 0xe3, 0xf9 } };
*/

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbRxStreamClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbRxStreamClockDomain::IasAvbRxStreamClockDomain()
  : IasAvbClockDomain(IasAvbStreamHandlerEnvironment::getDltContext("_RCD"), eIasAvbClockDomainRx)
  , mInstanceName("IasAvbRxStreamClockDomain")
  , mPtpProxy(NULL)
  , mFactorLong(10.0)
  , mFactorUnlock(1.0)
  , mThreshold1(10000u)
  , mThreshold2(100u)
  , mLastTimeStamp(0u)
  , mEpoch(0u)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  uint64_t val = 100u;
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRxTimeConstant, val);
  // pass time constant to base class for later use
  setFilter(double(val) * 0.001, 1u);

  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRxDeviationLongterm, val))
  {
    mFactorLong = double(val) * 0.001;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRxDeviationUnlock, val))
  {
    mFactorUnlock = double(val) * 0.001;
  }
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRxLockTreshold1, mThreshold1);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClkRxLockTreshold2, mThreshold2);

  mPtpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
}


/*
 *  Destructor.
 */
IasAvbRxStreamClockDomain::~IasAvbRxStreamClockDomain()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

void IasAvbRxStreamClockDomain::reset(IasAvbSrClass cl, uint32_t timestamp, uint32_t eventRate)
{
  // calculate full 64 bit time stamp from lower 32 bits, then call the 64 bit variant of reset()
  initLastTimeStamp(timestamp);
  reset(cl, mLastTimeStamp, eventRate);
}

void IasAvbRxStreamClockDomain::reset(IasAvbSrClass cl, uint64_t timestamp, uint32_t eventRate)
{
  uint32_t skipTime = 0u;
  uint32_t callsPerSecond = IasAvbTSpec::getPacketsPerSecondByClass( cl );

  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime) && (0u != skipTime))
  {
    callsPerSecond = (1000000u + (skipTime - 1u)) / skipTime;
  }

  AVB_ASSERT(NULL != mPtpProxy);
  mEpoch = mPtpProxy->getEpochCounter();

  mLastTimeStamp = timestamp;

  setEventCount(0u, mLastTimeStamp);
  setEventRate(eventRate);

  setFilter(getTimeConstant(), callsPerSecond);
  setDerivationFactors(mFactorLong, mFactorUnlock);
  setLockThreshold1(mThreshold1);
  setLockThreshold2(mThreshold2);
  mPacketRate.reset(callsPerSecond);
}

void IasAvbRxStreamClockDomain::update(uint64_t events, uint32_t timestamp, uint32_t deltaMediaClock, uint32_t deltaWallClock)
{
  // expand 32-bit time stamp into 64 bits, then call the 64 bit version of update()
  AVB_ASSERT(NULL != mPtpProxy);
  uint32_t epoch = mPtpProxy->getEpochCounter();
  if (epoch != mEpoch)
  {
    mEpoch = epoch;
    deltaMediaClock = 0u;

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "new epoch: ", epoch,
        "- updating timestamp high part");

    initLastTimeStamp(timestamp);
  }
  else
  {
    if (uint32_t(mLastTimeStamp) < timestamp)
    {
      mLastTimeStamp = (mLastTimeStamp & uint64_t(0xFFFFFFFF00000000)) + timestamp;
    }
    else
    {
      mLastTimeStamp = ((mLastTimeStamp + uint64_t(0x100000000)) & uint64_t(0xFFFFFFFF00000000)) + timestamp;
    }
  }

  update(events, mLastTimeStamp, deltaMediaClock, uint64_t(deltaWallClock));
}

void IasAvbRxStreamClockDomain::update(uint64_t events, uint64_t timestamp, uint32_t deltaMediaClock, uint64_t deltaWallClock)
{
  incrementEventCount(events, timestamp);
  mLastTimeStamp = timestamp;

  if (deltaMediaClock > 0u)
  {
    updateRateRatio( double(deltaWallClock) / double(deltaMediaClock) );
    mPacketRate.advance(events, deltaMediaClock);
  }
}


void IasAvbRxStreamClockDomain::initLastTimeStamp(uint32_t timestamp)
{
  // figure out high DWORD of timestamp: ask ptp proxy for current time
  AVB_ASSERT(NULL != mPtpProxy);
  const uint64_t now = mPtpProxy->getRealLocalTime();

  /* NOTE: AVTP timestamp is lower 32 bit only - no epoch. Assume that AVTP timestamp is within +/- 2^31 nanoseconds
   * from current time.
   * If timestamp is ahead of current time, but 32 bit value is less than the one of current time,
   * a wrap in the upper 32 bits has occurred.
   */
  mLastTimeStamp = (now & uint64_t(0xFFFFFFFF00000000)) + timestamp;
  if ((int32_t(timestamp - uint32_t(now)) > 0) && (timestamp < uint32_t(now)))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " detected wrap");
    mLastTimeStamp += uint64_t(0x100000000);
  }

  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " now=", now,
        "ts=", timestamp,
        "mLastTimeStamp=", mLastTimeStamp);
}

void IasAvbRxStreamClockDomain::invalidate()
{
  // this will cause the base class to change to "unlocked" state
  setFilter(getTimeConstant(), 1);
}


//
// Implementation of inherited methods
//

} // namespace IasMediaTransportAvb
