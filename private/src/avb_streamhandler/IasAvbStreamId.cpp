/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbStreamId.cpp
 * @brief   This is the implementation of the IasAvbStreamId class.
 * @date    2013
 */

#include <string.h>

#include "avb_streamhandler/IasAvbStreamId.hpp"


namespace IasMediaTransportAvb {

/*
 *  Constructor 1.
 */
IasAvbStreamId::IasAvbStreamId()
  : mStreamId( 0u )
{
}


/*
 *  Constructor 2.
 */
IasAvbStreamId::IasAvbStreamId(uint8_t * streamId)
  : mStreamId( 0u )
{
  setStreamId( streamId );
}


/*
 *  Constructor 3.
 */
IasAvbStreamId::IasAvbStreamId(uint64_t streamId)
  : mStreamId( streamId )
{
}


/*
 *  Destructor.
 */
IasAvbStreamId::~IasAvbStreamId()
{
  // nothing to do
}


void IasAvbStreamId::setStreamId(const uint8_t * const streamId)
{
  if (NULL != streamId)
  {
    for (size_t i = 0u; i < sizeof mStreamId; i++)
    {
      mStreamId = mStreamId << 8;
      mStreamId += streamId[i];
    }
  }
}


void IasAvbStreamId::copyStreamIdToBuffer(uint8_t * buf, size_t bufSize) const
{
  if (bufSize > sizeof mStreamId)
  {
    bufSize = sizeof mStreamId;
  }

  if (NULL != buf)
  {
    for (size_t i = 0u, j = 8u * (bufSize - 1u); i < bufSize; i++, j-= 8)
    {
      buf[i] = static_cast<uint8_t>(mStreamId >> j);
    }
  }
}


void IasAvbStreamId::setDynamicStreamId()
{
}

} // namespace IasMediaTransportAvb
