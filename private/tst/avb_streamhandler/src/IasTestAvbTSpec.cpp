/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestAvbTSpec.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbTSpec.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private


using namespace IasMediaTransportAvb;

class IasTestAvbTSpec : public ::testing::Test
{
protected:
  IasTestAvbTSpec() :
    mTSpec(NULL),
    mEnvironment(NULL)
  {
  }

  virtual ~IasTestAvbTSpec() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    mTSpec = new IasAvbTSpec(IasTestAvbTSpec::cMaxFrameSize, IasAvbSrClass::eIasAvbSrClassHigh);
  }

  virtual void TearDown()
  {
    delete mEnvironment;
    mEnvironment = NULL;

    delete mTSpec;
    mTSpec = NULL;
  }

  IasAvbTSpec *mTSpec;
  static const uint16_t cMaxFrameSize;
  IasAvbStreamHandlerEnvironment *mEnvironment;
};

const uint16_t IasTestAvbTSpec::cMaxFrameSize = 256;


TEST_F(IasTestAvbTSpec, CTor_DTor)
{
  ASSERT_TRUE(mTSpec != NULL);
}

TEST_F(IasTestAvbTSpec, Getters_Setters)
{
  ASSERT_TRUE(mTSpec != NULL);
  IasAvbSrClass srClassHigh = IasAvbSrClass::eIasAvbSrClassHigh;
  IasAvbSrClass srClassLow  = IasAvbSrClass::eIasAvbSrClassLow;

  //
  // Vlan PRIO
  //
  uint64_t val = 0u;
  if (mEnvironment->getConfigValue("tspec.vlanprio.low", val) && (val != 0u))
    ASSERT_EQ((uint8_t)val, mTSpec->getVlanPrioritybyClass(IasAvbSrClass::eIasAvbSrClassLow));
  else
    ASSERT_EQ(mTSpec->mPrioTable[srClassLow], mTSpec->getVlanPrioritybyClass(srClassLow));

  val = 0u;
  if (mEnvironment->getConfigValue("tspec.vlanprio.high", val) && (val != 0u))
  {
    ASSERT_EQ((uint8_t)val, mTSpec->getVlanPrioritybyClass(srClassHigh));
    ASSERT_EQ((uint8_t)val, mTSpec->getVlanPriority());
  }
  else
  {
    ASSERT_EQ(mTSpec->mPrioTable[srClassHigh], mTSpec->getVlanPrioritybyClass(srClassHigh));
    ASSERT_EQ(mTSpec->mPrioTable[mTSpec->mClass], mTSpec->getVlanPriority());
  }
  //
  // Vlan ID
  //
  val = 0u;
  if (mEnvironment->getConfigValue("tspec.vlanid.low", val) && (val != 0u))
    ASSERT_EQ((uint16_t)val, mTSpec->getVlanIdbyClass(srClassLow));
  else
    ASSERT_EQ(mTSpec->mIdTable[srClassLow], mTSpec->getVlanIdbyClass(srClassLow));

  val = 0u;
  if (mEnvironment->getConfigValue("tspec.vlanid.high", val) && (val != 0u))
  {
    ASSERT_EQ((uint16_t)val, mTSpec->getVlanIdbyClass(srClassHigh));
    ASSERT_EQ((uint16_t)val, mTSpec->getVlanId());
  }
  else
  {
    ASSERT_EQ(mTSpec->mIdTable[srClassHigh], mTSpec->getVlanIdbyClass(srClassHigh));
    ASSERT_EQ(mTSpec->mIdTable[mTSpec->mClass], mTSpec->getVlanId());
  }

  //
  // Packets per second
  //
  val = 0u;
  if (mEnvironment->getConfigValue("tspec.interval.low", val) && (val != 0u))
  {
    uint32_t intervalLow = uint32_t(uint64_t(1e9) / val);
    ASSERT_EQ(intervalLow, IasAvbTSpec::getPacketsPerSecondByClass(srClassLow));
  }
  else
  {
    uint32_t intervalLow = uint32_t(uint64_t(1e9) / mTSpec->mClassMeasurementTimeTable[srClassLow]);
    ASSERT_EQ(intervalLow, IasAvbTSpec::getPacketsPerSecondByClass(srClassLow));
  }

  val = 0u;
  if (mEnvironment->getConfigValue("tspec.interval.high", val) && (val != 0u))
  {
    uint32_t intervalHigh = (uint32_t(uint64_t(1e9) / val));
    ASSERT_EQ(intervalHigh, mTSpec->IasAvbTSpec::getPacketsPerSecondByClass(srClassHigh));

    uint32_t packetsPerSecond = mTSpec->mMaxIntervalFrames * intervalHigh;
    ASSERT_EQ(packetsPerSecond, mTSpec->getPacketsPerSecond());
  }
  else
  {
    uint32_t intervalHigh = uint32_t(uint64_t(1e9) / mTSpec->mClassMeasurementTimeTable[srClassHigh]);
    ASSERT_EQ(intervalHigh, mTSpec->getPacketsPerSecondByClass(srClassHigh));

    intervalHigh = uint32_t(uint64_t(1e9) / mTSpec->mClassMeasurementTimeTable[mTSpec->mClass]);
    ASSERT_EQ(intervalHigh, mTSpec->getPacketsPerSecond());
  }

  ASSERT_EQ(mTSpec->mMaxIntervalFrames, mTSpec->getMaxIntervalFrames());
  mTSpec->setMaxIntervalFrames(2u);
  ASSERT_EQ(mTSpec->mMaxIntervalFrames, mTSpec->getMaxIntervalFrames());
  ASSERT_EQ(IasTestAvbTSpec::cMaxFrameSize, mTSpec->getMaxFrameSize());
  ASSERT_EQ(IasAvbSrClass::eIasAvbSrClassHigh, mTSpec->getClass());
  ASSERT_EQ(0u, mTSpec->getVlanPrioritybyClass((IasAvbSrClass)5));
  ASSERT_EQ(0u, mTSpec->getVlanIdbyClass((IasAvbSrClass)5));
  ASSERT_EQ(0u, IasAvbTSpec::getPacketsPerSecondByClass((IasAvbSrClass)5));
  ASSERT_EQ("<UNKNOWN>", IasAvbTSpec::getClassSuffix((IasAvbSrClass)5));
}

TEST_F(IasTestAvbTSpec, getRequiredBandwidth)
{
  delete mTSpec;
  /* AVB header + (sample size * channels * samples per channel per packet) */
  const uint16_t maxFrameSize = 24u + (2u * 2u * 6u); // = 48 --> 2 bytes (SAF), 2 channels, 6 samples per channel per packet (Class A/48kHz)
  mTSpec = new IasAvbTSpec(maxFrameSize, IasAvbSrClass::eIasAvbSrClassHigh);
  ASSERT_TRUE(mTSpec != NULL);

  ASSERT_EQ(mTSpec->getRequiredBandwidth(), 5824u);
}

TEST_F(IasTestAvbTSpec, getRequiredBandwidth_2)
{
  delete mTSpec;
  /* AVB header + (sample size * channels * samples per channel per packet) */
  const uint16_t maxFrameSize = 24u + (2u * 2u * 3u); // = 36 --> 2 bytes (SAF), 2 channels, 3 samples per channel per packet (Class A/24kHz)
  mTSpec = new IasAvbTSpec(maxFrameSize, IasAvbSrClass::eIasAvbSrClassHigh);
  ASSERT_TRUE(mTSpec != NULL);

  ASSERT_EQ(mTSpec->getRequiredBandwidth(), 5440u);
}
