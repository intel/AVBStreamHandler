/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockController.cpp
 * @brief   IasAvbClockController class implementation
 * @details
 *
 * @date    2013
 */

#include "avb_streamhandler/IasAvbClockController.hpp"

#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#include <unistd.h>
#include <math.h>
#include <stdlib.h>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbClockController::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

IasAvbClockController::IasAvbClockController()
  : mMaster(NULL)
  , mSlave(NULL)
  , mClockDriver(NULL)
  , mDriverParam(0u)
  , mEngage(true)
  , mLockState(eInit)
  , mUpperLimit(1.0001)
  , mLowerLimit(1.0/1.0001)
  , mThread(this,"ClockController")
  , mSignal()
  , mEndThread(false)
  , mWait(25000u)
  , mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_ACC"))
{
  // do nothing
}

IasAvbClockController::~IasAvbClockController()
{
  cleanup();
}

void IasAvbClockController::cleanup()
{
  if (mThread.isRunning())
  {
    mThread.stop();
  }

  if (NULL != mSlave)
  {
    (void) mSlave->unregisterClient(this);
    mSlave= NULL;
  }

  if (NULL != mMaster)
  {
    (void) mMaster->unregisterClient(this);
    mMaster= NULL;
  }

  mClockDriver = NULL;
}

IasAvbProcessingResult IasAvbClockController::init(IasAvbClockDomain * const master
    , IasAvbClockDomain * const slave
    , uint32_t driverParam
    )
{
  IasAvbProcessingResult ret = eIasAvbProcInvalidParam;

  if ((NULL != master) && (NULL != slave))
  {
    mClockDriver = IasAvbStreamHandlerEnvironment::getClockDriver();
    setLimits();
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlWaitInterval, mWait);
    if (mWait < cWaitMin)
    {
      mWait = cWaitMin;
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "ignored specified wait period, set to", uint32_t(cWaitMin), "us");
    }
    uint64_t engage = 1u;
    if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlEngage, engage))
    {
      mEngage = (engage != 0u);
    }
    if (!mEngage)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "clock driver disengaged!");
    }

    mDriverParam = driverParam;

    if (NULL == mClockDriver)
    {
      /* Clock driver is needed since clock recovery is requested in configuration
         (slave clock id is != 0) but the driver might not be specified in command
         line or configuration */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "No clock driver available!");
      ret = eIasAvbProcInitializationFailed;
    }
    else
    {
      mLockState = eInit;
      mMaster = master;
      mSlave = slave;
      ret = slave->registerClient(this);

      if (eIasAvbProcOK == ret)
      {
        ret = master->registerClient(this);
      }
    }
  }

  if (eIasAvbProcOK == ret)
  {
    IasResult r = mThread.start(true, this);
    if (!IAS_SUCCEEDED(r))
    {
      ret = eIasAvbProcThreadStartFailed;
    }
  }

  if (eIasAvbProcOK != ret)
  {
    cleanup();
  }

  return ret;
}

void IasAvbClockController::setLimits()
{
  uint64_t ppm = 0u;
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlUpperLimit, ppm))
  {
    mUpperLimit = 1.0 + (double(ppm) * 0.000001);
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlLowerLimit, ppm))
  {
    mLowerLimit = 1.0 / (1.0 + (double(ppm) * 0.000001));
  }
}

void IasAvbClockController::notifyUpdateRatio(const IasAvbClockDomain * const domain)
{
  if (domain == mSlave)
  {
    (void) mSignal.signal();
  }
}

void IasAvbClockController::notifyUpdateLockState(const IasAvbClockDomain * const domain)
{
  if (NULL != mMaster && NULL != mSlave)
  {
    std::string name;
    if (domain == mMaster)
    {
      name = "master";
    }
    else if (domain == mSlave)
    {
      name = "slave";
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "received notification from unknown clock domain!");
    }

    if (!name.empty())
    {
      const IasAvbClockDomain::IasAvbLockState newState = domain->getLockState();
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, name.c_str(), int32_t(newState));

      if ((newState < IasAvbClockDomain::eIasAvbLockStateLocked) && (mLockState > eUnlocked))
      {
        mLockState = eUnlocked;
      }
    }
  }
}

IasResult IasAvbClockController::beforeRun()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  mEndThread = false;
  return IasResult::cOk;
}

IasResult IasAvbClockController::run()
{
  static uint32_t count = 0u;
  int64_t lastCountMaster = 0;
  int64_t lastCountSlave = 0;
  int64_t lastTimeMaster = 0;
  int64_t lastTimeSlave = 0;
  int64_t offset = 0;
  uint64_t holdOff = 0;
  uint64_t holdOffTime = 60000000u;
  uint64_t lockCount = 0u;
  uint64_t lockCountMax = 5u;
  double lastDev = 0.0;
  double bufDev = 0.0;
  double bufRate = 0.0;
  double gain = 100.0e-9;
  double coeff1 = 0.5;
  double coeff2 = 0.0;
  double coeff3 = 0.8;
  double coeff4 = 0.0;
  double lockThreshold = 2e-6;


  AVB_ASSERT(NULL != mMaster);
  AVB_ASSERT(NULL != mSlave);
  AVB_ASSERT(NULL != mClockDriver);

  uint64_t val = 0u;
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlHoldOff, val))
  {
    holdOffTime = val * 1000u;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlGain, val))
  {
    gain = double(val) * 1e-9;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlCoeff1, val))
  {
    coeff1 = double(int64_t(val)) * 1e-6;
  }
  coeff2 = 1.0 - coeff1; // auto-adapt coeff2 so the filter is gain-neutral
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlCoeff2, val))
  {
    coeff2 = double(int64_t(val)) * 1e-6;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlCoeff3, val))
  {
    coeff3 = double(int64_t(val)) * 1e-6;
  }
  // do not auto-adapt coeff4 since the differential part of the controller should have a smaller gain
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlCoeff4, val))
  {
    coeff4 = double(int64_t(val)) * 1e-6;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlLockCount, val))
  {
    lockCountMax = val;
  }
  if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cClockCtrlLockThres, val))
  {
    lockThreshold = double(val) * 1e-6;
  }



  while (!mEndThread)
  {
    mSignal.wait();

    if (!mEndThread)
    {
      double correction0 = 0.0;

      uint64_t masterTime = 0u;
      uint64_t slaveTime = 0u;
      const int64_t masterCount = int64_t(mMaster->getEventCount(masterTime));
      const int64_t slaveCount = int64_t(mSlave->getEventCount(slaveTime));
      double masterRate = 0.0;
      int64_t deltaTM = int64_t(masterTime - lastTimeMaster);
      double slaveRate = 0.0;
      int64_t deltaTS = int64_t(slaveTime - lastTimeSlave);
      const double timeOffset = double(int64_t(masterTime - slaveTime));


      // If slave time jumps back into past an epoch change in PTP is very likely. In order to align the time
      // in the rx clock domain a reset request is set. The AVB audio RX stream will detect it and reset the domain.
      if (int64_t(slaveTime - lastTimeSlave) < 0)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "set a reset request (curr. slave time, last slave time",
            slaveTime, lastTimeSlave);

        mMaster->setResetRequest();
        holdOff = 0u;
      }

      if (0 == deltaTM)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "unlocked (no master time update)");
        mLockState = eUnlocked;
      }
      else
      {
        masterRate = double(masterCount - lastCountMaster) / double(deltaTM);
      }

      if (0 == deltaTS)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "unlocked (no slave time update)");
        mLockState = eUnlocked;
      }
      else
      {
        slaveRate = double(slaveCount - lastCountSlave) / double(deltaTS);
      }

      // calculate ideal slave count based on master rate and compare with actual slave count
      const double deviation = double((slaveCount - masterCount) - offset) + (timeOffset * masterRate);
      correction0 =  1.0;

      switch (mLockState)
      {
      case eInit:
        // first iteration is only to fill last..Master variables
        mLockState = eUnlocked;
        break;

      case eUnlocked:
        {
          if (IasAvbClockDomain::eIasAvbLockStateLocked == mMaster->getLockState())
          {
            // input clock domain is locked, now we can try to lock as well
            lockCount = 0u;
            mLockState = eLockingRate;
            holdOff = 0u;
            DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "master domain locked, rates (m,s)",
                mMaster->getRateRatio(), mSlave->getRateRatio());
          }
        }
        break;

      case eLockingRate:
        {
          double masterRateFiltered = mMaster->getRateRatio();
          double slaveRateFiltered = mSlave->getRateRatio();

          if (fabs(masterRateFiltered - slaveRateFiltered) < lockThreshold)
          {
            lockCount++;
            if (lockCount > lockCountMax)
            {
              lockCount = 0u;
              mLockState = eLockingPhase;
              offset = slaveCount - masterCount + int64_t(timeOffset * masterRate);
              lastDev = 0.0;

              DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "rate lock achieved (o/m/s/m-s)",
                  offset,
                  masterRateFiltered,
                  slaveRateFiltered,
                  masterRateFiltered - slaveRateFiltered);
              break;
            }
          }
          else
          {
            lockCount = 0u;
          }
        }
        // fall-through

      case eLockingPhase:
      case eLocked:

        {
          // don't touch the PLL at every cycle - give it some time to settle
          if ((masterTime > holdOff) || (masterTime < (holdOff - holdOffTime)))
          {
            correction0 = masterRate / slaveRate;
            holdOff = masterTime + holdOffTime;
          }

          if (mLockState >= eLockingPhase)
          {
            // determine rate (phase differential)
            // do not consider the sample rate (mWait) here
            const double rate = deviation - lastDev;
            lastDev = deviation;

            // Butterworth filters for phase and rate
            bufDev = (coeff1 * (-deviation)) + (coeff2 * bufDev);
            bufRate = (coeff3 * (-rate)) + (coeff4 * bufRate);

            correction0 =  correction0 + ((bufDev + bufRate) * gain);

            if (eLockingPhase == mLockState)
            {
              if (fabs(deviation) < 1.0)
              {
                DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " phase lock achieved, deviation =", deviation);
                mLockState = eLocked;
              }
            }

            // unlock if we're more than 10 samples (@48kHz) off
            if (fabs(deviation) > 10.0)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " phase unlocked (deviation out of range)", deviation);
              lockCount = 0u;
              correction0 = 1.0;
              mLockState = eLockingRate;
              holdOff = 0u;
            }
          }
        }
        break;

      case eOff: // DEBUG ONLY
        correction0 = 1.0;
        break;

      default:
        AVB_ASSERT(false);
        break;
      }

      DltLogLevelType loglevel = DLT_LOG_VERBOSE;
      count++;
      AVB_ASSERT(0u != mWait);
      if ((1000000u / mWait) == count)
      {
        loglevel = DLT_LOG_DEBUG;
        count = 0u;
      }

      lastCountMaster = masterCount;
      lastCountSlave = slaveCount;
      lastTimeMaster = masterTime;
      lastTimeSlave = slaveTime;

      double correction = correction0;

      if (correction > mUpperLimit)
      {
        correction = mUpperLimit;
      }

      if (correction < mLowerLimit)
      {
        correction = mLowerLimit;
      }

      if ((mEngage) && (1.0 != correction))
      {
        mClockDriver->updateRelative(mDriverParam, correction);
      }

      DLT_LOG_CXX(*mLog, loglevel, LOG_PREFIX, " << m=", masterCount,
          ("/"), masterTime,
          ("s="), slaveCount,
          ("/"), slaveTime,
          ("d="), deviation,
          ("c0'="), correction0 - 1.0,
          ("corr="), correction - 1.0,
          ("m-s="), masterTime - slaveTime,
          ("l="), int32_t(mLockState),
          ("o="), int32_t(offset),
          (">>"));

      ::usleep(mWait);
    }
  }

  return IasResult::cOk;
}

IasResult IasAvbClockController::shutDown()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  mEndThread = true;
  IasResult ret = IasResult::cFailed;
  if (mSignal.signal())
  {
    ret = IasResult::cOk;
  }
  return ret;
}

IasResult IasAvbClockController::afterRun()
{
  return IasResult::cOk;
}


} // namespace
