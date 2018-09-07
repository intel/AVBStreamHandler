/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbPacket.cpp
 * @brief   The implementation of the IasAvbPacket class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_helper/ias_safe.h"

namespace IasMediaTransportAvb
{

IasAvbPacket::IasAvbPacket()
  : mHome(NULL)
  , mMagic(cMagic)
  , mPayloadOffset(0u)
  , mDummyFlag(false)
{
}


/**
 *  @brief Destructor, non-virtual as we don't have any virtual methods.
 */
/*non-virtual*/
IasAvbPacket::~IasAvbPacket()
{
  // clear member fields to mark packet invalid
  mHome = NULL;
  mMagic = 0u;
}


/**
 * @brief Assignment operator
 */
IasAvbPacket& IasAvbPacket::operator=(IasAvbPacket const &other)
{
  /* Do a deep copy, including metadata,
   * do neither copy the payload pointer
   * nor mHome!
   */
  this->attime = other.attime;
  this->dmatime = other.dmatime;
  this->flags = other.flags;
  this->len = other.len;
  this->mPayloadOffset = other.mPayloadOffset;
  this->mDummyFlag = other.mDummyFlag;
  avb_safe_result copyResult = avb_safe_memcpy( this->vaddr, other.len, other.vaddr, other.len );
  AVB_ASSERT(e_avb_safe_result_ok == copyResult);
  (void) copyResult;

  return *this;
}


} // namespace IasMediaTransportAvb
