/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasLocalVideoStream.cpp
 * @brief   Implementation of a local video stream
 * @details
 *
 * @date    2014
 */

#include "avb_streamhandler/IasLocalVideoStream.hpp"
#include "avb_streamhandler/IasLocalVideoBuffer.hpp"
#include <dlt/dlt_cpp_extension.hpp>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasLocalVideoStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasLocalVideoStream::IasLocalVideoStream(DltContext &dltContext, IasAvbStreamDirection direction,
    IasLocalStreamType type, uint16_t streamId):
    mLog(&dltContext),
    mDirection(direction),
    mType(type),
    mStreamId(streamId),
    mMaxPacketRate(0),
    mMaxPacketSize(0),
    mLocalVideoBuffer(NULL),
    mFormat(IasAvbVideoFormat::eIasAvbVideoFormatRtp),
    mClientState(eIasNotConnected),
    mClient(NULL)
{
  // do nothing
}


/*
 *  Destructor.
 */
IasLocalVideoStream::~IasLocalVideoStream()
{
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasLocalVideoStream::init(IasAvbVideoFormat format, uint16_t numPackets, uint16_t maxPacketRate, uint16_t maxPacketSize, bool internalBuffers)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  mMaxPacketRate = maxPacketRate;
  mMaxPacketSize = maxPacketSize;
  mFormat = format;

  if (!isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }

  if (1500u < maxPacketSize)
  {
    result = eIasAvbProcInvalidParam;
  }

  if (eIasAvbProcOK == result)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

    // numPackets == 0 means that the derived class uses something else instead of the ring buffers (e.g. AVB->SHM)
    if (numPackets > 0u)
    {

      mLocalVideoBuffer = new (nothrow) IasLocalVideoBuffer();

      if (NULL != mLocalVideoBuffer)
      {
        if ((result = mLocalVideoBuffer->init(numPackets, maxPacketSize, internalBuffers)) == eIasAvbProcOK)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Create Ringbuffer for video data:");
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, " newLocalVideoBuffer->init fails");
          delete mLocalVideoBuffer;
          mLocalVideoBuffer = NULL;
        }
      }
      else
      {
        result = eIasAvbProcNotEnoughMemory;
      }
    }

    if(eIasAvbProcOK != result)
    {
      cleanup();
    }
  }

  return result;
}


IasAvbProcessingResult IasLocalVideoStream::writeLocalVideoBuffer(IasLocalVideoBuffer * buffer, IasLocalVideoBuffer::IasVideoDesc * descPacket)
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  (void) descPacket;
  (void) buffer;

  return error;
}


IasAvbProcessingResult IasLocalVideoStream::readLocalVideoBuffer(IasLocalVideoBuffer * buffer, IasLocalVideoBuffer::IasVideoDesc * descPacket)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }

  if ((eIasAvbProcOK == result) && ((NULL != buffer) || (NULL != descPacket)))
  {
    if(NULL != getLocalVideoBuffer())
    {
      getLocalVideoBuffer()->read(NULL, descPacket);
    }
    else
    {
      result = eIasAvbProcNullPointerAccess;
    }
  }

  return result;
}


/*
 *  Cleanup method.
 */
void IasLocalVideoStream::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if(isConnected())
  {
    disconnect();
  }

  delete mLocalVideoBuffer;
  mLocalVideoBuffer = NULL;

  mMaxPacketRate=0;
  mMaxPacketSize=0;
}


IasAvbProcessingResult IasLocalVideoStream::connect( IasLocalVideoStreamClientInterface * client )
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  if (NULL == client)
  {
    ret = eIasAvbProcInvalidParam;
  }
  else if (NULL != mClient)
  {
    ret = eIasAvbProcAlreadyInUse;
  }
  else
  {
    mClient = client;
    mClientState = eIasIdle;
  }

  return ret;
}


IasAvbProcessingResult IasLocalVideoStream::disconnect()
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  IasLocalVideoBuffer *buf = getLocalVideoBuffer();
  if (NULL != buf)
  {
    buf->setAvbPacketPool(NULL);
    buf->reset(0u);
  }

  mClientState = eIasNotConnected;
  mClient = NULL;

  return ret;
}


void IasLocalVideoStream::setClientActive( bool active )
{
  if (NULL != mClient)
  {
    if (active)
    {
      if(eIasActive != mClientState)
      {
        mClientState = eIasActive;
        (void) resetBuffers();
      }
    }
    else
    {
      mClientState = eIasIdle;
    }
  }
}

IasAvbProcessingResult IasLocalVideoStream::setAvbPacketPool(IasAvbPacketPool * avbPacketPool)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if(NULL != getLocalVideoBuffer())
  {
    getLocalVideoBuffer()->setAvbPacketPool(avbPacketPool);
  }
  else
  {
    ret = eIasAvbProcNullPointerAccess;
  }

  return ret;
}



IasLocalVideoStreamAttributes::IasLocalVideoStreamAttributes()
  : direction(eIasAvbTransmitToNetwork)
  , type(eIasLocalVideoOutStream)
  , streamId(0u)
  , format(IasAvbVideoFormat::eIasAvbVideoFormatIec61883)
  , maxPacketRate(0u)
  , maxPacketSize(0u)
  , internalBuffers(false)
{
}

IasLocalVideoStreamAttributes::IasLocalVideoStreamAttributes(const IasLocalVideoStreamAttributes &iOther)
  : direction(iOther.direction)
  , type(iOther.type)
  , streamId(iOther.streamId)
  , format(iOther.format)
  , maxPacketRate(iOther.maxPacketRate)
  , maxPacketSize(iOther.maxPacketSize)
  , internalBuffers(iOther.internalBuffers)
{
}

IasLocalVideoStreamAttributes::IasLocalVideoStreamAttributes(
    IasAvbStreamDirection direction,
    IasLocalStreamType type,
    uint16_t streamId,
    IasAvbVideoFormat format,
    uint16_t maxPacketRate,
    uint16_t maxPacketSize,
    bool internalBuffers)
  : direction(direction)
  , type(type)
  , streamId(streamId)
  , format(format)
  , maxPacketRate(maxPacketRate)
  , maxPacketSize(maxPacketSize)
  , internalBuffers(internalBuffers)
{
}

IasLocalVideoStreamAttributes &IasLocalVideoStreamAttributes::operator =(const IasLocalVideoStreamAttributes &iOther)
{
  if (this != &iOther)
  {
    direction = iOther.direction;
    type = iOther.type;
    streamId = iOther.streamId;
    format = iOther.format;
    maxPacketRate = iOther.maxPacketRate;
    maxPacketSize = iOther.maxPacketSize;
    internalBuffers = iOther.internalBuffers;
  }

  return *this;
}

bool IasLocalVideoStreamAttributes::operator ==(const IasLocalVideoStreamAttributes &iOther) const
{
  return (this == &iOther)
          || ((direction == iOther.direction)
          && (type == iOther.type)
          && (streamId == iOther.streamId)
          && (format == iOther.format)
          && (maxPacketRate == iOther.maxPacketRate)
          && (maxPacketSize == iOther.maxPacketSize)
          && (internalBuffers == iOther.internalBuffers));
}

bool IasLocalVideoStreamAttributes::operator !=(const IasLocalVideoStreamAttributes &iOther) const
{
  return !(*this == iOther);
}



} // namespace IasMediaTransportAvb
