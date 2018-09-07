/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestLocalAudioBuffer.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_streamhandler/IasLocalAudioBuffer.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#define protected protected
#define private private

extern size_t heapSpaceLeft;
extern size_t heapSpaceInitSize;

using namespace IasMediaTransportAvb;

namespace IasMediaTransportAvb
{

class IasTestLocalAudioBuffer : public ::testing::Test
{
protected:
    IasTestLocalAudioBuffer()
    : mLocalAudioBuffer(NULL)
    , mEnvironment(NULL)
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  virtual ~IasTestLocalAudioBuffer()
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
    mLocalAudioBuffer = new IasLocalAudioBuffer();
  }

  virtual void TearDown()
  {
    delete mLocalAudioBuffer;
    mLocalAudioBuffer = NULL;

    ASSERT_TRUE(NULL != mEnvironment);
    mEnvironment->unregisterDltContexts();
    delete mEnvironment;
    mEnvironment = NULL;

    heapSpaceLeft = heapSpaceInitSize;
  }

  IasLocalAudioBuffer* mLocalAudioBuffer;
  IasAvbStreamHandlerEnvironment* mEnvironment;
};


} // namespace IasMediaTransportAvb



TEST_F(IasTestLocalAudioBuffer, CTor_DTor)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
}

TEST_F(IasTestLocalAudioBuffer, init)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 0;
  bool doAnalysis = false;
  IasAvbProcessingResult result = mLocalAudioBuffer->init(totalSize, doAnalysis);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestLocalAudioBuffer, reset)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t optimalFillLevel = 0;
  IasAvbProcessingResult result = mLocalAudioBuffer->reset(optimalFillLevel);
  ASSERT_EQ(eIasAvbProcOK, result);
}

TEST_F(IasTestLocalAudioBuffer, write)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);

  uint32_t totalSize = 64u;
  bool doAnalysis = false;
  IasLocalAudioBuffer::AudioData buffer[totalSize];
  uint32_t nrSamples = totalSize - 1u;
  IasAvbProcessingResult result = mLocalAudioBuffer->init(totalSize, doAnalysis);
  ASSERT_EQ(eIasAvbProcOK, result);
  uint32_t nrSamplesOut = mLocalAudioBuffer->write(buffer, nrSamples);
  ASSERT_EQ(nrSamplesOut, nrSamples);
}

TEST_F(IasTestLocalAudioBuffer, read)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 1u;
  bool doAnalysis = false;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  uint32_t nrSamples = totalSize + 1u;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];
  mLocalAudioBuffer->mReadIndex = 0u;
  mLocalAudioBuffer->mWriteIndex = 2u;

  ASSERT_EQ(2u, mLocalAudioBuffer->read(buffer, nrSamples));
}

TEST_F(IasTestLocalAudioBuffer, getFillLevel)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t fillLevel = mLocalAudioBuffer->getFillLevel();
  (void) fillLevel;
}

TEST_F(IasTestLocalAudioBuffer, getTotalSize)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = mLocalAudioBuffer->getTotalSize();
  (void) totalSize;
}

TEST_F(IasTestLocalAudioBuffer, HEAP_faild)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);

  IasAvbProcessingResult result = eIasAvbProcErr;

  heapSpaceLeft = 0;

  result = mLocalAudioBuffer->init(1, false);
  ASSERT_EQ(eIasAvbProcNotEnoughMemory, result);
}

TEST_F(IasTestLocalAudioBuffer, read_not_interleaved)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 1u;
  bool doAnalysis = false;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  uint32_t nrSamples = totalSize + 1u;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];
  mLocalAudioBuffer->mReadIndex = 0u;
  mLocalAudioBuffer->mWriteIndex = 2u;

  ASSERT_EQ(nrSamples, mLocalAudioBuffer->read(buffer, nrSamples
                                               , static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));

  nrSamples = totalSize;
  mLocalAudioBuffer->mReadIndex = 0u;
  mLocalAudioBuffer->mWriteIndex = 0u;

  ASSERT_EQ(0u, mLocalAudioBuffer->read(buffer, nrSamples
                                        , static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
}

TEST_F(IasTestLocalAudioBuffer, read_not_interleaved_branch)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 1u;
  bool doAnalysis = false;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  uint32_t nrSamples = totalSize + 1u;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];
  uint32_t readIndex = 0u;
  mLocalAudioBuffer->mReadIndex = readIndex;
  mLocalAudioBuffer->mWriteIndex = 1u;

  ASSERT_EQ(mLocalAudioBuffer->mWriteIndex - readIndex
             , mLocalAudioBuffer->read(buffer, nrSamples, static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
  ASSERT_EQ(nrSamples - (totalSize - readIndex), mLocalAudioBuffer->mReadIndex);

  nrSamples = totalSize;
  mLocalAudioBuffer->mReadIndex = 0u;
  ASSERT_EQ(nrSamples, mLocalAudioBuffer->read(buffer, nrSamples
             , static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
}

TEST_F(IasTestLocalAudioBuffer, write_not_interleaved)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);

  uint32_t totalSize = 64u;
  bool doAnalysis = false;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  IasLocalAudioBuffer::AudioData buffer[totalSize];
  uint32_t nrSamples = totalSize - 1u;
  ASSERT_EQ(nrSamples, mLocalAudioBuffer->write(buffer, nrSamples
             , sizeof(IasLocalAudioBuffer::AudioData)));
  ASSERT_EQ(nrSamples, mLocalAudioBuffer->mWriteIndex);

  uint32_t writeIdx = 1u;
  mLocalAudioBuffer->mWriteIndex = writeIdx;
  mLocalAudioBuffer->mReadIndex = 1u;
  nrSamples = totalSize;
  ASSERT_EQ(nrSamples - (writeIdx - mLocalAudioBuffer->mReadIndex) - 1u
             , mLocalAudioBuffer->write(buffer, nrSamples, sizeof(IasLocalAudioBuffer::AudioData)));
  ASSERT_EQ(writeIdx + totalSize - (writeIdx - mLocalAudioBuffer->mReadIndex) -1u, mLocalAudioBuffer->mWriteIndex);

  uint32_t readIdx = 3u;
  writeIdx = 6u;
  mLocalAudioBuffer->mWriteIndex = writeIdx;
  mLocalAudioBuffer->mReadIndex = readIdx;
  nrSamples = totalSize - 5u;
  uint32_t nrSamplesOut = nrSamples;
  // write method returns the number of the written samples
  ASSERT_EQ(nrSamplesOut, mLocalAudioBuffer->write(buffer, nrSamples, sizeof(IasLocalAudioBuffer::AudioData)));
}

TEST_F(IasTestLocalAudioBuffer, read_interleaved)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 1u;
  bool doAnalysis = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  uint32_t nrSamples = totalSize;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];
  mLocalAudioBuffer->mReadIndex = 0u;
  mLocalAudioBuffer->mWriteIndex = 2u;

  ASSERT_EQ(nrSamples, mLocalAudioBuffer->read(buffer, nrSamples
                                               , 2 * static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
  ASSERT_EQ(0u, mLocalAudioBuffer->mReadIndex);
  ASSERT_EQ(2u, mLocalAudioBuffer->mWriteIndex);
}

TEST_F(IasTestLocalAudioBuffer, readInterleavedSampleRead)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 2u;
  bool doAnalysis = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  uint32_t nrSamples = totalSize;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];

  mLocalAudioBuffer->mWriteIndex = 2u;
  mLocalAudioBuffer->mReadIndex = 1u;
  ASSERT_EQ(1u, mLocalAudioBuffer->read(buffer, nrSamples
                                                 , 2 * static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
}

TEST_F(IasTestLocalAudioBuffer, write_interleaved)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 3u;
  bool doAnalysis = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  mLocalAudioBuffer->mReadIndex = 2u;
  mLocalAudioBuffer->mWriteIndex = 2u;
  uint32_t nrSamples = 2u;
  IasLocalAudioBuffer::AudioData buffer[nrSamples * 2] = {0u, 0u, 0u, 0u};

  ASSERT_EQ(2u, mLocalAudioBuffer->write(buffer, nrSamples
                                               , 2 * static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
  ASSERT_EQ(1u, mLocalAudioBuffer->mWriteIndex);
}

TEST_F(IasTestLocalAudioBuffer, writeInterleaved)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 4u;
  bool doAnalysis = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  mLocalAudioBuffer->mReadIndex = 2u;
  mLocalAudioBuffer->mWriteIndex = 2u;
  uint32_t nrSamples = 2u;
  IasLocalAudioBuffer::AudioData buffer[8u] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

  mLocalAudioBuffer->mWriteIndex = 3u;
  ASSERT_EQ(2u, mLocalAudioBuffer->write(buffer, nrSamples
                                                 , 2 * static_cast<uint32_t>(sizeof(IasLocalAudioBuffer::AudioData))));
  ASSERT_EQ(1u, mLocalAudioBuffer->mWriteIndex);
}

TEST_F(IasTestLocalAudioBuffer, read_threshold)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 3u;
  bool doAnalysis = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  ASSERT_EQ(0u, mLocalAudioBuffer->getReadThreshold());
  ASSERT_EQ(eIasAvbProcInvalidParam, mLocalAudioBuffer->setReadThreshold(totalSize + 1u));
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->setReadThreshold(totalSize - 1u));
  ASSERT_EQ(totalSize - 1u, mLocalAudioBuffer->getReadThreshold());

  const uint32_t nrSamples = 1u;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];
  ASSERT_EQ(1u, mLocalAudioBuffer->write(buffer, nrSamples));
  ASSERT_FALSE(mLocalAudioBuffer->isReadReady());

  ASSERT_EQ(1u, mLocalAudioBuffer->write(buffer, nrSamples));
  ASSERT_TRUE(mLocalAudioBuffer->isReadReady());

  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->reset(0u));
  ASSERT_FALSE(mLocalAudioBuffer->isReadReady());
}

TEST_F(IasTestLocalAudioBuffer, get_monotonic_indexes)
{
  ASSERT_TRUE(NULL != mLocalAudioBuffer);
  uint32_t totalSize = 3u;
  bool doAnalysis = true;
  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->init(totalSize, doAnalysis));

  const uint32_t nrSamples = 1u;
  IasLocalAudioBuffer::AudioData buffer[nrSamples];
  for (uint32_t i = 0; i < (totalSize + 1u); i++)
  {
    ASSERT_EQ(i, mLocalAudioBuffer->getMonotonicReadIndex());
    ASSERT_EQ(i, mLocalAudioBuffer->getMonotonicWriteIndex());

    ASSERT_EQ(1u, mLocalAudioBuffer->write(buffer, nrSamples));
    ASSERT_EQ(1u, mLocalAudioBuffer->read(buffer, nrSamples));
  }

  ASSERT_EQ(eIasAvbProcOK, mLocalAudioBuffer->reset(0u));
  ASSERT_EQ(0u, mLocalAudioBuffer->getMonotonicReadIndex());
  ASSERT_EQ(0u, mLocalAudioBuffer->getMonotonicWriteIndex());

}
