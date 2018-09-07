/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * IasTestDiaLogger.cpp
 *
 *  Created on: Jun 2, 2018
 */
#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasDiaLogger.hpp"
#include "avb_streamhandler/IasAvbDiagnosticPacket.hpp"
#undef private
#undef protected
#include "test_common/IasSpringVilleInfo.hpp"

using namespace IasMediaTransportAvb;

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

class IasTestDiaLogger: public ::testing::Test
{
  protected:
  IasTestDiaLogger():
    mDiaLogger(),
    mEnvironment(NULL)
  {}

  virtual ~IasTestDiaLogger() {}

  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

    mDiaLogger = new IasDiaLogger();
    mEnvironment = new IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    (void) dlt_enable_local_print();
  }

  virtual void TearDown()
  {
    delete mDiaLogger;
    mDiaLogger = NULL;

    mEnvironment->unregisterDltContexts();
    delete mEnvironment;
    mEnvironment = NULL;

    heapSpaceLeft = heapSpaceInitSize;
  }

  bool LocalSetup()
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
    return false;
  }

  IasDiaLogger * mDiaLogger;
  IasAvbStreamHandlerEnvironment * mEnvironment;
  DltContext mDltCtx;
};

TEST_F(IasTestDiaLogger, CTor_DTor)
{
  ASSERT_TRUE(NULL != mDiaLogger);
}

TEST_F(IasTestDiaLogger, cleanup)
{
  ASSERT_TRUE(NULL != mDiaLogger);
  mDiaLogger->cleanup();
}

TEST_F(IasTestDiaLogger, init)
{
  ASSERT_TRUE(NULL != mDiaLogger);

  heapSpaceLeft = sizeof(IasAvbDiagnosticPacket) - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mDiaLogger->init(mEnvironment));

  heapSpaceLeft = sizeof(IasAvbDiagnosticPacket) + sizeof(uint8_t) * IasAvbDiagnosticPacket::cPacketLength - 1;
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, mDiaLogger->init(mEnvironment));

  heapSpaceLeft = heapSpaceInitSize;
  // (NULL == socket) || (ioctl(*socket, SIOCGIFINDEX, &ifr) < 0)  (F || T)
  ASSERT_EQ(eIasAvbProcInitializationFailed, mDiaLogger->init(mEnvironment));

  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cTestingProfileEnable, true));

  ASSERT_EQ(eIasAvbProcOK, mDiaLogger->init(mEnvironment));
}

TEST_F(IasTestDiaLogger, triggerTalkerMediaReadyPacket_badSocket)
{
  ASSERT_TRUE(NULL != mDiaLogger);

  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cTestingProfileEnable, true));
  mEnvironment->mTestingProfileEnabled = true;
  ASSERT_EQ(eIasAvbProcOK, mDiaLogger->init(mEnvironment));

  mDiaLogger->triggerTalkerMediaReadyPacket(-1);
  const uint16_t * diagBuffer = reinterpret_cast<const uint16_t*>(mDiaLogger->mDiagnosticPacket->getBuffer()
                                                                + IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_TYPE);
  ASSERT_EQ(IasAvbDiagnosticPacket::cStreamOutputDescriptorType, ntohs(*diagBuffer));
}

TEST_F(IasTestDiaLogger, incSequenceNumber)
{
  ASSERT_TRUE(NULL != mDiaLogger);
  uint16_t oldSeqNo = mDiaLogger->mSequenceNumber;
  mDiaLogger->incSequenceNumber();
  ASSERT_EQ(++oldSeqNo, mDiaLogger->mSequenceNumber);
}

TEST_F(IasTestDiaLogger, incLinkDown)
{
  ASSERT_TRUE(NULL != mDiaLogger);
  uint16_t oldLinkDownCount = mDiaLogger->mLinkDownCount;
  mDiaLogger->incLinkDown();
  ASSERT_EQ(++oldLinkDownCount, mDiaLogger->mLinkDownCount);
}

TEST_F(IasTestDiaLogger, incRxCount)
{
  ASSERT_TRUE(NULL != mDiaLogger);
  uint16_t oldRxCount = mDiaLogger->mFramesRxCount;
  mDiaLogger->incRxCount();
  ASSERT_EQ(++oldRxCount, mDiaLogger->mFramesRxCount);
}

TEST_F(IasTestDiaLogger, incTxCount)
{
  ASSERT_TRUE(NULL != mDiaLogger);
  uint16_t oldTxCount = mDiaLogger->mFramesTxCount;
  mDiaLogger->incTxCount();
  ASSERT_EQ(++oldTxCount, mDiaLogger->mFramesTxCount);
}


TEST_F(IasTestDiaLogger, clearTxCount)
{
  ASSERT_TRUE(NULL != mDiaLogger);
  mDiaLogger->clearTxCount();
  ASSERT_EQ(0, mDiaLogger->mFramesTxCount);
}

TEST_F(IasTestDiaLogger, setTimestampField)
{
  ASSERT_TRUE(NULL != mDiaLogger);

  mDiaLogger->setTimestampField(0u);
}

TEST_F(IasTestDiaLogger, triggerAvbSyncPacket)
{
  ASSERT_TRUE(NULL != mDiaLogger);

  ASSERT_TRUE(LocalSetup());
  ASSERT_EQ(IasAvbResult::eIasAvbResultOk, mEnvironment->setConfigValue(IasRegKeys::cTestingProfileEnable, true));
  mEnvironment->mTestingProfileEnabled = true;
  ASSERT_EQ(eIasAvbProcOK, mDiaLogger->init(mEnvironment));

  mDiaLogger->triggerAvbSyncPacket(0u);
}
