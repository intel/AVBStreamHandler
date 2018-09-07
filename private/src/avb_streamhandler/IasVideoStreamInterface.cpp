/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasVidoeStreamInterface.cpp
 * @brief   This file contains the implementation of the video streaming interface
 * @details
 * @date    2018
 */

#include <dlt/dlt_cpp_extension.hpp>
#include "avb_streamhandler/IasVideoStreamInterface.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <cstdlib>


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasVideoStreamInterface::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasVideoStreamInterface::IasVideoStreamInterface()
  : mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_AVI"))
  , mIsInitialized(false)
  , mRunning(false)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


/*
 *  Destructor.
 */
IasVideoStreamInterface::~IasVideoStreamInterface()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasVideoStreamInterface::init()
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (!mIsInitialized)
  {
  // TODO: nothing to init ATM

    mIsInitialized = (eIasAvbProcOK == result);
  }

  return result;
}


/*
 *  Start method.
 */
IasAvbProcessingResult IasVideoStreamInterface::start()
{
  IasAvbProcessingResult result = eIasAvbProcNotInitialized;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (mIsInitialized)
  {
    mRunning = true;
    result = eIasAvbProcOK;

    for (VideoInStreamMap::iterator it = mVideoInStreams.begin(); it != mVideoInStreams.end(); it++)
    {
      AVB_ASSERT(NULL != it->second);
      result = it->second->start();

      if (eIasAvbProcOK != result)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error starting VideoInStream");
        break;
      }
    }

#if 0   // outstreams do not have a start method (yet)
    if (eIasAvbProcOK == result)
    {
      for (VideoOutStreamMap::iterator it = mVideoOutStreams.begin(); it != mVideoOutStreams.end(); it++)
      {
        AVB_ASSERT(NULL != it->second);
        result = it->second->start();

        if (eIasAvbProcOK != result)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error starting VideoOutStream");
          break;
        }
      }
    }
#endif

    if (eIasAvbProcOK != result)
    {
      stop();
    }
  }

  return result;
}


/*
 *  Stop method.
 */
IasAvbProcessingResult IasVideoStreamInterface::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (mRunning)
  {
    for (VideoInStreamMap::iterator it = mVideoInStreams.begin(); it != mVideoInStreams.end(); it++)
    {
      AVB_ASSERT(NULL != it->second);

      (void) it->second->stop();
    }

    for (VideoOutStreamMap::iterator it = mVideoOutStreams.begin(); it != mVideoOutStreams.end(); it++)
    {
      AVB_ASSERT(NULL != it->second);
      // outstreams do not have a start method (yet)
      // (void) it->second->stop();
    }

    mRunning = false;
  }

  return result;
}


/*
 *  Cleanup method.
 */
void IasVideoStreamInterface::cleanup()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  for (VideoInStreamMap::iterator it = mVideoInStreams.begin(); it != mVideoInStreams.end(); it++)
  {
    AVB_ASSERT(NULL != it->second);
    delete it->second;
  }

  for (VideoOutStreamMap::iterator it = mVideoOutStreams.begin(); it != mVideoOutStreams.end(); it++)
  {
    AVB_ASSERT(NULL != it->second);
    delete it->second;
  }

  mVideoInStreams.clear();
  mVideoOutStreams.clear();
}


/*
 *  Create a video stream
 */
IasAvbProcessingResult IasVideoStreamInterface::createVideoStream(IasAvbStreamDirection direction,
                                         uint16_t maxPacketRate,
                                         uint16_t maxPacketSize,
                                         IasAvbVideoFormat format,
                                         std::string ipcName,
                                         uint16_t streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mRunning)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

    if (direction == IasAvbStreamDirection::eIasAvbTransmitToNetwork)
    {
      result = createVideoInStream( maxPacketRate, maxPacketSize, format, ipcName, streamId);
    }
    else if (direction == IasAvbStreamDirection::eIasAvbReceiveFromNetwork)
    {
      result = createVideoOutStream( maxPacketRate, maxPacketSize, format, ipcName, streamId);
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not be called while video Interface is not running");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


/*
 *  Create a video IN stream
 */
IasAvbProcessingResult IasVideoStreamInterface::createVideoInStream( uint16_t maxPacketRate,
                                         uint16_t maxPacketSize,
                                         IasAvbVideoFormat format,
                                         std::string ipcName,
                                         uint16_t streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mRunning)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

    if (mVideoInStreams.find(streamId) !=  mVideoInStreams.end())
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, " StreamId already in use. Id=", streamId);
      result = eIasAvbProcInvalidParam;
    }
    else
    {
      IasLocalVideoInStream *newVideoInStream = new (nothrow) IasLocalVideoInStream(*mLog, streamId);

      if (NULL != newVideoInStream)
      {
        uint16_t numPackets = 800u;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cVideoInNumPackets, numPackets);

        bool internalBuffers = false;
        const uint32_t optimalFillLevel = (numPackets / 2u); // has no effect currently

        result = newVideoInStream->init(ipcName, maxPacketRate, maxPacketSize, format, numPackets, optimalFillLevel, internalBuffers);

        if ((eIasAvbProcOK == result) && mRunning)
        {
          result = newVideoInStream->start();
        }

        if (eIasAvbProcOK == result)
        {
          // add stream to map
          mVideoInStreams[streamId] = newVideoInStream;
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to init new video stream!");
          delete newVideoInStream;
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate VideoStream!");
        result = eIasAvbProcNotEnoughMemory;
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not be called while video Interface is not running");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


/*
 *  Create a video OUT stream
 */
IasAvbProcessingResult IasVideoStreamInterface::createVideoOutStream( uint16_t maxPacketRate,
                                         uint16_t maxPacketSize,
                                         IasAvbVideoFormat format,
                                         std::string ipcName,
                                         uint16_t streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mRunning)
  {

    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

    if (mVideoOutStreams.find(streamId) !=  mVideoOutStreams.end())
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "StreamId already in use. Id=", streamId);
      result = eIasAvbProcInvalidParam;
    }
    else
    {
      IasLocalVideoOutStream *newVideoOutStream = new (nothrow) IasLocalVideoOutStream(*mLog, streamId);

      if (NULL != newVideoOutStream)
      {
//        uint16_t numPackets = 0u; //No ring buffer needed by current implementation
        uint16_t numPackets = 800u; // TODO:
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cVideoOutNumPackets, numPackets);

        bool internalBuffers = true;
        const uint32_t optimalFillLevel = (numPackets / 2u); // has no effect currently

        result = newVideoOutStream->init(ipcName, maxPacketRate, maxPacketSize, format, numPackets, optimalFillLevel,
                                         internalBuffers);

        if ((eIasAvbProcOK == result) && mRunning)
        {
//          result = newVideoOutStream->start();
        }

        if (eIasAvbProcOK == result)
        {
          // add stream to map
          mVideoOutStreams[streamId] = newVideoOutStream;
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to init new video stream!");
          delete newVideoOutStream;
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate VideoStream!");
        result = eIasAvbProcNotEnoughMemory;
      }
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Could not be called while video Interface is not running");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


/*
 *  Get local video stream pointer
 */
IasLocalVideoStream * IasVideoStreamInterface::getLocalVideoStream(uint16_t streamId)
{
  VideoInStreamMap::iterator  itIn  = mVideoInStreams.find(streamId);
  VideoOutStreamMap::iterator itOut = mVideoOutStreams.find(streamId);

  if(itIn != mVideoInStreams.end())
  {
    return itIn->second;
  }
  else if (itOut != mVideoOutStreams.end())
  {
    return itOut->second;
  }
  else
  {
    return (IasLocalVideoStream*)NULL;
  }
}


/*
 *  Destroy a video video stream
 */
IasAvbProcessingResult IasVideoStreamInterface::destroyVideoStream(uint16_t streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  VideoInStreamMap::iterator itIn = mVideoInStreams.find(streamId);
  VideoOutStreamMap::iterator itOut = mVideoOutStreams.find(streamId);

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (itIn != mVideoInStreams.end())
  {
    AVB_ASSERT(NULL != itIn->second);
    IasLocalVideoInStream *stream = itIn->second;

    if (!stream->IasLocalVideoStream::isConnected())
    {
      mVideoInStreams.erase(itIn);
      stream->cleanup();
      delete stream;
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "VideoIn stream still connected");
      result = eIasAvbProcAlreadyInUse;
    }
  }
  else if (itOut != mVideoOutStreams.end())
  {
    AVB_ASSERT(NULL != itOut->second);
    IasLocalVideoOutStream *stream = itOut->second;

    if (!stream->isConnected())
    {
      mVideoOutStreams.erase(itOut);
      stream->cleanup();
      delete stream;
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "VideoOut stream still connected");
      result = eIasAvbProcAlreadyInUse;
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Element doesn't exist");
    result = eIasAvbProcInvalidParam;
  }

  return result;
}


bool IasVideoStreamInterface::getLocalStreamInfo(const uint16_t &streamId, LocalVideoStreamInfoList &videoStreamInfo) const
{
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Stream Id =", streamId);
  bool found = false;
  for (VideoInStreamMap::const_iterator it = mVideoInStreams.begin(); it != mVideoInStreams.end() && !found; ++it)
  {
    if (0u == streamId || it->first == streamId)
    {
      IasLocalVideoInStream *inStream = it->second;
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Local Video Rx");
      IasLocalVideoStreamAttributes att;
      att.setDirection(inStream->getDirection());
      att.setType(inStream->getType());
      att.setStreamId(inStream->getStreamId());
      att.setFormat(inStream->getFormat());
      att.setMaxPacketRate(inStream->getMaxPacketRate());
      att.setMaxPacketSize(inStream->getMaxPacketSize());
      att.setInternalBuffers(inStream->getLocalVideoBuffer()->getInternalBuffers());

      videoStreamInfo.push_back(att);
    }

    if (0u != streamId && it->first == streamId)
    {
      found = true;
    }

  }
  for (VideoOutStreamMap::const_iterator it = mVideoOutStreams.begin(); it != mVideoOutStreams.end() && !found; ++it)
  {
    if (0u == streamId || it->first == streamId)
    {
      IasLocalVideoOutStream *outStream = it->second;
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Local Video Tx");
      IasLocalVideoStreamAttributes att;
      att.setDirection(outStream->getDirection());
      att.setType(outStream->getType());
      att.setStreamId(outStream->getStreamId());
      att.setFormat(outStream->getFormat());
      att.setMaxPacketRate(outStream->getMaxPacketRate());
      att.setMaxPacketSize(outStream->getMaxPacketSize());
      att.setInternalBuffers(outStream->getLocalVideoBuffer()->getInternalBuffers());

      videoStreamInfo.push_back(att);
    }

    if (0u != streamId && it->first == streamId)
    {
      found = true;
    }

  }

  return found;
}


} // namespace IasMediaTransportAvb




