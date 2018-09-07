/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbClockDomain.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbClockDomain.hpp"
#include "avb_streamhandler/IasAvbRawClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private


#include "test_common/IasSpringVilleInfo.hpp"

using namespace IasMediaTransportAvb;

namespace IasMediaTransportAvb
{


/* helper class */
class IasAvbClockDomainClientInterfaceImpl : public IasAvbClockDomainClientInterface
{
public:
  IasAvbClockDomainClientInterfaceImpl() {}
  virtual ~IasAvbClockDomainClientInterfaceImpl() {}

  virtual void notifyUpdateRatio(const IasAvbClockDomain * domain)
  {
    (void) domain;
  }
  virtual void notifyUpdateLockState(const IasAvbClockDomain * domain)
  {
    (void) domain;
  }
};

class IasTestAvbClockDomain : public ::testing::Test
{
protected:
  IasTestAvbClockDomain()
  : mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAvbClockDomain",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);
    mAvbClockDomain = new IasAvbClockDomain(mDltContext, eIasAvbClockDomainPtp);
  }

  virtual ~IasTestAvbClockDomain()
  {
    delete mAvbClockDomain;
    mAvbClockDomain = NULL;

    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    dlt_enable_local_print();
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->registerDltContexts();
  }

  virtual void TearDown()
  {
    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }
  }
  IasAvbClockDomain * mAvbClockDomain;

  DltContext mDltContext;
  IasAvbStreamHandlerEnvironment* mEnvironment;

  bool setupEnvironment()
  {
    if (mEnvironment)
    {
      mEnvironment->registerDltContexts();
      mEnvironment->setDefaultConfigValues();

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName()))
        {
          if (eIasAvbProcOK == mEnvironment->createIgbDevice())
          {
            if (eIasAvbProcOK == mEnvironment->createPtpProxy())
            {
              return true;
            }
          }
        }
      }
    }
    return false;
  }
};
} // namespace IasMediaTransportAvb

TEST_F(IasTestAvbClockDomain, CTor_DTor)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
}

TEST_F(IasTestAvbClockDomain, getLockState)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);

  IasAvbClockDomain::IasAvbLockState state = mAvbClockDomain->getLockState();
  (void) state;
}

TEST_F(IasTestAvbClockDomain, getRateRatio)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);

  float rateRatio = mAvbClockDomain->getRateRatio();
  (void) rateRatio;
}

TEST_F(IasTestAvbClockDomain, getType)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);

  IasAvbClockDomainType type = mAvbClockDomain->getType();
  (void) type;
}

TEST_F(IasTestAvbClockDomain, updateRateRatio)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  mAvbClockDomain->mLockState = (IasMediaTransportAvb::IasAvbClockDomain::IasAvbLockState)5;
  float newRatio = 0.0f;
  // locked1high (F), locked1low (F), locked (F), locked2 (F)
  mAvbClockDomain->updateRateRatio(newRatio);

  IasAvbClockDomainClientInterfaceImpl clockDomainClient;
  ASSERT_EQ(eIasAvbProcOK, mAvbClockDomain->registerClient(&clockDomainClient));
  mAvbClockDomain->mLockState = IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateUnlocked;
  mAvbClockDomain->mRateRatioFast = 1.5f;
  mAvbClockDomain->mThresholdSlowHigh = 2.0f;
  mAvbClockDomain->mThresholdSlowLow = mAvbClockDomain->mThresholdSlowHigh;
  newRatio = 10.0f;
  // locked1high (F), locked1low (T), locked (F), locked2 (F)
  mAvbClockDomain->updateRateRatio(newRatio);

  mAvbClockDomain->mThresholdFastHigh = 10.0f;
  mAvbClockDomain->mThresholdFastLow = 10.0f;
  mAvbClockDomain->mLockState = IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateLocked;
  // locked1high (T), locked1low (F), locked (F), locked2 (F)
  mAvbClockDomain->updateRateRatio(newRatio);

  mAvbClockDomain->mThresholdFastHigh = 11.0f;
  mAvbClockDomain->mThresholdFastLow  = 5.0f;
  mAvbClockDomain->mRateRatioFast = 1.0f;
  newRatio = 8.0f;
  mAvbClockDomain->mLockState = IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateLocking;
  // locked1high (T), locked1low (T), locked (T), locked2 (F)
  mAvbClockDomain->updateRateRatio(newRatio);

  newRatio = 11.0f;
  mAvbClockDomain->updateRateRatio(newRatio);

  newRatio = -0.1f;
  mAvbClockDomain->updateRateRatio(newRatio);
}

TEST_F(IasTestAvbClockDomain, setInitialValue)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  float initVal = 0.0;
  mAvbClockDomain->setInitialValue(initVal);

  initVal = -1.0;
  mAvbClockDomain->setInitialValue(initVal);
}

TEST_F(IasTestAvbClockDomain, setFilter)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  float timeConstant = 0.0;
  uint32_t avgCallsPerSec = 1u;
  mAvbClockDomain->setFilter(timeConstant, avgCallsPerSec);

  timeConstant = -1.0;
  mAvbClockDomain->setFilter(timeConstant, avgCallsPerSec);
}

TEST_F(IasTestAvbClockDomain, lockStateChanged)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  mAvbClockDomain->lockStateChanged();
}

TEST_F(IasTestAvbClockDomain, setDerivationFactors)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  float factorLongTerm = 0.0;
  mAvbClockDomain->setDerivationFactors(factorLongTerm);
}

TEST_F(IasTestAvbClockDomain, setLockThreshold1)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  uint32_t ppm = 0;
  mAvbClockDomain->setLockThreshold1(ppm);
}

TEST_F(IasTestAvbClockDomain, setResetRequest)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  mAvbClockDomain->setResetRequest();
  ASSERT_TRUE(mAvbClockDomain->getResetRequest());
  ASSERT_FALSE(mAvbClockDomain->getResetRequest());
}

TEST_F(IasTestAvbClockDomain, setDriftCompensation)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  int ppm = 0;

  IasAvbProcessingResult result = mAvbClockDomain->setDriftCompensation(ppm);
  ASSERT_EQ(eIasAvbProcOK, result);

  ppm = 1000001;
  result = mAvbClockDomain->setDriftCompensation(ppm);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  ppm = -1;
  result = mAvbClockDomain->setDriftCompensation(ppm);
  ASSERT_EQ(eIasAvbProcOK, result);

  ppm = -1000001;
  result = mAvbClockDomain->setDriftCompensation(ppm);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);
}

TEST_F(IasTestAvbClockDomain, setLockThreshold2)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  uint32_t ppm = 0;
  mAvbClockDomain->setLockThreshold2(ppm);
}

TEST_F(IasTestAvbClockDomain, clientTesting)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);

  IasAvbProcessingResult result = eIasAvbProcErr;
  IasAvbClockDomainClientInterfaceImpl clockDomainClient;

  /* registering */

  result = mAvbClockDomain->registerClient(NULL);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mAvbClockDomain->registerClient(&clockDomainClient);
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbClockDomain->registerClient(&clockDomainClient);
  ASSERT_EQ(eIasAvbProcAlreadyInUse, result);

  /* lockStateChanged */

  mAvbClockDomain->lockStateChanged();

  /* unregistering */

  result = mAvbClockDomain->unregisterClient(NULL);
  ASSERT_EQ(eIasAvbProcInvalidParam, result);

  result = mAvbClockDomain->unregisterClient(&clockDomainClient);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbClockDomain, onGetEventCount)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);

  mAvbClockDomain->onGetEventCount();
}

TEST_F(IasTestAvbClockDomain, onGetEventCount_rawCD)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);
  ASSERT_TRUE(setupEnvironment());

  delete mAvbClockDomain;
  IasAvbRawClockDomain * rawCD = new IasAvbRawClockDomain();
  mAvbClockDomain = rawCD;
  ASSERT_TRUE(0u == rawCD->mStartTime);

  mAvbClockDomain->onGetEventCount();
  ASSERT_TRUE(0u < rawCD->mStartTime);
  mAvbClockDomain->onGetEventCount();
}

TEST_F(IasTestAvbClockDomain, onGetEventCount_rawCD_invalid_params)
{
  ASSERT_TRUE(NULL != mAvbClockDomain);

  delete mAvbClockDomain;
  IasAvbRawClockDomain * rawCD = new IasAvbRawClockDomain();
  mAvbClockDomain = rawCD;
  mAvbClockDomain->onGetEventCount();

  ASSERT_TRUE(setupEnvironment());
  delete mAvbClockDomain;
  rawCD = new IasAvbRawClockDomain();
  mAvbClockDomain = rawCD;
  ASSERT_TRUE(0u == rawCD->mStartTime);

  rawCD->mLastUpdate = -250000;
  mAvbClockDomain->onGetEventCount();
  rawCD->mLastRaw = -250000;
  rawCD->mLastUpdate = -250000;
  mAvbClockDomain->onGetEventCount();

  rawCD->mLastRaw = -250000;
  rawCD->mLastUpdate = -250000;
  rawCD->mLastPtp = 0u;
  mAvbClockDomain->onGetEventCount();
}

