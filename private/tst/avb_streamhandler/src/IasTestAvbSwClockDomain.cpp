/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbSwClockDomain.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbSwClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private

using namespace IasMediaTransportAvb;
namespace IasMediaTransportAvb
{

class IasTestAvbSwClockDomain : public ::testing::Test
{
protected:
    IasTestAvbSwClockDomain():
    mAvbSwClockDomain(NULL),
    mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbSwClockDomain()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    dlt_enable_local_print();
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->registerDltContexts();

    mAvbSwClockDomain = new IasAvbSwClockDomain();
  }

  virtual void TearDown()
  {
    delete mAvbSwClockDomain;
    mAvbSwClockDomain = NULL;

    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }
  }

  IasAvbSwClockDomain* mAvbSwClockDomain;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};
} // namespace IasMediaTransportAvb


TEST_F(IasTestAvbSwClockDomain, reset)
{
  ASSERT_TRUE(NULL != mAvbSwClockDomain);
  uint32_t avgCallsPerSec = 0;
  mAvbSwClockDomain->reset(avgCallsPerSec);
}

TEST_F(IasTestAvbSwClockDomain, advance)
{
  ASSERT_TRUE(NULL != mAvbSwClockDomain);

  mAvbSwClockDomain->mPtpProxy = new IasLibPtpDaemon("/ptp", static_cast<uint32_t>(SHM_SIZE));
  ASSERT_EQ(eIasAvbProcOK, mAvbSwClockDomain->mPtpProxy->init());

  ASSERT_TRUE(NULL != mAvbSwClockDomain->mPtpProxy);
  uint32_t elapsed = 0;
  uint64_t timestamp = 0u;
  mAvbSwClockDomain->advance(elapsed, timestamp);

  elapsed = 6000000 + 1;
  timestamp = 10000000;
  mAvbSwClockDomain->advance(elapsed, timestamp);

  delete mAvbSwClockDomain->mPtpProxy;
  mAvbSwClockDomain->mPtpProxy = NULL;
}

TEST_F(IasTestAvbSwClockDomain, updateRelative)
{
  ASSERT_TRUE(NULL != mAvbSwClockDomain);

  mAvbSwClockDomain->mPtpProxy = new IasLibPtpDaemon("/ptp", SHM_SIZE);

  ASSERT_TRUE(NULL != mAvbSwClockDomain->mPtpProxy);

  mAvbSwClockDomain->reset( 0 ); // switches off smoothing
  mAvbSwClockDomain->updateRelative( 1.0 ); // get out of init state

  mAvbSwClockDomain->updateRelative( 1.25 );

  ASSERT_NEAR( 1.25,   mAvbSwClockDomain->getRateRatio(), 0.001 );

  mAvbSwClockDomain->updateRelative( 0.8 );

  ASSERT_NEAR( 1.0,   mAvbSwClockDomain->getRateRatio(), 0.001 );

  delete mAvbSwClockDomain->mPtpProxy;
  mAvbSwClockDomain->mPtpProxy = NULL;
}

TEST_F(IasTestAvbSwClockDomain, CTor_setSwDeviation)
{
  ASSERT_TRUE(NULL != mAvbSwClockDomain);

  mEnvironment->setConfigValue(IasRegKeys::cClkSwDeviationLongterm, 1000u);
  mEnvironment->setConfigValue(IasRegKeys::cClkSwDeviationUnlock, 1000u);

  delete mAvbSwClockDomain;
  mAvbSwClockDomain = new IasAvbSwClockDomain();

  ASSERT_EQ(1.0f, mAvbSwClockDomain->mFactorLong);
  ASSERT_EQ(1.0f, mAvbSwClockDomain->mFactorUnlock);
}
