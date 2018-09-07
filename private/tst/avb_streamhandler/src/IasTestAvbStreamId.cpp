/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 *  @file IasTestAvbStreamId.cpp
 *  @date 2018
 */
#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_streamhandler/IasAvbStreamId.hpp"
#undef protected
#undef private

#include <inttypes.h>
#include <list>

using namespace IasMediaTransportAvb;

class IasTestAvbStreamId : public ::testing::Test
{
protected:
  IasTestAvbStreamId():
    mAvbStreamId(NULL)
  {
  }

  virtual ~IasTestAvbStreamId() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
  }

  virtual void TearDown()
  {
  }

  IasAvbStreamId* mAvbStreamId;
};

TEST_F(IasTestAvbStreamId, default_CTor)
{
  mAvbStreamId = new IasAvbStreamId();
  ASSERT_TRUE(NULL != mAvbStreamId);
  ASSERT_EQ( 0x0000000000000000u, *mAvbStreamId );
  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, uint8_t_CTor_NULL)
{
  uint8_t *id8 = NULL;
  mAvbStreamId = new IasAvbStreamId(id8);
  ASSERT_TRUE(NULL != mAvbStreamId);
  ASSERT_EQ( 0x0000000000000000u, *mAvbStreamId );

  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, uint8_t_CTor_Valid)
{
  uint8_t id8[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
  mAvbStreamId = new IasAvbStreamId(id8);
  ASSERT_TRUE(NULL != mAvbStreamId);
  ASSERT_EQ( 0x0807060504030201u, *mAvbStreamId );
  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, uint64_t_CTor_0)
{
  uint64_t id64 = 0;
  mAvbStreamId = new IasAvbStreamId(id64);
  ASSERT_TRUE(NULL != mAvbStreamId);
  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, uint64_t_CTor_valid)
{
  uint64_t id64 = 255;
  mAvbStreamId = new IasAvbStreamId(id64);
  ASSERT_TRUE(NULL != mAvbStreamId);
  ASSERT_EQ( 0x00000000000000FFu, *mAvbStreamId );

  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, setStreamId_uint8_t_NULL)
{
  uint8_t * id = NULL;
  mAvbStreamId = new IasAvbStreamId();
  ASSERT_TRUE(NULL != mAvbStreamId);

  mAvbStreamId->setStreamId(id);
  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, setStreamId_uint8_t_20_chars)
{
  const uint8_t * id_out = NULL;
  uint8_t id_in[] = "12345678901234567890";
  mAvbStreamId = new IasAvbStreamId();
  ASSERT_TRUE(NULL != mAvbStreamId);

  mAvbStreamId->setStreamId(id_in);
  //ASSERT_EQ( 0x0000000000000000u, *mAvbStreamId );

  (void)id_out;

  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, setStreamId_uint8_t_8_chars)
{
  uint8_t id_in[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
  mAvbStreamId = new IasAvbStreamId();
  ASSERT_TRUE(NULL != mAvbStreamId);

  mAvbStreamId->setStreamId(id_in);
  ASSERT_EQ( 0x0807060504030201u, *mAvbStreamId );

  delete mAvbStreamId;
}


TEST_F(IasTestAvbStreamId, setDynamicStreamId)
{
  mAvbStreamId = new IasAvbStreamId();
  ASSERT_TRUE(NULL != mAvbStreamId);
  mAvbStreamId->setDynamicStreamId();

  // TODO - What should this do?

  delete mAvbStreamId;
}

TEST_F(IasTestAvbStreamId, sort_less_than_test)
{
  std::list<IasAvbStreamId> list;

  for (int i = 10; i > 0; i--)
  {
    list.push_back(IasAvbStreamId((uint64_t)i));
  }

  std::list<IasAvbStreamId>::iterator it = list.begin();
  printf("Before Sort\n");
  for (; it != list.end(); it++)
  {
    printf("%" PRIx64 "\n", uint64_t( *it ) );
  }

  list.sort();

  printf("After Sort\n");
  it = list.begin();
  for (; it != list.end(); it++)
  {
    printf("%" PRIx64 "\n", uint64_t( *it ) );
  }

  const uint64_t expected = 0x0000000000000001u;

  ASSERT_EQ( expected, *list.begin() );
}

TEST_F(IasTestAvbStreamId, copyStreamIdToBuffer)
{
  mAvbStreamId = new IasAvbStreamId();
  ASSERT_TRUE(NULL != mAvbStreamId);

  // set buffer size bigger than mStreamId - to get T -> F
  mAvbStreamId->copyStreamIdToBuffer(NULL, sizeof(uint64_t) + 1);

  delete mAvbStreamId;
}
