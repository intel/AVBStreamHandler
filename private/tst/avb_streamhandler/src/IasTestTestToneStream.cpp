/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestTestToneStream.cpp
 * @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasTestToneStream.hpp"
#define protected protected
#define private private

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;

class IasTestTestToneStream : public ::testing::Test
{
protected:
    IasTestTestToneStream():
    mTestToneStream(NULL),
    mLocalAudioStream(NULL),
    mStreamId(1234u)
  {
  }

  virtual ~IasTestTestToneStream() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    heapSpaceLeft = heapSpaceInitSize;
    DLT_REGISTER_CONTEXT_LL_TS(mDltCtx,
                  "TEST",
                  "IasTestTestToneStream",
                  DLT_LOG_INFO,
                  DLT_TRACE_STATUS_OFF);
    mTestToneStream = new IasTestToneStream( mDltCtx, mStreamId );
    mLocalAudioStream = static_cast<IasLocalAudioStream*>(mTestToneStream);
  }

  virtual void TearDown()
  {
    delete mTestToneStream;
    mTestToneStream = NULL;
    mLocalAudioStream = NULL;

    heapSpaceLeft = heapSpaceInitSize;
    DLT_UNREGISTER_CONTEXT(mDltCtx);
  }

  IasTestToneStream* mTestToneStream;
  IasLocalAudioStream* mLocalAudioStream;
  uint16_t mStreamId;
  DltContext mDltCtx;
};

TEST_F(IasTestTestToneStream,  Create_Destroy)
{
  ASSERT_TRUE(mTestToneStream != NULL);
}

TEST_F(IasTestTestToneStream, Invalid_Init)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t numChannel = 0u;
  uint32_t sampleFrequency = 48000u;
  uint8_t  channelLayout = 0u;

  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->init(numChannel, sampleFrequency, channelLayout));
}

TEST_F(IasTestTestToneStream, Valid_Init)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t numChannel = 1u;
  uint32_t sampleFrequency = 48000u;
  uint8_t  channelLayout = 0u;

  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->init(numChannel, sampleFrequency, channelLayout));
}

TEST_F(IasTestTestToneStream, noInit_setParams)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t channel = 0u;
  uint32_t signalFrequency = 0u;
  int32_t level = -30;
  IasAvbTestToneMode mode = IasAvbTestToneMode::eIasAvbTestToneSine;
  int32_t userParam = 0;

  ASSERT_EQ(eIasAvbProcNotInitialized, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));
}

TEST_F(IasTestTestToneStream, valid_init_invalid_setParams)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t channel = 0u;
  uint32_t sampleFrequency = 48000u;
  uint32_t signalFrequency = 0u;
  int32_t level = -30;
  IasAvbTestToneMode mode = IasAvbTestToneMode::eIasAvbTestToneSine;
  int32_t userParam = 0;
  uint16_t numChannel = 1u;
  uint8_t channelLayout = 0u;

  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->init(numChannel, sampleFrequency, channelLayout));

  channel = 2u;
  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));

  channel = 1u;
  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));

  channel = 0u;

  // Force bad mode
  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(channel, signalFrequency, level, (IasAvbTestToneMode)-1, userParam));

  signalFrequency = sampleFrequency + 2;
  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));
}

TEST_F(IasTestTestToneStream, setParams_channel_index_equal_num_channels)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t channel = 1u;
  uint32_t sampleFrequency = 48000u;
  uint32_t signalFrequency = 24000u;
  int32_t level = -30;
  IasAvbTestToneMode mode = IasAvbTestToneMode::eIasAvbTestToneSine;
  int32_t userParam = 0;
  uint16_t numChannel = 1u;
  uint8_t channelLayout = 0u;

  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->init(numChannel, sampleFrequency, channelLayout));

  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));
}

TEST_F(IasTestTestToneStream, setParams)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t channel = 0u;
  uint32_t sampleFrequency = 48000u;
  uint32_t signalFrequency = 24000u;
  int32_t level = -30;
  IasAvbTestToneMode mode = IasAvbTestToneMode::eIasAvbTestToneSine;
  int32_t userParam = 0;
  uint16_t numChannel = 1u;
  uint8_t channelLayout = 0u;

  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->init(numChannel, sampleFrequency, channelLayout));

  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));

  mode = IasAvbTestToneMode::eIasAvbTestToneFile;
  // case IasAvbTestToneMode::eIasAvbTestToneFile
  ASSERT_EQ(eIasAvbProcUnsupportedFormat, mTestToneStream->setChannelParams(channel, signalFrequency, level, mode, userParam));
}

TEST_F(IasTestTestToneStream, ResetBuffers)
{
  ASSERT_TRUE(mTestToneStream != NULL);
  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->resetBuffers());
}

// ----------------------TESTS FOR LocalAudioStream INHERITed by TestToneStream --------------------------------------

TEST_F(IasTestTestToneStream, LOCAL_AUDIO_Cleanup)
{
  ASSERT_TRUE(mLocalAudioStream != NULL);

  mLocalAudioStream->cleanup();
  ASSERT_TRUE(1);
}

TEST_F(IasTestTestToneStream, LOCAL_AUDIO_WriteLocalAudioBuffer)
{
  ASSERT_TRUE(mLocalAudioStream != NULL);
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t channelIdx = 0;
  IasLocalAudioBuffer::AudioData* buffer = NULL;
  uint32_t bufferSize = 0;
  uint16_t samplesWritten = 0;
  uint32_t timeStamp = 0u;

  // check uninitialized state
  ASSERT_EQ(eIasAvbProcNotImplemented, mLocalAudioStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp));
}

TEST_F(IasTestTestToneStream, LOCAL_AUDIO_ReadLocalAudioBuffer)
{
  ASSERT_TRUE(mLocalAudioStream != NULL);
  ASSERT_TRUE(mTestToneStream != NULL);

  uint16_t channelIdx = 0;
  IasLocalAudioBuffer::AudioData* buffer = NULL;
  uint32_t bufferSize = 0;
  uint16_t samplesRead = 0;
  uint64_t timeStamp = 0u;

  // check uninitialized state
  ASSERT_EQ(eIasAvbProcNotInitialized, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp));

  uint16_t numChannels          = 2;
  uint32_t sampleFrequency  = 48000;
  uint8_t  channelLayout        = 0;

  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->init(numChannels, sampleFrequency, channelLayout));

  // check invalid params - numChannels == index (should fail due to index being 0 based)
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalAudioStream->readLocalAudioBuffer(numChannels, buffer, bufferSize, samplesRead, timeStamp));

  // check invalid params - buffer is NULL
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp));

  bufferSize = 1024;
  IasLocalAudioBuffer::AudioData bufTab[bufferSize];
  buffer = bufTab;

  // check invalid params - bufferSize is 0, but valid buffer.
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, 0, samplesRead, timeStamp));

  // This should provoke generateSineWave
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp));
  ASSERT_EQ(samplesRead, bufferSize);

  samplesRead = 0;
  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(0, 24000, -30, IasAvbTestToneMode::eIasAvbTestTonePulse, -1));
  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(0, 24000, -30, IasAvbTestToneMode::eIasAvbTestTonePulse, 101));
  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->setChannelParams(0, 24000, -30, IasAvbTestToneMode::eIasAvbTestTonePulse, 0));
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp));
  ASSERT_EQ(samplesRead, bufferSize);
  samplesRead = 0;

  ASSERT_EQ(eIasAvbProcInvalidParam, mTestToneStream->setChannelParams(0, 24000, -30, IasAvbTestToneMode::eIasAvbTestToneSawtooth, 0));
  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->setChannelParams(0, 24000, -30, IasAvbTestToneMode::eIasAvbTestToneSawtooth, -1));
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp));
  ASSERT_EQ(samplesRead, bufferSize);

  samplesRead = 0;
  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->setChannelParams(0, 24000, -30, IasAvbTestToneMode::eIasAvbTestToneSawtooth, 1));
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioStream->readLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesRead, timeStamp));
  ASSERT_EQ(samplesRead, bufferSize);

  samplesRead = 0;
}

TEST_F(IasTestTestToneStream, HEAP_fail_testing)
{
  ASSERT_TRUE(mTestToneStream != NULL);

  uint8_t channelLayout = 0;
  uint16_t numChannels = 2;
  uint32_t sampleFrequency = 48000;

  // Call with Zero memory.
  size_t heapSpace = 0;
  heapSpaceLeft = heapSpace;

  // This should pass as test tone does not allocate a buffer due to having 0 totalsize.
  ASSERT_EQ(eIasAvbProcOK, mTestToneStream->init(numChannels, sampleFrequency, channelLayout));
}

TEST_F(IasTestTestToneStream, writeLocalAudioBuffer)
{

  uint16_t channelIdx = 0;
  IasLocalAudioBuffer::AudioData *buffer = NULL;
  uint32_t bufferSize = 0;
  uint16_t samplesWritten = 0;
  uint32_t timeStamp = 0u;

  ASSERT_EQ(eIasAvbProcNotImplemented, mTestToneStream->writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timeStamp));
}
