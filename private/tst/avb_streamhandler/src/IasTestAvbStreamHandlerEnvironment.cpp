/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestAvbStreamHandlerEnvironment.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"
#include <vector>

#include <sys/socket.h>

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;

class IasTestAvbStreamHandlerEnvironment : public ::testing::Test
{
protected:
  IasTestAvbStreamHandlerEnvironment():
    mIasAvbStreamHandlerEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
    DLT_REGISTER_CONTEXT_LL_TS(mDltContext,
              "TEST",
              "IasTestAvbStreamHandlerEnvironment",
              DLT_LOG_INFO,
              DLT_TRACE_STATUS_OFF);
  }

  virtual ~IasTestAvbStreamHandlerEnvironment()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;
    dlt_enable_local_print();
    mIasAvbStreamHandlerEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mIasAvbStreamHandlerEnvironment);
    mIasAvbStreamHandlerEnvironment->registerDltContexts();
  }

  virtual void TearDown()
  {
    if (NULL != mIasAvbStreamHandlerEnvironment)
    {
      delete mIasAvbStreamHandlerEnvironment;
      mIasAvbStreamHandlerEnvironment = NULL;
    }

    if(0 < mSocketFdList.size())
    {
      for(auto &vecFd : mSocketFdList)
      {
        if(0 > close(vecFd))
          DLT_LOG(mDltContext,  DLT_LOG_ERROR, DLT_STRING("Error closing fd: "), DLT_INT(vecFd),
                    DLT_STRING(strerror(errno)));
      }

      mSocketFdList.clear();
    }

    heapSpaceLeft = heapSpaceInitSize;
  }

  void createMaxFds();

  IasAvbStreamHandlerEnvironment* mIasAvbStreamHandlerEnvironment;

protected:
  DltContext mDltContext;
  std::vector<int32_t> mSocketFdList;
};

void IasTestAvbStreamHandlerEnvironment::createMaxFds()
{
  int32_t sFd = -1;
  while(0 <= (sFd = socket(PF_LOCAL, SOCK_DGRAM, 0)))
    mSocketFdList.push_back(sFd);

  if (0 > sFd)
    DLT_LOG(mDltContext,  DLT_LOG_INFO, DLT_STRING("Created max number of fd's: ["), DLT_INT(errno), DLT_STRING("]: "),
              DLT_STRING(strerror(errno)));
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetClockDriver)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasAvbClockDriverInterface* clockDriver = NULL;

  clockDriver = mIasAvbStreamHandlerEnvironment->getClockDriver();
  ASSERT_TRUE(NULL == clockDriver);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetNetworkInterfaceName)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  const std::string* result;
  result = mIasAvbStreamHandlerEnvironment->getNetworkInterfaceName();
  (void)result;
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetPtpProxy)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasLibPtpDaemon* result;
  result = mIasAvbStreamHandlerEnvironment->getPtpProxy();
  ASSERT_TRUE(result == NULL);
}


TEST_F(IasTestAvbStreamHandlerEnvironment, GetMrpProxy)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasLibMrpDaemon* result;
  result = mIasAvbStreamHandlerEnvironment->getMrpProxy();
  ASSERT_TRUE(result == NULL);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetIgbDevice)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  device_t* result;
  result = mIasAvbStreamHandlerEnvironment->getIgbDevice();
  ASSERT_TRUE(result == NULL);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetSourceMac)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  const IasAvbMacAddress* result;
  result = mIasAvbStreamHandlerEnvironment->getSourceMac();
  ASSERT_TRUE(result != NULL);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, IsLinkUp)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  bool result;
  result = mIasAvbStreamHandlerEnvironment->isLinkUp();
  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetConfigValueT)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  bool result;
  std::string key;
  uint32_t value = 0;
  result = mIasAvbStreamHandlerEnvironment->getConfigValue(key, value);

  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, GetConfigValue)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  bool result;
  std::string key;
  std::string value;
  result = mIasAvbStreamHandlerEnvironment->getConfigValue(key, value);

  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, SetQueryConfigValueT)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  std::string key;
  uint64_t value = 4711;
  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  // key empty
  result = mIasAvbStreamHandlerEnvironment->setConfigValue(key, value);
  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, result);

  key = "my.test.key";
  result = mIasAvbStreamHandlerEnvironment->setConfigValue(key, value);
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, result);

  uint64_t value2 = 1;
  bool result2 = mIasAvbStreamHandlerEnvironment->queryConfigValue(key, value2);
  ASSERT_TRUE(result2);
  ASSERT_EQ( value, value2 );

  value2 = 2;
  value = value2;
  key = "my.non-existing.key";
  result2 = mIasAvbStreamHandlerEnvironment->queryConfigValue(key, value2);
  ASSERT_FALSE(result2);
  ASSERT_EQ( value, value2 );

  mIasAvbStreamHandlerEnvironment->mRegistryLocked = true;
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, mIasAvbStreamHandlerEnvironment->setConfigValue(key, value));
}

TEST_F(IasTestAvbStreamHandlerEnvironment, SetQueryConfigValue)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  std::string key;
  std::string value = "08/15";
  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  // key empty
  result = mIasAvbStreamHandlerEnvironment->setConfigValue(key, value);
  ASSERT_EQ(IasAvbResult::eIasAvbResultInvalidParam, result);

  key = "my.test.key";
  result = mIasAvbStreamHandlerEnvironment->setConfigValue(key, value);
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, result);

  std::string value2 = "dummy";
  bool result2 = mIasAvbStreamHandlerEnvironment->queryConfigValue(key, value2);
  ASSERT_TRUE( result2);
  ASSERT_EQ( value, value2 );

  value2 = "original value";
  value = value2;
  key = "my.non-existing.key";
  result2 = mIasAvbStreamHandlerEnvironment->queryConfigValue(key, value2);
  ASSERT_FALSE( result2);
  ASSERT_EQ( value, value2 );
}

TEST_F(IasTestAvbStreamHandlerEnvironment, SetDefaultConfigValues)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  mIasAvbStreamHandlerEnvironment->setDefaultConfigValues();
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, ValidateRegistryEntries)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  bool result = mIasAvbStreamHandlerEnvironment->validateRegistryEntries();
  uint8_t debugDumpRegistry = 1u;
  mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cDebugDumpRegistry, debugDumpRegistry);
  ASSERT_TRUE(result);
  result = mIasAvbStreamHandlerEnvironment->validateRegistryEntries();
  ASSERT_TRUE(result);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, CreatePtProxy)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;
  result = mIasAvbStreamHandlerEnvironment->createPtpProxy();

  ASSERT_EQ(eIasAvbProcInitializationFailed, result);

}

TEST_F(IasTestAvbStreamHandlerEnvironment, CreateMrpProxy)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;
  result = mIasAvbStreamHandlerEnvironment->createMrpProxy();

  ASSERT_EQ(eIasAvbProcNotImplemented, result);

}

TEST_F(IasTestAvbStreamHandlerEnvironment, CreateIgbDevice)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;
  result = mIasAvbStreamHandlerEnvironment->createIgbDevice();

  ASSERT_EQ(eIasAvbProcInitializationFailed, result);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, LoadClockDriver_bad_param)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  ASSERT_EQ(eIasAvbProcInvalidParam, mIasAvbStreamHandlerEnvironment->loadClockDriver(""));
  ASSERT_EQ(eIasAvbProcInvalidParam, mIasAvbStreamHandlerEnvironment->loadClockDriver("../../../libias-media_transport-avb_clockdriver.so"));
  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandlerEnvironment->loadClockDriver("i_am_not_there.so"));
}

// this test is disabled since using dlopen() in this context breaks Bullseye's coverage instrumentation.
//TODO: re-enable once Bullseye has fixed the problem
//TEST_F(IasTestAvbStreamHandlerEnvironment, LoadClockDriver)
//{
//  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);
//  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandlerEnvironment->loadClockDriver("pluginias-media_transport-avb_configuration_reference.so"));
//  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->loadClockDriver("libias-media_transport-avb_clockdriver.so"));
//}

TEST_F(IasTestAvbStreamHandlerEnvironment, QuerySourceMac)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;
  result = mIasAvbStreamHandlerEnvironment->querySourceMac();

  ASSERT_EQ(eIasAvbProcErr, result);

  if (IasSpringVilleInfo::fetchData())
    ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cNwIfName,
                                                                      std::string(IasSpringVilleInfo::getInterfaceName())));

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->querySourceMac());

  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->querySourceMac());
}

TEST_F(IasTestAvbStreamHandlerEnvironment, queries_maxFds)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  if (IasSpringVilleInfo::fetchData())
    ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cNwIfName,
                                                                      std::string(IasSpringVilleInfo::getInterfaceName())));

  createMaxFds();
  ASSERT_EQ(eIasAvbProcErr, mIasAvbStreamHandlerEnvironment->querySourceMac());
  ASSERT_FALSE(mIasAvbStreamHandlerEnvironment->queryLinkState());
  ASSERT_EQ(-1, mIasAvbStreamHandlerEnvironment->queryLinkSpeed());
}

TEST_F(IasTestAvbStreamHandlerEnvironment, QueryLinkState)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  bool result;
  result = mIasAvbStreamHandlerEnvironment->queryLinkState();

  ASSERT_FALSE(result);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, QueryLinkSpeed)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);
  mIasAvbStreamHandlerEnvironment->setDefaultConfigValues();
  IasSpringVilleInfo::fetchData();
  mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());
  mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cTestingProfileEnable, 1);
  mIasAvbStreamHandlerEnvironment->querySourceMac();
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment->queryLinkSpeed() != 0);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, double_create)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasSpringVilleInfo::printDebugInfo();
  mIasAvbStreamHandlerEnvironment->setDefaultConfigValues();

  ASSERT_TRUE(IasSpringVilleInfo::fetchData());
  IasSpringVilleInfo::printDebugInfo();

  mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());

  mIasAvbStreamHandlerEnvironment->createIgbDevice();
  mIasAvbStreamHandlerEnvironment->createIgbDevice();

  mIasAvbStreamHandlerEnvironment->createPtpProxy();
  mIasAvbStreamHandlerEnvironment->createPtpProxy();

  mIasAvbStreamHandlerEnvironment->createMrpProxy();
  mIasAvbStreamHandlerEnvironment->createMrpProxy();
}

TEST_F(IasTestAvbStreamHandlerEnvironment, no_instance)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);
  mIasAvbStreamHandlerEnvironment->unregisterDltContexts();
  delete mIasAvbStreamHandlerEnvironment;
  mIasAvbStreamHandlerEnvironment = NULL;

  ASSERT_TRUE(NULL == IasAvbStreamHandlerEnvironment::getNetworkInterfaceName());
  ASSERT_TRUE(NULL == IasAvbStreamHandlerEnvironment::getPtpProxy());
  ASSERT_TRUE(NULL == IasAvbStreamHandlerEnvironment::getMrpProxy());
  ASSERT_TRUE(NULL == IasAvbStreamHandlerEnvironment::getIgbDevice());
  ASSERT_TRUE(NULL == IasAvbStreamHandlerEnvironment::getSourceMac());

  ASSERT_EQ(0u, IasAvbStreamHandlerEnvironment::getWatchdogTimeout());
  ASSERT_FALSE(IasAvbStreamHandlerEnvironment::isWatchdogEnabled());
  ASSERT_FALSE(IasAvbStreamHandlerEnvironment::isTestProfileEnabled());
  ASSERT_EQ(NULL, IasAvbStreamHandlerEnvironment::getStatusSocket());
  ASSERT_EQ(NULL, IasAvbStreamHandlerEnvironment::getDiaLogger());

  std::string value("");
  uint32_t numValue(0);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, HEAP_Failed)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  IasAvbProcessingResult result = eIasAvbProcErr;

  heapSpaceLeft = 0;

  result = mIasAvbStreamHandlerEnvironment->createPtpProxy();
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);

  heapSpaceLeft = 0;

  result = mIasAvbStreamHandlerEnvironment->createIgbDevice();
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);
}

//TODO: Implement right asserts for KSL
TEST_F(IasTestAvbStreamHandlerEnvironment, create_destroyWatchdog)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  ASSERT_EQ(eIasAvbProcInitializationFailed, mIasAvbStreamHandlerEnvironment->createWatchdog());

  std::string wdEnvVar("WATCHDOG_USEC");
  std::string wdTimeoutUSec("100000");
  ASSERT_EQ(0, setenv(wdEnvVar.c_str(), wdTimeoutUSec.c_str(), 0));

  ASSERT_FALSE(mIasAvbStreamHandlerEnvironment->isWatchdogEnabled());
  uint32_t wdTimeout = std::stoul(wdTimeoutUSec, NULL, 10);
//  ASSERT_EQ(uint32_t(wdTimeout/1000), mIasAvbStreamHandlerEnvironment->getWatchdogTimeout());
  ASSERT_EQ(0, mIasAvbStreamHandlerEnvironment->getWatchdogTimeout());

  // already created
//  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->createWatchdog());

  ASSERT_EQ(0, unsetenv(wdEnvVar.c_str()));

  mIasAvbStreamHandlerEnvironment->destroyWatchdog();
  ASSERT_FALSE(mIasAvbStreamHandlerEnvironment->mUseWatchdog);
  ASSERT_EQ(0u, mIasAvbStreamHandlerEnvironment->mWdTimeout);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, getDltContext)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);

  DltContext &some = mIasAvbStreamHandlerEnvironment->getDltContext("testContext");
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment->mDltCtxDummy == &some);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, notifySchedulingIssue)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);
  std::string exText(" exception ");
  mIasAvbStreamHandlerEnvironment->notifySchedulingIssue(mDltContext, exText, 1000000u, 2000000u);
}

TEST_F(IasTestAvbStreamHandlerEnvironment, setTxRingSize)
{
  ASSERT_TRUE(mIasAvbStreamHandlerEnvironment != NULL);
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->setTxRingSize());

  mIasAvbStreamHandlerEnvironment->setDefaultConfigValues();
  IasSpringVilleInfo::fetchData();
  mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());
  mIasAvbStreamHandlerEnvironment->setConfigValue(IasRegKeys::cDebugNwIfTxRingSize, 256);
  ASSERT_EQ(eIasAvbProcOK, mIasAvbStreamHandlerEnvironment->setTxRingSize());
}
