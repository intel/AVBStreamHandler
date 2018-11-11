#include <stdlib.h>

#include "gtest/gtest.h"
#include "dlt/dlt.h"

#define private public
#define protected public
#include "avb_watchdog/IasSystemdWatchdogManager.hpp"
#include "avb_watchdog/IasWatchdogTimerRegistration.hpp"
#include "avb_watchdog/IasWatchdogInterface.hpp"
#include "avb_watchdog/IasWatchdogResult.hpp"
#undef protected
#undef private

namespace IasWatchdog {

class IasTestSystemdWatchdog : public ::testing::Test
{
  protected:
    IasTestSystemdWatchdog()
    : mDltContext()
    , mWatchdogTimerRegistration(nullptr)
    , mWdManager(nullptr)
    {}

  virtual ~IasTestSystemdWatchdog()
  {}

  virtual void SetUp()
  {
    mWdManager = new (std::nothrow) IasSystemdWatchdogManager(mDltContext);
    ASSERT_TRUE(mWdManager != nullptr);

    mWatchdogTimerRegistration = std::make_shared<IasWatchdog::IasWatchdogTimerRegistration>(mDltContext);
    ASSERT_TRUE(mWatchdogTimerRegistration != nullptr);
  }

  virtual void TearDown()
  {
    delete mWdManager;
    mWatchdogTimerRegistration = nullptr;
  }

  DltContext mDltContext;
  std::shared_ptr<IasWatchdogTimerRegistration> mWatchdogTimerRegistration;
  IasSystemdWatchdogManager *mWdManager;
};

TEST_F(IasTestSystemdWatchdog, init)
{
  ASSERT_EQ(setenv("WATCHDOG_USEC", "30000000", 1), 0);
  IasWatchdogResult result = mWdManager->init(mWatchdogTimerRegistration);
  EXPECT_EQ(result, IasMediaTransportAvb::IasResult::cOk);

  ASSERT_EQ(setenv("WATCHDOG_USEC", "99999", 1), 0);
  result = mWdManager->init(mWatchdogTimerRegistration);
  EXPECT_EQ(result, IasMediaTransportAvb::IasResult::cInitFailed);

  result = mWdManager->init(nullptr);
  EXPECT_EQ(result, IasMediaTransportAvb::IasResult::cParameterInvalid);

  ASSERT_EQ(unsetenv("WATCHDOG_USEC"), 0);
  result = mWdManager->init(mWatchdogTimerRegistration);
  EXPECT_EQ(result, IasMediaTransportAvb::IasResult::cInitFailed);
}

TEST_F(IasTestSystemdWatchdog, createWatchdog)
{
  IasWatchdogInterface *wdInterface = nullptr;
  wdInterface = mWdManager->createWatchdog();
  ASSERT_NE(wdInterface, nullptr);
  EXPECT_EQ(mWdManager->mWatchdogInterfaces.size(), 1);

  wdInterface = nullptr;
  wdInterface = mWdManager->createWatchdog(30000, "Odin");
  ASSERT_NE(wdInterface, nullptr);
  EXPECT_EQ(mWdManager->mWatchdogInterfaces.size(), 2);
}

TEST_F(IasTestSystemdWatchdog, destroyWatchdog)
{
  IasSystemdWatchdogManager *altWdManager = nullptr;
  altWdManager = new (std::nothrow) IasSystemdWatchdogManager(mDltContext);
  IasWatchdogInterface *wdIf_1, *wdIf_2;
  IasWatchdogResult res = IasMediaTransportAvb::IasResult::cOk;
  wdIf_1 = wdIf_2 = nullptr;

  wdIf_1 = mWdManager->createWatchdog(30000, "Milky Way");
  ASSERT_NE(wdIf_1, nullptr);

  wdIf_2 = altWdManager->createWatchdog(30000, "Andromeda");
  ASSERT_NE(wdIf_2, nullptr);

  res = mWdManager->destroyWatchdog(nullptr);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cParameterInvalid);

  res = mWdManager->destroyWatchdog(wdIf_2);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cObjectNotFound);

  res = mWdManager->destroyWatchdog(wdIf_1);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  res = altWdManager->destroyWatchdog(wdIf_2);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);
}

TEST_F(IasTestSystemdWatchdog, removeWatchdog)
{
  IasWatchdogInterface *wdIf = nullptr;
  IasWatchdogResult res = IasMediaTransportAvb::IasResult::cOk;

  wdIf = mWdManager->createWatchdog(30000, "Thor");
  ASSERT_NE(wdIf, nullptr);

  res = mWdManager->removeWatchdog(wdIf);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  res = mWdManager->removeWatchdog(wdIf);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cObjectNotFound);

  res = mWdManager->removeWatchdog(nullptr);
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cParameterInvalid);
}

TEST_F(IasTestSystemdWatchdog, getCurrentRawTime)
{
  uint64_t curr_timestamp = 0;

  curr_timestamp = mWdManager->getCurrentRawTime();
  EXPECT_NE(curr_timestamp, 0);
}

TEST_F(IasTestSystemdWatchdog, registerWatchdog)
{
  IasWatchdogInterface *wdIf = nullptr;
  IasWatchdogResult res = IasMediaTransportAvb::IasResult::cOk;

  wdIf = mWdManager->createWatchdog();
  ASSERT_NE(wdIf, nullptr);
  res = wdIf->registerWatchdog();
  EXPECT_EQ(res, IasWatchdogResult::cWatchdogNotPreconfigured);

  wdIf->setTimeout(30000);
  wdIf->setName("Eitri");
  res = wdIf->registerWatchdog();
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  res = wdIf->unregisterWatchdog();
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  res = wdIf->registerWatchdog(30000, "");
  EXPECT_EQ(res, IasWatchdogResult::cParameterInvalid);

  res = wdIf->registerWatchdog(0, "Sif");
  EXPECT_EQ(res, IasWatchdogResult::cParameterInvalid);

  res = wdIf->registerWatchdog(30000, "Sif");
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  res = wdIf->registerWatchdog(30000, "Sif");
  EXPECT_EQ(res, IasWatchdogResult::cAlreadyRegistered);
}


TEST_F(IasTestSystemdWatchdog, unregisterWatchdog)
{
  IasWatchdogInterface *wdIf = nullptr;
  IasWatchdogResult res = IasMediaTransportAvb::IasResult::cOk;

  wdIf = mWdManager->createWatchdog(30000, "Loki");
  ASSERT_NE(wdIf, nullptr);

  res = wdIf->unregisterWatchdog();
  EXPECT_EQ(res, IasWatchdogResult::cWatchdogUnregistered);

  res = wdIf->registerWatchdog();
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);
}

TEST_F(IasTestSystemdWatchdog, reset)
{
  IasWatchdogInterface *wdIf = nullptr;
  IasWatchdogResult res = IasMediaTransportAvb::IasResult::cOk;

  wdIf = mWdManager->createWatchdog(5000, "Loki");
  ASSERT_NE(wdIf, nullptr);

  res = wdIf->reset();
  EXPECT_EQ(res, IasWatchdogResult::cWatchdogUnregistered);

  res = wdIf->registerWatchdog();
  ASSERT_EQ(res, IasMediaTransportAvb::IasResult::cOk);
  res = wdIf->reset();
  ASSERT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  sleep(6);
  res = wdIf->reset();
  EXPECT_EQ(res, IasWatchdogResult::cTimedOut);
}

TEST_F(IasTestSystemdWatchdog, allWatchdogInterfacesValid)
{
  IasWatchdogInterface *wdIf_1, *wdIf_2;
  IasWatchdogResult res = IasMediaTransportAvb::IasResult::cOk;
  bool allValid = false;
  wdIf_1 = wdIf_2 = nullptr;

  wdIf_1 = mWdManager->createWatchdog(5000, "Hel");
  wdIf_2 = mWdManager->createWatchdog(5000, "Baldr");

  res = wdIf_1->registerWatchdog();
  ASSERT_EQ(res, IasMediaTransportAvb::IasResult::cOk);
  res = wdIf_2->registerWatchdog();
  ASSERT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  res = wdIf_1->reset();
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);
  res = wdIf_2->reset();
  EXPECT_EQ(res, IasMediaTransportAvb::IasResult::cOk);

  allValid = mWdManager->allWatchdogInterfacesValid();
  EXPECT_TRUE(allValid);

  sleep(6);

  allValid = mWdManager->allWatchdogInterfacesValid();
  EXPECT_FALSE(allValid);
}

}
