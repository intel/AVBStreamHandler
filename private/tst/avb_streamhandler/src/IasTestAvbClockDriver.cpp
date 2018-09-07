/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
* @file IasTestAvbClockDriver.cpp
* @brief Test file for IasBrd2ClockDriver
* @date 2013
*/

#include "gtest/gtest.h"
#include "media_transport/avb_streamhandler_api/IasAvbClockDriverInterface.hpp"

using namespace IasMediaTransportAvb;

static const char regKey[] = "clockdriver.config.i2cdevice";

class IasTestAvbClockDriver : public ::testing::Test
{
protected:

  IasTestAvbClockDriver():
    clockDriver(NULL)
  {}

  virtual ~IasTestAvbClockDriver() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    clockDriver = &getIasAvbClockDriverInterfaceInstance();
  }

  virtual void TearDown()
  {
    clockDriver = NULL;
  }

  IasAvbClockDriverInterface* clockDriver;

  class IasRegistryQuery: public IasAvbRegistryQueryInterface
  {
  public:

    IasRegistryQuery():
      IasAvbRegistryQueryInterface(),
      mName(eHasNoName)
    {}
    virtual ~IasRegistryQuery(){}

    bool queryConfigValue(const std::string & key, uint64_t & value) const
    {
      (void) key;
      (void) value;
      return (eHasNoName == mName) ? false : true;
    }

    bool queryConfigValue(const std::string & key, std::string & value) const
    {
      if (key == std::string(regKey))
      {
        switch (mName)
        {
          case eHasName:
          {
            value = "/dev/null";
            break;
          }
          case eHasWrongName:
          {
            value = "/dev/null_wrong";
            break;
          }
          case eHasNoName:
          default:
            break;
        }
      }

      return (eHasNoName == mName) ? false : true;
    }

    enum eName
    {
      eHasName = 0,
      eHasWrongName, // it's needed to test wrong opening of clock device
      eHasNoName
    };

    eName mName;
  };

  IasRegistryQuery registryQuery;
};

TEST_F(IasTestAvbClockDriver, Init)
{
  ASSERT_TRUE(NULL != clockDriver);

  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  registryQuery.mName = IasRegistryQuery::eHasNoName;
  result = clockDriver->init(registryQuery);
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, result);

  registryQuery.mName = IasRegistryQuery::eHasWrongName;
  result = clockDriver->init(registryQuery);
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, result);

  registryQuery.mName = IasRegistryQuery::eHasName;
  result = clockDriver->init(registryQuery);
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, result);
}

TEST_F(IasTestAvbClockDriver, QueryConfigValue)
{
  ASSERT_TRUE(NULL != clockDriver);

  uint32_t value = 0;
  registryQuery.mName = IasRegistryQuery::eHasNoName;
  IasAvbRegistryQueryInterface& query = registryQuery;
  query.queryConfigValue("NoName", value);

  registryQuery.mName = IasRegistryQuery::eHasName;
  query.queryConfigValue("NoName", value);
}

TEST_F(IasTestAvbClockDriver, UpdateRelative)
{
  ASSERT_TRUE(NULL != clockDriver);

  IasAvbResult result = IasAvbResult::eIasAvbResultErr;

  registryQuery.mName = IasRegistryQuery::eHasWrongName;
  result = clockDriver->init(registryQuery);
  ASSERT_EQ(IasAvbResult::eIasAvbResultErr, result);

  clockDriver->updateRelative(0, 2.0);

  registryQuery.mName = IasRegistryQuery::eHasName;
  result = clockDriver->init(registryQuery);
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, result);

  clockDriver->updateRelative(0, 0.1);

  int repeat = 20;

  for (;repeat > 0; --repeat)
  {
    clockDriver->updateRelative(0, 0.1 * repeat);
  }
}
