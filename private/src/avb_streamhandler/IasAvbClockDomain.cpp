/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockDomain.cpp
 * @brief   The implementation of the IasAvbClockDomain class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include <cmath>
#include <dlt_cpp_extension.hpp>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbClockDomain::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasAvbClockDomain::IasAvbClockDomain(DltContext &dltContext, IasAvbClockDomainType type)
  : mType(type)
  , mTimeConstant(0.0)
  , mAvgCallsPerSec(1)
  , mRateRatio(1.0)
  , mCompensation(1.0)
  , mEventCount(0u)
  , mEventRate(0u)
  , mEventTimeStamp(0u)
  , mRateRatioSlow(1.0)
  , mRateRatioFast(1.0)
  , mCoeffSlowLocked(0.0)
  , mCoeffSlowUnlocked(0.0)
  , mCoeffFastLocked(0.0)
  , mCoeffFastUnlocked(0.0)
  , mThresholdSlowLow(0.0)
  , mThresholdSlowHigh(0.0)
  , mThresholdFastLow(0.0)
  , mThresholdFastHigh(0.0)
  , mInitialValue(1.0)
  , mDerivationFactorUnlock(1.0)
  , mDerivationFactorLongTerm(1.0)
  , mLockState(eIasAvbLockStateInit)
  , mLock()
  , mDebugCount(0u)
  , mDebugUnlockCount(0u)
  , mDebugLockedPercentage(1.0)
  , mDebugMinRatio(1.0/0.0)
  , mDebugMaxRatio(0.0)
  , mDebugOver(0u)
  , mDebugUnder(0u)
  , mDebugIn(0u)
  , mClient(NULL)
  , mDebugLogInterval(5u)
  , mResetRequest(false)
  , mClockId(-1)
  , mLog(&dltContext)
{
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cDebugClkDomainIntvl, mDebugLogInterval);
}


/*
 *  Destructor.
 */
IasAvbClockDomain::~IasAvbClockDomain()
{
  // do nothing
}


void IasAvbClockDomain::updateRateRatio(double newRatio)
{
  // sanity check, needed for ptp epoch changes
  if ((newRatio < 0.0) || (newRatio > 10.0))
  {
    return;
  }

  const bool locked1high = (newRatio < (mThresholdFastHigh * mRateRatioFast));
  const bool locked1low = (newRatio > (mThresholdFastLow * mRateRatioFast));

  const bool locked1 = locked1high && locked1low;

  if (eIasAvbLockStateLocked == mLockState)
  {
    smooth(mRateRatioSlow, newRatio, mCoeffSlowLocked);
    smooth(mRateRatioFast, newRatio, mCoeffFastLocked);
  }
  else
  {
    smooth(mRateRatioSlow, newRatio, mCoeffSlowUnlocked);
    smooth(mRateRatioFast, newRatio, mCoeffFastUnlocked);
  }

  mDebugMinRatio = newRatio < mDebugMinRatio ? newRatio : mDebugMinRatio;
  mDebugMaxRatio = newRatio > mDebugMaxRatio ? newRatio : mDebugMaxRatio;
  smooth( mDebugLockedPercentage, locked1 ? 1.0 : 0.0, mCoeffFastUnlocked );
  mDebugOver += locked1high ? 0 : 1;
  mDebugUnder += locked1low ? 0 : 1;
  mDebugIn += locked1 ? 1 : 0;

  const double rateRatioMax = mThresholdSlowHigh * mRateRatioSlow;
  const double rateRatioMin = mThresholdSlowLow * mRateRatioSlow;

  const bool locked2 = (mRateRatioFast < rateRatioMax) && (mRateRatioFast > rateRatioMin);

  if (mDebugCount++ > (mAvgCallsPerSec * mDebugLogInterval))
  {
    mDebugCount = 0u;

    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "clock[", int32_t(mType),
        "/", uint64_t(this), "]:", newRatio, mRateRatioFast, mRateRatioSlow, (locked1 ? "1" : "-"),
        (locked2 ? "2" : "-"), (mLockState == eIasAvbLockStateLocked ? "L" : "-"), mDebugUnlockCount);

    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "clock[", int32_t(mType),
        "/", uint64_t(this), "]: max", mDebugMaxRatio, "min", mDebugMinRatio, "locked1", mDebugLockedPercentage, mDebugOver,
        "/", mDebugIn, "/", mDebugUnder, float( mDebugOver ) / float( mDebugUnder ));

    mDebugMinRatio = 1.0/0.0;
    mDebugMaxRatio = 0.0;
  }

  if (NULL != mClient)
  {
    mClient->notifyUpdateRatio(this);
  }

  switch (mLockState)
  {
  case eIasAvbLockStateInit:
    mRateRatioSlow = mInitialValue;
    mRateRatioFast = mInitialValue;
    // fall-through

  case eIasAvbLockStateUnlocked:
    mLockState = eIasAvbLockStateLocking;
    // fall-through

  case eIasAvbLockStateLocking:
    if (locked1 && locked2)
    {
      mLockState = eIasAvbLockStateLocked;
      lockStateChanged();
    }
    break;

  case eIasAvbLockStateLocked:
    if (!locked2)
    {
      mLockState = eIasAvbLockStateUnlocked;
      lockStateChanged();
      mDebugUnlockCount++;
    }
    break;

  default:
    break;
  }

  mRateRatio = mRateRatioFast > rateRatioMax ? rateRatioMax : (mRateRatioFast < rateRatioMin ? rateRatioMin : mRateRatioFast);
  mRateRatio *= mCompensation;
}

IasAvbProcessingResult IasAvbClockDomain::setDriftCompensation(int32_t val)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  /*
   * The following is a simple linear approximation of pow(mRatioBendMax, -relFillLevel) within the required range
   */
  if ((val >= 0) && (val <= 1000000))
  {
    mCompensation = 1.0 / (1.0 + (double(val) * 1e-6));
  }
  else if ((val < 0) && (val >= -1000000))
  {
    mCompensation = 1.0 + (double(-val) * 1e-6);
  }
  else
  {
    // relFillLevel out of range
    ret = eIasAvbProcInvalidParam;
  }

  return ret;
}


void IasAvbClockDomain::lockStateChanged()
{
  if (NULL != mClient)
  {
    mClient->notifyUpdateLockState(this);
  }
  else
  {
    /**
     * @log Change in the clock domain lock state but the client is null, unable to notify.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "no client");
  }
}



void IasAvbClockDomain::setInitialValue(double initVal)
{
  if (initVal >= 0.0)
  {
    mInitialValue = initVal;
  }
}


void IasAvbClockDomain::setFilter(double timeConstant, uint32_t avgCallsPerSec)
{
  if (timeConstant >= 0.0)
  {
    mTimeConstant = timeConstant;
    mAvgCallsPerSec = avgCallsPerSec;
    const double tc = timeConstant * double(avgCallsPerSec);

    mCoeffFastLocked = calculateCoefficient(tc);
    mCoeffFastUnlocked = calculateCoefficient(tc * mDerivationFactorUnlock);
    mCoeffSlowLocked = calculateCoefficient(tc * mDerivationFactorLongTerm);
    mCoeffSlowUnlocked = calculateCoefficient(tc * mDerivationFactorLongTerm * mDerivationFactorUnlock);

    DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "[IasAvbClockDomain::setFilter] tc=", mTimeConstant,
        mAvgCallsPerSec, "call/sec (", mCoeffFastLocked,
        "/", mCoeffFastUnlocked, "/", mCoeffSlowLocked,
        "/", mCoeffSlowUnlocked);

    if (mLockState > eIasAvbLockStateUnlocked)
    {
      mLockState = eIasAvbLockStateUnlocked;
      lockStateChanged();
    }
  }
}


void IasAvbClockDomain::setDerivationFactors(double factorLongTerm, double factorUnlock)
{
  mDerivationFactorLongTerm = factorLongTerm;
  mDerivationFactorUnlock = factorUnlock;
  setFilter(mTimeConstant, mAvgCallsPerSec);
}


void IasAvbClockDomain::setLockThreshold1( uint32_t ppm )
{
  if (ppm > 0u)
  {
    mThresholdFastHigh = 1.0 + (1e-6 * double(ppm));
    mThresholdFastLow = 1.0 / mThresholdFastHigh;
  }
}


void IasAvbClockDomain::setLockThreshold2(uint32_t ppm)
{
  if (ppm > 0u)
  {
    mThresholdSlowHigh = 1.0 + (1e-6 * double(ppm));
    mThresholdSlowLow = 1.0 / mThresholdSlowHigh;
  }
}


double IasAvbClockDomain::calculateCoefficient(double timeConstant)
{
  return (0.0 == timeConstant) ? 0.0 : (std::exp(-1.0 / timeConstant));
}

IasAvbProcessingResult IasAvbClockDomain::registerClient(IasAvbClockDomainClientInterface * const client)
{
  IasAvbProcessingResult ret = eIasAvbProcInvalidParam;

  if (NULL != client)
  {
    if (NULL != mClient)
    {
      ret = eIasAvbProcAlreadyInUse;
    }
    else
    {
      ret = eIasAvbProcOK;
      mClient = client;
    }
  }

  return ret;
}

IasAvbProcessingResult IasAvbClockDomain::unregisterClient(IasAvbClockDomainClientInterface * const client)
{
  IasAvbProcessingResult ret = eIasAvbProcInvalidParam;

  if (client == mClient)
  {
    ret = eIasAvbProcOK;
    mClient = NULL;
  }

  return ret;
}



} // namespace IasMediaTransportAvb
