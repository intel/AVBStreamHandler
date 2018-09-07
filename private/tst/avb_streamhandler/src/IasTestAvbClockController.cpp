/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbClockController.cpp
 *  @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbClockController.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAvbPtpClockDomain.hpp"
#include "avb_streamhandler/IasAvbSwClockDomain.hpp"
#include "avb_streamhandler/IasAvbHwCaptureClockDomain.hpp"
#undef protected
#undef private

#include "test_common/IasSpringVilleInfo.hpp"



namespace IasMediaTransportAvb
{

class IasTestAvbPCDProtected : public IasAvbPtpClockDomain
{
  public:
  IasTestAvbPCDProtected() {}
  ~IasTestAvbPCDProtected() {}

  void updateRateRatio(float newRatio)
  {
    IasAvbClockDomain::updateRateRatio(newRatio);
  }

  virtual void lockStateChanged()
  {
    IasAvbClockDomain::lockStateChanged();
  }


};


class IasTestAvbClockController : public ::testing::Test
{
  public:
  IasTestAvbClockController() :
   mClockController(NULL)
  ,mEnvironment(NULL)
  ,mPtpClockDomain(NULL)
  ,mSwClockDomain(NULL)
  ,mPcdProtected(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestAvbClockController()
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

    mClockController = new (nothrow) IasAvbClockController;
  }

  virtual void TearDown()
  {
    delete mClockController;
    mClockController = NULL;

    delete mPtpClockDomain;
    mPtpClockDomain = NULL;

    delete mSwClockDomain;
    mSwClockDomain = NULL;

    delete mPcdProtected;
    mPcdProtected = NULL;

    if (NULL != mEnvironment)
    {
      mEnvironment->unregisterDltContexts();
      delete mEnvironment;
      mEnvironment = NULL;
    }
  }

  protected:
  bool startEnvironment()
  {
    if (mEnvironment)
    {
      mEnvironment->setDefaultConfigValues();

      if (IasSpringVilleInfo::fetchData())
      {
        IasSpringVilleInfo::printDebugInfo();

        if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cNwIfName, IasSpringVilleInfo::getInterfaceName()))
        {
          std::string driverFileName("libias-media_transport-avb_clockdriver.so");

          if (IasAvbResult::eIasAvbResultOk == mEnvironment->setConfigValue(IasRegKeys::cClockDriverFileName, driverFileName))
          {
            if (eIasAvbProcOK == mEnvironment->loadClockDriver(driverFileName))
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
      }
    }
    return false;
  }

  IasAvbResult setConfigValue(const std::string& key, uint64_t& value)
  {
    return mEnvironment->setConfigValue(key, value);
  }

  IasAvbClockController *mClockController;
  IasAvbStreamHandlerEnvironment* mEnvironment;
  IasAvbPtpClockDomain *mPtpClockDomain;
  IasAvbSwClockDomain *mSwClockDomain;
  IasTestAvbPCDProtected *mPcdProtected;
};

TEST_F(IasTestAvbClockController, notifyUpdateLockState)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);
  IasAvbClockDomain * nullDomain = NULL;

  mClockController->notifyUpdateLockState(nullDomain);

  mPtpClockDomain = new (nothrow) IasAvbPtpClockDomain;
  ASSERT_TRUE(NULL != mPtpClockDomain);

  mClockController->notifyUpdateLockState(mPtpClockDomain);

  mClockController->mMaster = mPtpClockDomain;
  mClockController->mLockState = IasAvbClockController::eLocked;
  mPtpClockDomain->mLockState = IasAvbClockDomain::eIasAvbLockStateInit;
  // domain == mMaster
  // (newState < IasAvbClockDomain::eIasAvbLockStateLocked) (T)
  // && (mLockState > eUnlocked)                            (&& T)
  mClockController->notifyUpdateLockState(mPtpClockDomain);

  mPtpClockDomain->mLockState = (IasAvbClockDomain::IasAvbLockState)5;
  // domain == mMaster
  // (newState < IasAvbClockDomain::eIasAvbLockStateLocked) (F)
  // && (mLockState > eUnlocked)                            (&& N/A)
  mClockController->notifyUpdateLockState(mPtpClockDomain);
}

TEST_F(IasTestAvbClockController, CTor_DTor)
{
  ASSERT_TRUE(NULL != mClockController);
}

TEST_F(IasTestAvbClockController, cleanup_no_init)
{
  ASSERT_TRUE(NULL != mClockController);
  mClockController->cleanup();
}

TEST_F(IasTestAvbClockController, init_with_NULLs)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_EQ(eIasAvbProcInvalidParam, mClockController->init(NULL, NULL, 0));

  IasAvbPtpClockDomain ptpClockDomain;
  ASSERT_EQ(eIasAvbProcInvalidParam, mClockController->init(&ptpClockDomain, NULL, 0));
  ASSERT_EQ(eIasAvbProcInvalidParam, mClockController->init(NULL, &ptpClockDomain, 0));
}

TEST_F(IasTestAvbClockController, init_no_SH_environment)
{
  ASSERT_TRUE(NULL != mClockController);
  IasAvbPtpClockDomain ptpClockDomain;
  ASSERT_EQ(eIasAvbProcInitializationFailed, mClockController->init(&ptpClockDomain, &ptpClockDomain, 0));
}

TEST_F(IasTestAvbClockController, init_env_and_same_master_slave)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);

  mPtpClockDomain = new (nothrow) IasAvbPtpClockDomain;
  ASSERT_TRUE(NULL != mPtpClockDomain);

  ASSERT_TRUE(startEnvironment());

  ASSERT_EQ(eIasAvbProcAlreadyInUse, mClockController->init(mPtpClockDomain, mPtpClockDomain, 0));
  sleep(1);
}

TEST_F(IasTestAvbClockController, init_env_and_same_master_slave_with_limits)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);
  ASSERT_TRUE(NULL == mSwClockDomain);

  mPtpClockDomain = new (nothrow) IasAvbPtpClockDomain;
  ASSERT_TRUE(NULL != mPtpClockDomain);
  mSwClockDomain = new (nothrow) IasAvbSwClockDomain;
  ASSERT_TRUE(NULL != mSwClockDomain);

  uint64_t val = 1;
  uint64_t waitVal = 999u;

  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlUpperLimit, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlLowerLimit, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlHoldOff, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlGain, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlCoeff1, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlCoeff2, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlCoeff3, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlCoeff4, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlLockCount, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlLockThres, val));
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlWaitInterval, waitVal));
  ASSERT_TRUE(startEnvironment());

  ASSERT_EQ(eIasAvbProcOK, mClockController->init(mPtpClockDomain, mSwClockDomain, 0));
  sleep(1);
}

TEST_F(IasTestAvbClockController, init_env_and_diff_master_slave_mix_ratios)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);
  ASSERT_TRUE(NULL == mSwClockDomain);

  mPcdProtected = new (nothrow) IasTestAvbPCDProtected;
  ASSERT_TRUE(NULL != mPcdProtected);
  mSwClockDomain = new (nothrow) IasAvbSwClockDomain;
  ASSERT_TRUE(NULL != mSwClockDomain);

  uint64_t val = 0;
  setConfigValue(IasRegKeys::cClockCtrlUpperLimit, val);
  setConfigValue(IasRegKeys::cClockCtrlLowerLimit, val);
  ASSERT_TRUE(startEnvironment());

  mPcdProtected->updateRateRatio(float(-1.0));
  mPcdProtected->updateRateRatio(float(10.1));
  mPcdProtected->updateRateRatio(10.0f);

  ASSERT_EQ(eIasAvbProcOK, mClockController->init(mPcdProtected, mSwClockDomain, 0));
  sleep(1);
}

TEST_F(IasTestAvbClockController, init_env_and_diff_master_slave)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);
  ASSERT_TRUE(NULL == mSwClockDomain);

  mPtpClockDomain = new (nothrow) IasAvbPtpClockDomain;
  ASSERT_TRUE(NULL != mPtpClockDomain);
  mSwClockDomain = new (nothrow) IasAvbSwClockDomain;
  ASSERT_TRUE(NULL != mSwClockDomain);

  ASSERT_TRUE(startEnvironment());
  uint64_t val = 0u;
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, setConfigValue(IasRegKeys::cClockCtrlEngage, val));

  ASSERT_EQ(eIasAvbProcOK, mClockController->init(mPtpClockDomain, mSwClockDomain, 0));
  sleep(1);
}

TEST_F(IasTestAvbClockController, notifyUpdateLockState_branch)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);

  ASSERT_TRUE(startEnvironment());

  mPtpClockDomain = new (nothrow) IasAvbPtpClockDomain;
  ASSERT_TRUE(NULL != mPtpClockDomain);
  mSwClockDomain = new (nothrow) IasAvbSwClockDomain;
  ASSERT_TRUE(NULL != mSwClockDomain);
  mPcdProtected = new (nothrow) IasTestAvbPCDProtected;
  ASSERT_TRUE(NULL != mPcdProtected);
  ASSERT_EQ(eIasAvbProcOK, mClockController->init(mPtpClockDomain, mSwClockDomain, 0));
  sleep(1);

  mClockController->notifyUpdateLockState(mPtpClockDomain);

  mSwClockDomain->mLockState = IasAvbClockDomain::IasAvbLockState::eIasAvbLockStateLocking;

  mClockController->notifyUpdateLockState(mSwClockDomain);

  mClockController->mLockState = IasAvbClockController::LockState::eLocked;

  mClockController->notifyUpdateLockState(mSwClockDomain);
  mClockController->notifyUpdateLockState(mPcdProtected);
}

TEST_F(IasTestAvbClockController, notifyUpdateRatio)
{
  ASSERT_TRUE(NULL != mClockController);
  ASSERT_TRUE(NULL == mPtpClockDomain);

  mPtpClockDomain = new (nothrow) IasAvbPtpClockDomain;
  ASSERT_TRUE(NULL != mPtpClockDomain);

  mClockController->mSlave = mPtpClockDomain;
  IasAvbClockDomain * nullDomain = NULL;

  mClockController->notifyUpdateRatio(nullDomain);
}


} // namespace IasMediaTransportAvb
