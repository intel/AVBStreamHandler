/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoShmConnection.cpp
 * @brief   Implementation of the connection class.
 * @details See header file for details.
 *
 * @date    2018
 */

#include "avb_video_common/IasAvbVideoShmConnection.hpp"
#include "avb_video_common/IasAvbVideoRingBufferFactory.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#include <dlt/dlt_cpp_extension.hpp>


namespace IasMediaTransportAvb {


static const std::string cClassName = "IasAvbVideoShmConnection::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"


/**
 *  Constructor.
 */
IasAvbVideoShmConnection::IasAvbVideoShmConnection(bool isCreator)
//  : mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_AVC"))
  : mLog(nullptr)
  , mRingBuffer(nullptr)
  , mIsCreator(isCreator)
  , mConnectionName()
  , mGroupName()
{
  // Need to do it here since this class is also used on client side where IasAvbStreamHandlerEnvironment isn't available
  mLog = new DltContext();
  dlt_register_context(mLog, "_AVC", "Context for video SHM connection");

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


/**
 *  Destructor.
 */
IasAvbVideoShmConnection::~IasAvbVideoShmConnection()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();
}


IasVideoCommonResult IasAvbVideoShmConnection::init(const std::string & connectionName, const std::string& groupName)
{
  IasVideoCommonResult result = eIasResultOk;

  if (connectionName.empty() || groupName.empty())
  {
    result = eIasResultInvalidParam;
  }
  else
  {
    mConnectionName = "avb_" + connectionName;
    mGroupName = groupName;
  }
  return result;
}


void IasAvbVideoShmConnection::cleanup()
{
  if (mIsCreator)
  {
    IasAvbVideoRingBufferFactory::getInstance()->destroyRingBuffer(mRingBuffer);
  }
  else
  {
    IasAvbVideoRingBufferFactory::getInstance()->loseRingBuffer(mRingBuffer);
  }
  mConnectionName.clear();
  mGroupName.clear();

  dlt_unregister_context(mLog);
  delete(mLog);
  mLog = nullptr;
}


IasVideoCommonResult IasAvbVideoShmConnection::createRingBuffer(uint32_t numPackets, uint32_t maxPacketSize)
{
  IasVideoCommonResult result = eIasResultOk;

  if (mConnectionName.empty()) // test if initialized
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Connection is not initialized, can't create ring buffer!");
    result = eIasResultNotInitialized;
  }
  else if (!mIsCreator)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Only the server is allowed to create a ring buffer!");
    result = eIasResultNotAllowed;
  }
  else
  {
    // if already available destroy it first
    if (mRingBuffer)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Ringbuffer", mConnectionName,
                  "already available, destroying it before recreation");
      IasAvbVideoRingBufferFactory::getInstance()->destroyRingBuffer(mRingBuffer);
    }

    // Create Ringbuffer
    result = IasAvbVideoRingBufferFactory::getInstance()->createRingBuffer(&mRingBuffer, maxPacketSize, numPackets,
                                                                           mConnectionName, mGroupName);
  }

  return result;
}


IasVideoCommonResult IasAvbVideoShmConnection::findRingBuffer()
{
  IasVideoCommonResult result = eIasResultOk;

  if (mConnectionName.empty())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Connection is not initialized, can't find ring buffer!");
    result = eIasResultNotInitialized;
  }
  else if (mIsCreator)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "findRingbuffer can only be performed from client side!");
    result = eIasResultNotAllowed;
  }
  else
  {
    if (mRingBuffer)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Ringbuffer", mConnectionName, "has already been found and is available");
    }
    else
    {
      mRingBuffer = IasAvbVideoRingBufferFactory::getInstance()->findRingBuffer(mConnectionName);
      if (nullptr == mRingBuffer)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Ringbuffer with name", mConnectionName,"can't be found!");
        result = eIasResultObjectNotFound;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Ringbuffer", mConnectionName, "has been found");
      }
    }
  }

  return result;
}


} // namespace IasMediaTransportAvb




