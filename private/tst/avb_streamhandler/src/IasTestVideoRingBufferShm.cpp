/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestVideoRingBufferShm.cpp
 * @brief   The implementation of the IasTestVideoRingBufferShm test class.
 * @date    2018
 */

#include "gtest/gtest.h"
#define private public
#define protected public
#include "avb_video_common/IasAvbVideoRingBufferShm.hpp"
#define protected protected
#define private private

namespace IasMediaTransportAvb
{

#define NSEC_PER_SEC 1000000000UL
#define NOW_THRESHOLD 1000

class IasTestVideoRingBufferShm : public ::testing::Test
{
protected:
  IasTestVideoRingBufferShm()
    : mRingBuffer()
  {
    DLT_REGISTER_APP("IAAS", "AVB Streamhandler");
  }

  ~IasTestVideoRingBufferShm()
  {
    DLT_UNREGISTER_APP();
  }

  void SetUp() override
  {
    mRingBuffer.init(cPacketSize, cNumPackets, (void *)&mBuffer, true);
  }

  static const uint32_t cNumPackets = 800u;
  static const uint32_t cPacketSize = 1460u;
  uint8_t mBuffer[cNumPackets * cPacketSize];
  IasAvbVideoRingBufferShm mRingBuffer;
};

TEST_F(IasTestVideoRingBufferShm, addReader)
{
  // Invalid params
  IasVideoRingBufferResult result = mRingBuffer.addReader(-1);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.addReader(0);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  for (int i = 0; i < cIasVideoRingBufferShmMaxReaders; i++)
  {
    result = mRingBuffer.addReader(i + 1); // Smallest accepted pid is 1
    EXPECT_EQ(result, eIasRingBuffOk);
  }

  // To many readers
  result = mRingBuffer.addReader(1);
  EXPECT_EQ(result, eIasRingBuffTooManyReaders);
}

TEST_F(IasTestVideoRingBufferShm, removeReader)
{
  // Invalid params
  IasVideoRingBufferResult result = mRingBuffer.removeReader(-1);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.removeReader(0);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Remove one that wasn't added
  result = mRingBuffer.removeReader(1);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Add as many readers as possible
  for (int i = 0; i < cIasVideoRingBufferShmMaxReaders; i++)
  {
    result = mRingBuffer.addReader(i + 1); // Smallest accepted pid is 1
    ASSERT_EQ(result, eIasRingBuffOk);
  }

  // Then remove some on an unspecified order
  result = mRingBuffer.removeReader(1);
  EXPECT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.removeReader(7);
  EXPECT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.removeReader(cIasVideoRingBufferShmMaxReaders);
  EXPECT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.removeReader(cIasVideoRingBufferShmMaxReaders - 1);
  EXPECT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.removeReader(2);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Five were removed, so we should be able to add five more
  for (int i = 0; i < 5; i++)
  {
    result = mRingBuffer.addReader(i + 100);
    ASSERT_EQ(result, eIasRingBuffOk);
  }

  // But nothing more
  result = mRingBuffer.addReader(200);
  EXPECT_EQ(result, eIasRingBuffTooManyReaders);

  // Remove something after adding
  result = mRingBuffer.removeReader(5);
  EXPECT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.removeReader(100);
  EXPECT_EQ(result, eIasRingBuffOk);
}

TEST_F(IasTestVideoRingBufferShm, findReader)
{
  // Add some readers
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(2);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(3);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(4);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(5);
  ASSERT_EQ(result, eIasRingBuffOk);

  // Find them
  EXPECT_NE(mRingBuffer.findReader(1), nullptr);
  EXPECT_NE(mRingBuffer.findReader(3), nullptr);
  EXPECT_NE(mRingBuffer.findReader(2), nullptr);
  EXPECT_NE(mRingBuffer.findReader(4), nullptr);
  EXPECT_NE(mRingBuffer.findReader(5), nullptr);

  // Do not find what wasn't there
  EXPECT_EQ(mRingBuffer.findReader(-1), nullptr);
  EXPECT_EQ(mRingBuffer.findReader(0), nullptr);
  EXPECT_EQ(mRingBuffer.findReader(6), nullptr);

  // Remove some
  result = mRingBuffer.removeReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.removeReader(3);
  ASSERT_EQ(result, eIasRingBuffOk);

  // Do not find those
  EXPECT_EQ(mRingBuffer.findReader(1), nullptr);
  EXPECT_EQ(mRingBuffer.findReader(3), nullptr);

  // But still find what wasn't removed
  EXPECT_NE(mRingBuffer.findReader(2), nullptr);
  EXPECT_NE(mRingBuffer.findReader(4), nullptr);
  EXPECT_NE(mRingBuffer.findReader(5), nullptr);
}

TEST_F(IasTestVideoRingBufferShm, calculateReaderBufferLevel)
{
  // Add a reader
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);

  // "reader buffer level" is the amount of packets non read by
  // some reader. It's usually the difference between was written
  // by the writer, and what was read by the reader
  // Each reader knows how much it read so far, so the only thing
  // to test it check expectations regarding writer position (what it
  // has written)

  IasAvbVideoRingBufferShm::RingBufferReader *reader = mRingBuffer.findReader(1);
  ASSERT_NE(reader, nullptr);

  // In the beginning, reader read nothing, and writer wrote
  // nothing
  mRingBuffer.mWriteOffset = 0;
  reader->offset = 0;
  // So buffer level should be 0
  EXPECT_EQ(mRingBuffer.calculateReaderBufferLevel(reader), 0);

  // After some writing it should be all that was written
  mRingBuffer.mWriteOffset = 400;
  EXPECT_EQ(mRingBuffer.calculateReaderBufferLevel(reader), 400);

  // Some reading, and level should decrease by what was read
  reader->offset = 300;
  EXPECT_EQ(mRingBuffer.calculateReaderBufferLevel(reader), 100);

  // Writer goes to the end and so wraps to 0.
  mRingBuffer.mWriteOffset = 0;
  EXPECT_EQ(mRingBuffer.calculateReaderBufferLevel(reader), 500);

  // Writer advances a bit
  mRingBuffer.mWriteOffset = 100;
  EXPECT_EQ(mRingBuffer.calculateReaderBufferLevel(reader), 600);
}

TEST_F(IasTestVideoRingBufferShm, aggregateReaderOffset)
{
  // Add some readers
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(2);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(3);
  ASSERT_EQ(result, eIasRingBuffOk);

  IasAvbVideoRingBufferShm::RingBufferReader *reader1 = mRingBuffer.findReader(1);
  ASSERT_NE(reader1, nullptr);
  IasAvbVideoRingBufferShm::RingBufferReader *reader2 = mRingBuffer.findReader(2);
  ASSERT_NE(reader2, nullptr);
  IasAvbVideoRingBufferShm::RingBufferReader *reader3 = mRingBuffer.findReader(3);
  ASSERT_NE(reader3, nullptr);

  // No one read anything, so we're still on zero
  mRingBuffer.aggregateReaderOffset();
  EXPECT_EQ(mRingBuffer.mReadOffset, 0);

  // Some advance, but not all, so we're still on zero
  reader1->offset = 300;
  reader2->offset = 200;
  mRingBuffer.aggregateReaderOffset();
  EXPECT_EQ(mRingBuffer.mReadOffset, 0);

  // Now reader2 lags behind
  reader3->offset = 300;
  mRingBuffer.aggregateReaderOffset();
  EXPECT_EQ(mRingBuffer.mReadOffset, 200);

  // One more round of advancements
  reader1->offset = 600;
  reader2->offset = 500;
  reader3->offset = 700;
  mRingBuffer.aggregateReaderOffset();
  EXPECT_EQ(mRingBuffer.mReadOffset, 500);

  // Some reach the end, but not all
  reader1->offset = cNumPackets;
  reader2->offset = cNumPackets;
  mRingBuffer.aggregateReaderOffset();
  EXPECT_EQ(mRingBuffer.mReadOffset, 700);

  // When all reach the end, mReadOffset wraps to zero
  reader3->offset = cNumPackets;
  mRingBuffer.aggregateReaderOffset();
  EXPECT_EQ(mRingBuffer.mReadOffset, 0);
}

TEST_F(IasTestVideoRingBufferShm, updateSmallerReaderOffset)
{
  // This method is called by aggregateReaderOffset, so most of what
  // it does should be already tested. Here, we are interested in
  // a) Does it resets all readers offset when they reach the end,
  // b) Does it not reset anything before that

  // XXX: some C++ trickery prevents gtest functions (such as EXPECT_EQ)
  // from seeing this static member of IasAvbVideoRingBufferShm. Find
  // out a beautiful solution. For now, we use a local variable
  uint32_t numPackets = cNumPackets;

  // Before adding any reader, it should return UINT32_MAX
  EXPECT_EQ(mRingBuffer.updateSmallerReaderOffset(), UINT32_MAX);

  // Add some readers
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(2);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(3);
  ASSERT_EQ(result, eIasRingBuffOk);

  IasAvbVideoRingBufferShm::RingBufferReader *reader1 = mRingBuffer.findReader(1);
  ASSERT_NE(reader1, nullptr);
  IasAvbVideoRingBufferShm::RingBufferReader *reader2 = mRingBuffer.findReader(2);
  ASSERT_NE(reader2, nullptr);
  IasAvbVideoRingBufferShm::RingBufferReader *reader3 = mRingBuffer.findReader(3);
  ASSERT_NE(reader3, nullptr);

  // In the beginning, everything is at zero
  EXPECT_EQ(mRingBuffer.updateSmallerReaderOffset(), 0);
  EXPECT_EQ(reader1->offset, 0);
  EXPECT_EQ(reader1->offset, 0);
  EXPECT_EQ(reader1->offset, 0);

  // Advance them in assorted ways
  reader1->offset = 0;
  reader2->offset = 500;
  reader3->offset = 700;
  EXPECT_EQ(mRingBuffer.updateSmallerReaderOffset(), 0);
  EXPECT_EQ(reader1->offset, 0);
  EXPECT_EQ(reader2->offset, 500);
  EXPECT_EQ(reader3->offset, 700);

  // One more time
  reader1->offset = 600;
  reader2->offset = 700;
  reader3->offset = 750;
  EXPECT_EQ(mRingBuffer.updateSmallerReaderOffset(), 600);
  EXPECT_EQ(reader1->offset, 600);
  EXPECT_EQ(reader2->offset, 700);
  EXPECT_EQ(reader3->offset, 750);

  // One reaches the end
  reader1->offset = numPackets;
  reader2->offset = 750;
  reader3->offset = 770;
  EXPECT_EQ(mRingBuffer.updateSmallerReaderOffset(), 750);
  EXPECT_EQ(reader1->offset, numPackets);
  EXPECT_EQ(reader2->offset, 750);
  EXPECT_EQ(reader3->offset, 770);

  // When all reach, their offset goes to zero
  reader1->offset = numPackets;
  reader2->offset = numPackets;
  reader3->offset = numPackets;
  EXPECT_EQ(mRingBuffer.updateSmallerReaderOffset(), 800);
  EXPECT_EQ(reader1->offset, 0);
  EXPECT_EQ(reader2->offset, 0);
  EXPECT_EQ(reader3->offset, 0);
}

TEST_F(IasTestVideoRingBufferShm, updateAvailable)
{
  uint32_t numBuffers;
  uint32_t numPackets = cNumPackets;

  // Test some invalid params
  IasVideoRingBufferResult result = mRingBuffer.updateAvailable(eIasRingBufferAccessRead, 0, nullptr);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessRead, 0, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessUndef, 0, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessWrite, 0, nullptr);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Add a reader
  result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);

  // In the beginning, it should have everything available to writing,
  // and nothing to read
  result = mRingBuffer.updateAvailable(eIasRingBufferAccessWrite, 0, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, numPackets);

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessRead, 1, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, 0);

  // Adjust variables to simulate some writing
  mRingBuffer.mBufferLevel = 400;
  mRingBuffer.mWriteOffset = 400;

  // Now, there should be 400 for read, and numPackets - 400 for writing
  result = mRingBuffer.updateAvailable(eIasRingBufferAccessWrite, 0, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, numPackets - 400);

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessRead, 1, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, 400);

  // After some reading, there should be less to read
  // Note that we don't update global variables that control what was
  // read - so we simulate a case in which there's another reader,
  // that reads nothing
  IasAvbVideoRingBufferShm::RingBufferReader *reader = mRingBuffer.findReader(1);
  ASSERT_NE(reader, nullptr);
  reader->offset = 300;

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessRead, 1, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, 100);

  // If more is writen and writer wraps, there should be a lot to read
  // (that doesn't include what was read)
  mRingBuffer.mBufferLevel = numPackets - 50;
  mRingBuffer.mWriteOffset = 100;
  mRingBuffer.mReadOffset = 150; // Simulate another reader, slower, that hedges
                                 // global read offset

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessRead, 1, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  // all that's available till the end of the buffer, less what was read, plus
  // what wrapped by writer
  EXPECT_EQ(numBuffers, numPackets - 300 + mRingBuffer.mWriteOffset);

  // And just some to write
  result = mRingBuffer.updateAvailable(eIasRingBufferAccessWrite, 0, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, 50); // Writer may never go beyond what wasn't read yet
                             // If this rule changes, this test changes

  // And nothing more to write after write to the end
  mRingBuffer.mBufferLevel = numPackets;

  result = mRingBuffer.updateAvailable(eIasRingBufferAccessWrite, 0, &numBuffers);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(numBuffers, 0);
}

TEST_F(IasTestVideoRingBufferShm, access)
{
  // Tests several readers and one writer access to a buffer
  // Note that things run linearly, concurrency scenarios are
  // not explored here

  uint32_t numBuffersWriter, numBuffersReader1, numBuffersReader2, numBuffersReader3;
  uint32_t offsetWriter, offsetReader1, offsetReader2, offsetReader3;
  uint32_t numPackets = cNumPackets;

  // Add some readers
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(2);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(3);
  ASSERT_EQ(result, eIasRingBuffOk);

  // Some invalid params
  result = mRingBuffer.beginAccess(eIasRingBufferAccessUndef, 0, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 0, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, nullptr, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, nullptr);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 0, nullptr, nullptr);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 1, nullptr, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 1, &offsetWriter, nullptr);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, nullptr, nullptr);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // In the beginning, all access should be ok, readers with nothing to read,
  // writer with all it wants to write
  numBuffersReader1 = 100; // How many packets we want to read?
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader1, 0);
  EXPECT_EQ(numBuffersReader1, 0);

  numBuffersReader2 = 0; // Not common, but a reader that just wants to be kept 'alive' would do this
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 2, &offsetReader2, &numBuffersReader2);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader2, 0);
  EXPECT_EQ(numBuffersReader2, 0);

  numBuffersWriter = 400;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 0);
  EXPECT_EQ(numBuffersWriter, 400);

  // A writer may not call beginAccess again before calling endAccess
  numBuffersWriter = 400;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffNotAllowed);

  // But there's no restrictions for readers
  // TODO should we add restrictions?
  numBuffersReader1 = 100; // How many packets we want to read?
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader1, 0);
  EXPECT_EQ(numBuffersReader1, 0);

  // Good practice though tell us to end all read access, had we read anything or not
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 1, 0, 0);
  EXPECT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 2, 0, 0);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Finish writer access, writing half of what was available
  // Note that offset field on endAccess is unused (maybe remove it?)
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 200);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 0); // No reads so far
  EXPECT_EQ(mRingBuffer.mWriteOffset, 200);
  EXPECT_EQ(mRingBuffer.mBufferLevel, 200);

  // Write a bit more
  numBuffersWriter = 2 * numPackets; //asks for a really big number
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 200);
  EXPECT_EQ(numBuffersWriter, numPackets - 200); // But should only get buffer size less what is already there

  // Finish writing. First try to say the it wrote more than possible
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, numPackets);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Then do the right thing (just add 100)
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 100);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 0); // No reads so far
  EXPECT_EQ(mRingBuffer.mWriteOffset, 300);
  EXPECT_EQ(mRingBuffer.mBufferLevel, 300);

  // Each reader reads some
  numBuffersReader1 = 200;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader1, 0);
  EXPECT_EQ(numBuffersReader1, 200);

  numBuffersReader2 = 400; // Tries to read more than available
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 2, &offsetReader2, &numBuffersReader2);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader2, 0);
  EXPECT_EQ(numBuffersReader2, 300); // But only gets what is available

  numBuffersReader3 = 300;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 3, &offsetReader3, &numBuffersReader3);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader3, 0);
  EXPECT_EQ(numBuffersReader3, 300);

  // First reader ends its access, stating that it read less than available
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 1, 0, 100);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Second reads everything
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 2, 0, 300);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Third lies and tries to write more than possible
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 3, 0, 400);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Writer starts access again
  numBuffersWriter = 400;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 300);
  EXPECT_EQ(numBuffersWriter, 400);

  // Third reader properly finishes access
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 3, 0, 300);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Second reader starts access again
  numBuffersReader2 = 400; // Tries to read more than available
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 2, &offsetReader2, &numBuffersReader2);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader2, 300);
  EXPECT_EQ(numBuffersReader2, 0); // But only gets what is available. Nothing, as writer didn't finish yet

  // Writer finishes
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 300);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 100); // Slowest reader, the first, only read 100
  EXPECT_EQ(mRingBuffer.mWriteOffset, 600);
  EXPECT_EQ(mRingBuffer.mBufferLevel, 500);

  // Second reader finishes. But it lies: it tells that it read more
  // than was available when it begun. Despite writer having written something,
  // this test should fail as it may not read more than was "granted" on beginAccess
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 2, 0, 300);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Writer goes to the end. It should only wrap on next access
  numBuffersWriter = 400;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 600);
  EXPECT_EQ(numBuffersWriter, numPackets - 600);

  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, numPackets - 600);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 100); // Slowest reader, the first, only read 100
  EXPECT_EQ(mRingBuffer.mWriteOffset, 0); // Wraps over
  EXPECT_EQ(mRingBuffer.mBufferLevel, 700);

  // Some more reading
  numBuffersReader1 = 500;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader1, 100);
  EXPECT_EQ(numBuffersReader1, 500);

  numBuffersReader2 = numPackets; // Tries to read more than available
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 2, &offsetReader2, &numBuffersReader2);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader2, 300);
  EXPECT_EQ(numBuffersReader2, 500); // But only gets what is available

  numBuffersReader3 = 500;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 3, &offsetReader3, &numBuffersReader3);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader3, 300);
  EXPECT_EQ(numBuffersReader3, 500);

  // First reader ends its access, stating that it read less than available
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 1, 0, 400);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Second reads everything and reaches the end
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 2, 0, 500);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Third also finishes
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 3, 0, 500);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 500); // Slowest reader, the first, still on 500
  EXPECT_EQ(mRingBuffer.mWriteOffset, 0);
  EXPECT_EQ(mRingBuffer.mBufferLevel, 300);

  // Writer starts from beginning 
  numBuffersWriter = numPackets;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 0);
  EXPECT_EQ(numBuffersWriter, 499); // Stops short from having everything available, as one reader is slow

  // But it lies stating that wrote all
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, numPackets);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Tries again with correct value
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 499);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 500); // Slowest reader, the first, still on 500
  EXPECT_EQ(mRingBuffer.mWriteOffset, 499);
  EXPECT_EQ(mRingBuffer.mBufferLevel, 799);

  // Writer goes again
  numBuffersWriter = numPackets;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 499);
  EXPECT_EQ(numBuffersWriter, 0); // Nothing available, first reader still on 500

  // First reader finally reads everything and all readers wraps
  numBuffersReader1 = numPackets;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader1, 500);
  EXPECT_EQ(numBuffersReader1, numPackets - 500);

  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 1, 0, numPackets - 500);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Writer finishes, stating that it wrote something. Even though slowed reader
  // read something (so there's space on buffer), writer should not write more than
  // was "granted" on beginAccess
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 300);
  EXPECT_EQ(result, eIasRingBuffInvalidParam);

  // Writer retries with correct value
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 0);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 0); // All readers wrap together
  EXPECT_EQ(mRingBuffer.mWriteOffset, 499);
  EXPECT_EQ(mRingBuffer.mBufferLevel, 499);

  // One more round of reading, after the wrapping
  numBuffersReader1 = 200;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader1, &numBuffersReader1);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader1, 0);
  EXPECT_EQ(numBuffersReader1, 200);

  numBuffersReader2 = 500; // Tries to read more than available
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 2, &offsetReader2, &numBuffersReader2);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader2, 0);
  EXPECT_EQ(numBuffersReader2, 499); // But only gets what is available

  numBuffersReader3 = 300;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 3, &offsetReader3, &numBuffersReader3);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetReader3, 0);
  EXPECT_EQ(numBuffersReader3, 300);

  // First reader ends its acces, stating that it read less than available
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 1, 0, 100);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Second reads everything
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 2, 0, 499);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Third reads what it asked for
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 3, 0, 300);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Writer advances till the end
  numBuffersWriter = numPackets;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_EQ(offsetWriter, 499);
  EXPECT_EQ(numBuffersWriter, 301);

  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, 0, 301);
  EXPECT_EQ(result, eIasRingBuffOk);

  // Sanity check of global variables
  EXPECT_EQ(mRingBuffer.mReadOffset, 100); // Slowest reader
  EXPECT_EQ(mRingBuffer.mWriteOffset, 0);
  EXPECT_EQ(mRingBuffer.mBufferLevel, numPackets - 100);
}

TEST_F(IasTestVideoRingBufferShm, updateReaderAccess)
{
  // updateReaderAccess is an internal method, so it never expects its
  // parameter to be NULL

  // Add the reader and get it
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  IasAvbVideoRingBufferShm::RingBufferReader *reader = mRingBuffer.findReader(1);
  ASSERT_NE(reader, nullptr);

  reader->lastAccess = 0;

  mRingBuffer.updateReaderAccess(reader);
  EXPECT_NE(reader->lastAccess, 0);
}

TEST_F(IasTestVideoRingBufferShm, updateReaderAccessUse)
{
  // This test checks if methods that should update reader->lastAccess
  // do that

  uint32_t offsetReader, numBuffersReader, offsetWriter, numBuffersWriter = 300;

  // Let's "write" something so mRingBuffer current state allows some reading
  IasVideoRingBufferResult result = mRingBuffer.beginAccess(eIasRingBufferAccessWrite, 0, &offsetWriter, &numBuffersWriter);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.endAccess(eIasRingBufferAccessWrite, 0, offsetWriter, numBuffersWriter);
  ASSERT_EQ(result, eIasRingBuffOk);

  // First one is the one that adds a reader
  result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  IasAvbVideoRingBufferShm::RingBufferReader *reader = mRingBuffer.findReader(1);
  ASSERT_NE(reader, nullptr);
  EXPECT_NE(reader->lastAccess, 0);

  // Now let's check begin access
  reader->lastAccess = 0;
  numBuffersReader = 100;
  result = mRingBuffer.beginAccess(eIasRingBufferAccessRead, 1, &offsetReader, &numBuffersReader);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_NE(reader->lastAccess, 0);

  // End access
  reader->lastAccess = 0;
  numBuffersReader = 100;
  result = mRingBuffer.endAccess(eIasRingBufferAccessRead, 1, offsetReader, numBuffersReader);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_NE(reader->lastAccess, 0);

  // Finally, on wait read - note that this method should return immediately,
  // as there should be things to read
  mRingBuffer.mWriteOffset = 200;
  mRingBuffer.mBufferLevel = 200;
  reader->lastAccess = 0;
  result = mRingBuffer.waitRead(1, 100, 100);
  EXPECT_EQ(result, eIasRingBuffOk);
  EXPECT_NE(reader->lastAccess, 0);
}

TEST_F(IasTestVideoRingBufferShm, purgeUnresponsiveReaders)
{
  // Add some readers
  IasVideoRingBufferResult result = mRingBuffer.addReader(1);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(2);
  ASSERT_EQ(result, eIasRingBuffOk);
  result = mRingBuffer.addReader(3);
  ASSERT_EQ(result, eIasRingBuffOk);

  IasAvbVideoRingBufferShm::RingBufferReader *reader1 = mRingBuffer.findReader(1);
  ASSERT_NE(reader1, nullptr);
  IasAvbVideoRingBufferShm::RingBufferReader *reader2 = mRingBuffer.findReader(2);
  ASSERT_NE(reader2, nullptr);
  IasAvbVideoRingBufferShm::RingBufferReader *reader3 = mRingBuffer.findReader(3);
  ASSERT_NE(reader3, nullptr);

  // Now we pretend that readers 2 and 3 have an old lastAccess time
  reader1->lastAccess -= 3 * NSEC_PER_SEC;
  reader3->lastAccess -= 3 * NSEC_PER_SEC;

  mRingBuffer.purgeUnresponsiveReaders();

  EXPECT_EQ(reader1->pid, 0);
  EXPECT_EQ(reader3->pid, 0);

  // Reader 2 should be untouched
  EXPECT_EQ(reader2->pid, 2);
}

}
