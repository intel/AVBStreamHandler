/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestLibPtpDaemon.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"

namespace IasMediaTransportAvb{

class IasTestLibPtpDaemon : public ::testing::Test
{
protected:
  IasTestLibPtpDaemon():
    libPtpDaemon(NULL),
    mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestLibPtpDaemon()
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

    libPtpDaemon = new IasLibPtpDaemon("/ptp", static_cast<uint32_t>(SHM_SIZE));
  }

  virtual void TearDown()
  {
    delete libPtpDaemon;
    libPtpDaemon = NULL;

    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }
  }

  void LocalSetup()
  {
    ASSERT_NE((void*)NULL, mEnvironment);

    mEnvironment->setDefaultConfigValues();

    ASSERT_TRUE(IasSpringVilleInfo::fetchData());
    IasSpringVilleInfo::printDebugInfo();

    mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName());

    ASSERT_EQ(eIasAvbProcOK, mEnvironment->createIgbDevice());
    ASSERT_TRUE(NULL != IasAvbStreamHandlerEnvironment::getIgbDevice());
  }

  IasLibPtpDaemon* libPtpDaemon;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};

TEST_F(IasTestLibPtpDaemon, no_init)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  libPtpDaemon->cleanUp();
  ASSERT_FALSE(libPtpDaemon->isPtpReady());
}


TEST_F(IasTestLibPtpDaemon, Init)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  ASSERT_TRUE(NULL != igbDevice);

  libPtpDaemon->cleanUp();
  ASSERT_TRUE(1);

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));
  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));
}

TEST_F(IasTestLibPtpDaemon, Init_calculateConversionCoeffs)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(NULL));
}

TEST_F(IasTestLibPtpDaemon, GetLocalTime)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));

  ASSERT_TRUE(0u != libPtpDaemon->getLocalTime());
}

TEST_F(IasTestLibPtpDaemon, GetRealLocalTime_invalid_params)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();

  mEnvironment->setConfigValue(IasRegKeys::cPtpXtstampThresh, 0);

  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));

  libPtpDaemon->mIgbDevice = nullptr;
  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));
  libPtpDaemon->mIgbDevice = igbDevice;

  clockid_t clk_id = libPtpDaemon->mClockId;
  libPtpDaemon->mClockId = -1;
  ASSERT_TRUE(0u == libPtpDaemon->getRealLocalTime(true));
  libPtpDaemon->mClockId = clk_id;

  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));
  libPtpDaemon->mAvgCoeff = 0.0;
  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));
}

TEST_F(IasTestLibPtpDaemon, GetTsc)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  (void)libPtpDaemon->getTsc();
  ASSERT_TRUE(1);
}

TEST_F(IasTestLibPtpDaemon, triggerStorePersistenceData)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  (void)libPtpDaemon->triggerStorePersistenceData();

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init());
  ASSERT_TRUE(libPtpDaemon->isPtpReady());
  (void)libPtpDaemon->triggerStorePersistenceData();

  ASSERT_TRUE(1);
}

TEST_F(IasTestLibPtpDaemon, sysToPtp)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  uint64_t sysTime = libPtpDaemon->mLastTsc + 1u;
  double factor = libPtpDaemon->mTscToLocalFactor;
  uint64_t offset2 = libPtpDaemon->mLastTime;
  uint64_t ptpTime = libPtpDaemon->sysToPtp(sysTime);
  uint64_t resTime = uint64_t(factor) + offset2;
  ASSERT_EQ(resTime, ptpTime);
}

TEST_F(IasTestLibPtpDaemon, Init_raw)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 1000000000);
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  ASSERT_TRUE(NULL != igbDevice);

  libPtpDaemon->cleanUp();
  ASSERT_TRUE(1);

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));
}

TEST_F(IasTestLibPtpDaemon, Init_raw_rev2)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 2);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 1000000000);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXtstampThresh, 1000000000);
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  ASSERT_TRUE(NULL != igbDevice);

  libPtpDaemon->cleanUp();
  ASSERT_TRUE(1);

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));
}

TEST_F(IasTestLibPtpDaemon, DetectTscFreq_raw)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  ASSERT_EQ(eIasAvbProcErr, libPtpDaemon->detectTscFreq()); // no support for desktop CPUs
}

TEST_F(IasTestLibPtpDaemon, GetLocalTime_raw)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 1000000000);
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));
  libPtpDaemon->mRawXtstampEn = 2;
  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));
}

TEST_F(IasTestLibPtpDaemon, check_invalid_clockid)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 1000000000);
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));

  libPtpDaemon->mClockId = -1;
  ASSERT_TRUE(0u == libPtpDaemon->getRealLocalTime(true));
  ASSERT_EQ(eIasAvbProcInitializationFailed, libPtpDaemon->calculateConversionCoeffs());
}

TEST_F(IasTestLibPtpDaemon, calculateConversionCoeffs_raw_rev2)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();
  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));

  libPtpDaemon->mRawXtstampEn = 2;
  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->calculateConversionCoeffs());
}

TEST_F(IasTestLibPtpDaemon, invalid_device_path)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);

  mEnvironment->setConfigValue(IasRegKeys::cNwIfPtpDev, "");
  ASSERT_EQ(eIasAvbProcInitializationFailed, libPtpDaemon->init());
}

TEST_F(IasTestLibPtpDaemon, adapt_clock_settings)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawDeviationUnlock, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawRatioToPtp, 1);

  ASSERT_EQ(eIasAvbProcErr, libPtpDaemon->init());
}

TEST_F(IasTestLibPtpDaemon, invalid_tsc_freq)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 0);
  ASSERT_EQ(eIasAvbProcErr, libPtpDaemon->init());
}

TEST_F(IasTestLibPtpDaemon, rawToPtp_xtstamp_enabled)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();
  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 1000000);

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init());
  (void)libPtpDaemon->rawToPtp(0);

  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));
}

TEST_F(IasTestLibPtpDaemon, GetRealLocalTime_raw)
{
  ASSERT_TRUE(NULL != libPtpDaemon);

  LocalSetup();

  mEnvironment->setConfigValue(IasRegKeys::cClkRawXTimestamp, 1);
  mEnvironment->setConfigValue(IasRegKeys::cClkRawTscFreq, 1000000);

  device_t* igbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();

  ASSERT_EQ(eIasAvbProcOK, libPtpDaemon->init(igbDevice));

  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));

  libPtpDaemon->mRawToLocalTstampThreshold = 1000000000;
  ASSERT_TRUE(0u != libPtpDaemon->getRealLocalTime(true));
}

TEST_F(IasTestLibPtpDaemon, ptpToSys_invalid_params)
{
  ASSERT_TRUE(NULL != libPtpDaemon);
  LocalSetup();

  libPtpDaemon->mTscToLocalFactor = 0.0;

  ASSERT_TRUE(0u == libPtpDaemon->ptpToSys(0));
}

} // namespace
