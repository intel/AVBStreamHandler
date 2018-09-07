/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbPtpClockDomain.cpp
 *  @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private

using namespace IasMediaTransportAvb;
namespace IasMediaTransportAvb
{
class IasTestAvbPtpClockDomain : public ::testing::Test
{
protected:
  IasTestAvbPtpClockDomain():
    mAvbPtpClockDomain(NULL)
  , mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbPtpClockDomain()
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

    mAvbPtpClockDomain = new IasAvbPtpClockDomain();
  }

  virtual void TearDown()
  {
    delete mAvbPtpClockDomain;
    mAvbPtpClockDomain = NULL;

    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }
  }

  IasAvbPtpClockDomain* mAvbPtpClockDomain;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};
} // namespace IasMediaTransportAvb

TEST_F(IasTestAvbPtpClockDomain, CTor_DTor)
{
  ASSERT_TRUE(NULL != mAvbPtpClockDomain);
}
