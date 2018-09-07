/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file   IasTestAvbRxStreamClockDomain.cpp
 * @date   2018
 * @brief  This file contains the unit tests of the IasAvbRxStreamClockDomain class.
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbRxStreamClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

namespace IasMediaTransportAvb {

class IasTestAvbRxStreamClockDomain : public ::testing::Test
{
protected:
  IasTestAvbRxStreamClockDomain():
    mRxClockDomain(NULL),
    mEnvironment(NULL)
  {
  }

  virtual ~IasTestAvbRxStreamClockDomain() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    // needed only for :Update" test
    // but
    // needs to be set up here due to ClockDomain gets Ptp on its CTor
    ASSERT_TRUE(LocalSetup());

    heapSpaceLeft = heapSpaceInitSize;
    mRxClockDomain = new IasAvbRxStreamClockDomain;
  }

  virtual void TearDown()
  {
    delete mEnvironment;
    mEnvironment = NULL;

    delete mRxClockDomain;
    mRxClockDomain = NULL;

    heapSpaceLeft = heapSpaceInitSize;
  }

  bool LocalSetup()
  {
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());

        if (eIasAvbProcOK == mEnvironment->createIgbDevice())
        {
          if (eIasAvbProcOK == mEnvironment->createPtpProxy())
          {
            if (IasAvbStreamHandlerEnvironment::getIgbDevice())
            {
              if (IasAvbStreamHandlerEnvironment::getPtpProxy())
              {
                return true;
              }
            }
          }
        }
      }
    }
    return false;
  }

  IasAvbRxStreamClockDomain* mRxClockDomain;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

TEST_F(IasTestAvbRxStreamClockDomain, Reset)
{
  ASSERT_TRUE(NULL != mRxClockDomain);

  IasAvbSrClass cl = IasAvbSrClass::eIasAvbSrClassHigh;
  uint32_t callsPerSecond = IasAvbTSpec::getPacketsPerSecondByClass(cl);
  uint64_t skipTime = 0u;
  uint32_t timestamp = 1u;
  uint32_t eventRate = 48000u;
  mEnvironment->setConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime);
  // IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime) (T)
  // && (0u != skipTime)                                                                        (F)
  mRxClockDomain->reset(cl, timestamp, eventRate);
  ASSERT_EQ(callsPerSecond, mRxClockDomain->mAvgCallsPerSec);

  mEnvironment->setConfigValue(IasRegKeys::cRxClkUpdateInterval, "skipTime");
  // IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime) (F)
  // && (0u != skipTime)                                                                        (F)
  mRxClockDomain->reset(cl, timestamp, eventRate);
  ASSERT_EQ(callsPerSecond, mRxClockDomain->mAvgCallsPerSec);

  callsPerSecond = 1E6;
  skipTime = 1u;
  mEnvironment->setConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime);
  // IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cRxClkUpdateInterval, skipTime) (T)
  // && (0u != skipTime)                                                                        (T)
  mRxClockDomain->reset(cl, timestamp, eventRate);
  ASSERT_EQ(callsPerSecond, mRxClockDomain->mAvgCallsPerSec);
}

TEST_F(IasTestAvbRxStreamClockDomain, Update)
{
  ASSERT_TRUE(NULL != mRxClockDomain);

  uint32_t deltaMediaClock = 0;
  uint32_t deltaWallClock = 0;
  uint32_t events = 6u;
  uint32_t timestamp = 0u;

  mRxClockDomain->update(events, timestamp, deltaMediaClock, deltaWallClock);

  deltaMediaClock = 1;
  timestamp = 125000;
  mRxClockDomain->update(events, timestamp, deltaMediaClock, deltaWallClock);
}

TEST_F(IasTestAvbRxStreamClockDomain, CTor_setSwDeviation)
{
  ASSERT_TRUE(NULL != mRxClockDomain);

  mEnvironment->setConfigValue(IasRegKeys::cClkRxDeviationLongterm, 1000u);
  mEnvironment->setConfigValue(IasRegKeys::cClkRxDeviationUnlock, 1000u);

  delete mRxClockDomain;
  mRxClockDomain = new IasAvbRxStreamClockDomain();

  ASSERT_EQ(1.0f, mRxClockDomain->mFactorLong);
  ASSERT_EQ(1.0f, mRxClockDomain->mFactorUnlock);
}

TEST_F(IasTestAvbRxStreamClockDomain, invalidate)
{
  ASSERT_TRUE(NULL != mRxClockDomain);

  mRxClockDomain->mTimeConstant = 1.0f;
  mRxClockDomain->mAvgCallsPerSec = 0;
  mRxClockDomain->mLockState = IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateInit;

  mRxClockDomain->invalidate();
  ASSERT_EQ(1, mRxClockDomain->mAvgCallsPerSec);
  ASSERT_EQ(1.0f, mRxClockDomain->mTimeConstant);
  ASSERT_EQ(IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateInit, mRxClockDomain->mLockState);
}

} // IasMediaTransportAvb
