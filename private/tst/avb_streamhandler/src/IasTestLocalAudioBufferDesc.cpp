/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestLocalAudioBufferDesc.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasLocalAudioBufferDesc.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#define protected protected
#define private private

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;

namespace IasMediaTransportAvb
{

class IasTestLocalAudioBufferDesc : public ::testing::Test
{
protected:
    IasTestLocalAudioBufferDesc()
    : mLocalAudioBufferDesc(NULL)
    , mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestLocalAudioBufferDesc()
  {
    DLT_UNREGISTER_APP();
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;

    mEnvironment = new (nothrow) IasAvbStreamHandlerEnvironment(DLT_LOG_INFO);
    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->registerDltContexts();

    dlt_enable_local_print();
    mLocalAudioBufferDesc = new IasLocalAudioBufferDesc(2u);
  }

  virtual void TearDown()
  {
    delete mLocalAudioBufferDesc;
    mLocalAudioBufferDesc = NULL;

    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->unregisterDltContexts();
    delete mEnvironment;
    mEnvironment = NULL;

    heapSpaceLeft = heapSpaceInitSize;
  }

  IasLocalAudioBufferDesc* mLocalAudioBufferDesc;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};


} // namespace IasMediaTransportAvb


TEST_F(IasTestLocalAudioBufferDesc, CTor_DTor)
{
  ASSERT_TRUE(NULL != mLocalAudioBufferDesc);
}

TEST_F(IasTestLocalAudioBufferDesc, peek)
{
  ASSERT_TRUE(NULL != mLocalAudioBufferDesc);
  (void) mLocalAudioBufferDesc->reset();

  struct IasLocalAudioBufferDesc::AudioBufferDesc desc;
  IasAvbProcessingResult result = eIasAvbProcErr;

  // empty
  result = mLocalAudioBufferDesc->dequeue(desc);
  ASSERT_EQ(eIasAvbProcErr, result);


  result = mLocalAudioBufferDesc->peek(desc);
  ASSERT_EQ(eIasAvbProcErr, result);

  result = mLocalAudioBufferDesc->peekX(desc, 1u);
  ASSERT_EQ(eIasAvbProcErr, result);

  // one entry
  memset(&desc, 0u, sizeof desc);
  (void) mLocalAudioBufferDesc->enqueue(desc);

  result = mLocalAudioBufferDesc->peek(desc);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(0u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peekX(desc, 1u);
  ASSERT_EQ(eIasAvbProcErr, result);

  // two entries (<= qSize)
  desc.bufIndex = 1u;
  (void) mLocalAudioBufferDesc->enqueue(desc);

  result = mLocalAudioBufferDesc->peek(desc);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(0u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peekX(desc, 1u);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(1u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peekX(desc, 2u);
  ASSERT_EQ(eIasAvbProcErr, result);

  // overflow (> qSize)
  desc.bufIndex = 2u;
  (void) mLocalAudioBufferDesc->enqueue(desc);

  result = mLocalAudioBufferDesc->peek(desc);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(1u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peekX(desc, 1u);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(2u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peekX(desc, 2u);
  ASSERT_EQ(eIasAvbProcErr, result);

  // after dequeieng
  result = mLocalAudioBufferDesc->dequeue(desc);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(1u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peek(desc);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(2u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peekX(desc, 1u);
  ASSERT_EQ(eIasAvbProcErr, result);

  // after dequeieng
  result = mLocalAudioBufferDesc->dequeue(desc);
  ASSERT_EQ(eIasAvbProcOK, result);
  ASSERT_EQ(2u, desc.bufIndex);

  result = mLocalAudioBufferDesc->peek(desc);
  ASSERT_EQ(eIasAvbProcErr, result);

  result = mLocalAudioBufferDesc->peekX(desc, 1u);
  ASSERT_EQ(eIasAvbProcErr, result);

  // emptry
  result = mLocalAudioBufferDesc->dequeue(desc);
  ASSERT_EQ(eIasAvbProcErr, result);
}

TEST_F(IasTestLocalAudioBufferDesc, reset_request)
{
  ASSERT_TRUE(NULL != mLocalAudioBufferDesc);

  (void) mLocalAudioBufferDesc->reset();
  ASSERT_TRUE(mLocalAudioBufferDesc->getResetRequest());
  ASSERT_FALSE(mLocalAudioBufferDesc->getResetRequest());
}

TEST_F(IasTestLocalAudioBufferDesc, get_mode_name)
{
  ASSERT_TRUE(NULL != mLocalAudioBufferDesc);

  typedef IasLocalAudioBufferDesc::AudioBufferDescMode AudioBufferDescMode;

  const AudioBufferDescMode modeList[] =
  {
      AudioBufferDescMode::eIasAudioBufferDescModeOff,
      AudioBufferDescMode::eIasAudioBufferDescModeFailSafe,
      AudioBufferDescMode::eIasAudioBufferDescModeHard,
      AudioBufferDescMode::eIasAudioBufferDescModeLast
  };

  const char* strModeNames[] =
  {
      "off",
      "fail-safe",
      "hard",
      "invalid"
  };

  for (uint32_t i = 0; i < sizeof(modeList) / sizeof(AudioBufferDescMode); i++)
  {
    const char* strModeName = mLocalAudioBufferDesc->getAudioBufferDescModeString(modeList[i]);
    ASSERT_TRUE(NULL != strModeName);
    ASSERT_EQ(0, strcmp(strModeName, strModeNames[i]));
  }

  uint32_t badMode = static_cast<uint32_t>(AudioBufferDescMode::eIasAudioBufferDescModeLast) + 1u;
  const char* strModeName = mLocalAudioBufferDesc->getAudioBufferDescModeString(static_cast<AudioBufferDescMode>(badMode));
  ASSERT_TRUE(NULL != strModeName);
  ASSERT_EQ(0, strcmp(strModeName, "invalid"));
}

TEST_F(IasTestLocalAudioBufferDesc, get_dlt_pt_warn_time)
{
  ASSERT_TRUE(NULL != mLocalAudioBufferDesc);

  uint64_t time = 1000000000u;

  mLocalAudioBufferDesc->setDbgPresentationWarningTime(time);
  ASSERT_EQ(time, mLocalAudioBufferDesc->getDbgPresentationWarningTime());
}

TEST_F(IasTestLocalAudioBufferDesc, get_alsa_rx_sync_start_mode)
{
  ASSERT_TRUE(NULL != mLocalAudioBufferDesc);

  const bool mode = true;

  mLocalAudioBufferDesc->setAlsaRxSyncStartMode(mode);
  ASSERT_EQ(mode, mLocalAudioBufferDesc->getAlsaRxSyncStartMode());
}
