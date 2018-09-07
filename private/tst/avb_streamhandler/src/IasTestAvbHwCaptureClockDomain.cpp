/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbHwCaptureClockDomain.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"
#include <string>

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;

namespace IasMediaTransportAvb
{

class IasTestAvbHwCaptureClockDomain : public ::testing::Test
{
protected:
  IasTestAvbHwCaptureClockDomain()
  : streamHandler(DLT_LOG_INFO)
  , mAvbHwCaptureClockDomain(NULL)
  , mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbHwCaptureClockDomain()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

    mAvbHwCaptureClockDomain = new (std::nothrow) IasAvbHwCaptureClockDomain();
  }

  virtual void TearDown()
  {
    delete mAvbHwCaptureClockDomain;
    mAvbHwCaptureClockDomain = NULL;

    if (mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }

    streamHandler.cleanup();

    heapSpaceLeft = heapSpaceInitSize;
  }

  bool createEnvironment()
  {
    if(NULL != mEnvironment)
    {
      return false;
    }

    mEnvironment = new (std::nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    if (mEnvironment)
    {
      mEnvironment->registerDltContexts();
    }
    return true;
  }

  enum TestPolicy : uint8_t {
        ePolicyOther,
        ePolicyRr,
        ePolicyFifo
  };

  IasAvbProcessingResult initStreamHandler(TestPolicy policy = ePolicyFifo);

  bool configSetup()
  {
    if (mAvbHwCaptureClockDomain && mEnvironment)
    {
      delete mAvbHwCaptureClockDomain;
      mAvbHwCaptureClockDomain = NULL;

      mEnvironment->setConfigValue(IasRegKeys::cClockHwCapFrequency, 1);
      mEnvironment->setConfigValue(IasRegKeys::cClkHwTimeConstant, 1);
      mEnvironment->setConfigValue(IasRegKeys::cClkHwDeviationLongterm, 1);
      mEnvironment->setConfigValue(IasRegKeys::cClkHwDeviationUnlock, 1);

      mAvbHwCaptureClockDomain = new (std::nothrow) IasAvbHwCaptureClockDomain();
      if (mAvbHwCaptureClockDomain)
      {
        return true;
      }
    }
    return false;
  }

  IasAvbStreamHandler streamHandler;
  IasAvbHwCaptureClockDomain* mAvbHwCaptureClockDomain;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

IasAvbProcessingResult IasTestAvbHwCaptureClockDomain::initStreamHandler(TestPolicy policy)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  // IT HAS TO BE SET TO ZERO - getopt_long - DefaultConfig_passArguments - needs to be reinitialized before use
  optind = 0;

  if (!IasSpringVilleInfo::fetchData())
  {
    return eIasAvbProcErr;
  }

  std::string policyStr;
  switch (policy)
  {
    case ePolicyFifo:
      policyStr = "fifo";
      break;
    case ePolicyOther:
      policyStr = "other";
      break;
    case ePolicyRr:
      policyStr = "rr";
      break;
  }

  std::string policyConfVal("sched.policy=");
  policyConfVal += policyStr;
#ifdef IAS_HOST_BUILD
  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", "UnitTests",
    "-n", IasSpringVilleInfo::getInterfaceName(),
    "-k", policyConfVal.c_str()
  };
#else
  const char *args[] = {
    "setup",
    "-t", "Fedora",
    "-p", "UnitTests",
    "-n", IasSpringVilleInfo::getInterfaceName()
  };
#endif

  int argc = sizeof(args) / sizeof(args[0]);

  if (NULL != mEnvironment)
  {
    ret = eIasAvbProcErr;
  }
  else
  {
    ret = streamHandler.init(theConfigPlugin, true, argc, (char**)args);
  }

  return ret;
}

} // namespace IasMediaTransportAvb



TEST_F(IasTestAvbHwCaptureClockDomain, CTor_DTor)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);

  ASSERT_TRUE(createEnvironment());
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cClockHwCapFrequency, 0.001));

  delete mAvbHwCaptureClockDomain;
  // (mSleep = 1e9 / 2.5 / mNominal = cClockHwCapFrequency * 0.001) < 1e6 (T)
  mAvbHwCaptureClockDomain = new IasAvbHwCaptureClockDomain();
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);
}

TEST_F(IasTestAvbHwCaptureClockDomain, CTor_with_config_DTor)
{
  ASSERT_TRUE(createEnvironment());
  ASSERT_TRUE(configSetup());
}

TEST_F(IasTestAvbHwCaptureClockDomain, init)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);
  IasAvbProcessingResult result = mAvbHwCaptureClockDomain->init();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler(ePolicyRr));

  heapSpaceLeft = sizeof (IasThread) - 1;
  ASSERT_EQ(eIasAvbProcInitializationFailed, mAvbHwCaptureClockDomain->init());

  heapSpaceLeft = heapSpaceInitSize;
  ASSERT_EQ(eIasAvbProcOK, mAvbHwCaptureClockDomain->init());
}

TEST_F(IasTestAvbHwCaptureClockDomain, start)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);
  IasAvbProcessingResult result = mAvbHwCaptureClockDomain->start();
  ASSERT_EQ(eIasAvbProcNullPointerAccess, result);
}

TEST_F(IasTestAvbHwCaptureClockDomain, stop)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);
  IasAvbProcessingResult result = mAvbHwCaptureClockDomain->stop();
  ASSERT_EQ(eIasAvbProcNullPointerAccess, result);
}

TEST_F(IasTestAvbHwCaptureClockDomain, BRANCH_Init_DeInit)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);

  IasAvbProcessingResult result = eIasAvbProcErr;

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler(ePolicyOther));

  result = mAvbHwCaptureClockDomain->init();
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbHwCaptureClockDomain->init();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

  sleep(1);

  result = mAvbHwCaptureClockDomain->stop();
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestAvbHwCaptureClockDomain, BRANCH_StopThread)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);

  IasAvbProcessingResult result = eIasAvbProcErr;

  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  result = mAvbHwCaptureClockDomain->init();
  ASSERT_EQ(eIasAvbProcOK, result);
  usleep(100);

  result = mAvbHwCaptureClockDomain->stop();
  ASSERT_EQ(eIasAvbProcOK, result);

  // already stopped
  result = mAvbHwCaptureClockDomain->stop();
  ASSERT_EQ(eIasAvbProcOK, result);

  result = mAvbHwCaptureClockDomain->start();
  ASSERT_EQ(eIasAvbProcOK, result);
  usleep(100);

  // clean up is called by the DTor - stop thread action
}

TEST_F(IasTestAvbHwCaptureClockDomain, HEAP_Fail_testing)
{
  ASSERT_TRUE(NULL != mAvbHwCaptureClockDomain);
  ASSERT_EQ(eIasAvbProcOK, initStreamHandler());

  IasAvbProcessingResult result = eIasAvbProcErr;

  heapSpaceLeft = 0;

  result = mAvbHwCaptureClockDomain->init();
  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}
