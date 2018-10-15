/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBufferFactory.cpp
 * @brief   This file contains the implementation of the video ring buffer factory.
 * @details See header file for details.
 *
 * @date    2018
 */

#include "audio/common/audiobuffer/IasMemoryAllocator.hpp"
#include "avb_video_common/IasAvbVideoLog.hpp"
#include "avb_video_common/IasAvbVideoRingBufferFactory.hpp"
#include "avb_video_common/IasAvbVideoRingBuffer.hpp"
#include "avb_video_common/IasAvbVideoRingBufferShm.hpp"

#include <dlt/dlt_cpp_extension.hpp>

using IasAudio::IasMemoryAllocator;


namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbVideoRingBufferFactory::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"
#define LOG_BUFFER "buffer=" + name + ":"


IasAvbVideoRingBufferFactory*  IasAvbVideoRingBufferFactory::getInstance()
{
  static IasAvbVideoRingBufferFactory theInstance;
  return &theInstance;
}


IasAvbVideoRingBufferFactory::IasAvbVideoRingBufferFactory()
  : mLog(nullptr)
  , mMemoryMap()
{
  mLog = &IasAvbVideoLog::getDltContext("_RBF");

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


IasAvbVideoRingBufferFactory::~IasAvbVideoRingBufferFactory()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


IasVideoCommonResult IasAvbVideoRingBufferFactory::createRingBuffer(IasAvbVideoRingBuffer** ringbuffer,
                                                                    uint32_t packetSize,
                                                                    uint32_t numPackets,
                                                                    std::string name,
                                                                    std::string groupName)
{
  IasVideoCommonResult res = eIasResultOk;
  bool memAllocatorShared = true;

  if(ringbuffer == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Invalid parameter: ringbuffer == nullptr");
    return eIasResultInvalidParam;
  }

  if (name.empty() == true)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Name of ring buffer must not be an empty string");
    return eIasResultInvalidParam;
  }
  else if (packetSize == 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Packet size must be greater than zero");
    return eIasResultInvalidParam;
  }
  else if (numPackets == 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Number of packets must be greater than zero");
    return eIasResultInvalidParam;
  }

  bool allocateDataMem = true;

  uint32_t ringBufSizeShm =  static_cast<uint32_t>(sizeof(IasAvbVideoRingBufferShm));
  uint32_t memDataBuffer = packetSize * numPackets;
  uint32_t totalMemorySize = memDataBuffer + ringBufSizeShm;

  IasMemoryAllocator* mem = new IasMemoryAllocator(name, totalMemorySize, memAllocatorShared);
  AVB_ASSERT(mem != nullptr);

  IasAudio::IasAudioCommonResult result = mem->init(IasMemoryAllocator::eIasCreate);
  res = (result == IasAudio::eIasResultOk) ? eIasResultOk : eIasResultInitFailed;
  if (eIasResultOk != res)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Error initializing the memory allocator:", toString(result));
    delete mem;
    return res;
  }
  if (true == memAllocatorShared)
  {
    // Change group of shared memory file
    std::string errorMsg = "";
    result = mem->changeGroup(groupName, &errorMsg);
    res = (result == IasAudio::eIasResultOk) ? eIasResultOk : eIasResultFailed;
    if (res != eIasResultOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, errorMsg);
      delete mem;
      return res;
    }
  }

  void* dataBuf = nullptr;
  if(true == allocateDataMem)
  {
    result = mem->allocate(16,memDataBuffer,&dataBuf);
    res = (result == IasAudio::eIasResultOk) ? eIasResultOk : eIasResultFailed;
    if(res != eIasResultOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Error allocating the data memory:", toString(result));
      delete mem;
      return res;
    }
  }

  IasAvbVideoRingBuffer* ringBuf = new IasAvbVideoRingBuffer();
  AVB_ASSERT(ringBuf != nullptr);
  IasVideoRingBufferResult ringBufRes = eIasRingBuffOk;

  IasAvbVideoRingBufferShm* ringBufShm;
  std::string ringBufNameShm = name +"_ringBufferShm";
  result = mem->allocate<IasAvbVideoRingBufferShm>(ringBufNameShm,1,&ringBufShm);
  res = (result == IasAudio::eIasResultOk) ? eIasResultOk : eIasResultFailed;

  if(eIasResultOk != res)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Error allocating ring buffer:", toString(result));
    delete ringBuf;
    delete mem;
    return res;
  }

  ringBufRes = ringBuf->init(packetSize, numPackets, dataBuf, memAllocatorShared, ringBufShm);

  if(ringBufRes != eIasRingBuffOk)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Error initializing ring buffer:", uint32_t(res));
    delete ringBuf;
    delete mem;
    return eIasResultInitFailed;
  }

  std::pair<IasAvbVideoRingBuffer*,IasMemoryAllocator*> tmpPair(ringBuf,mem);

  ringBuf->setName(name);

  *ringbuffer = ringBuf;
  mMemoryMap.insert(tmpPair);

  return res;
}


void IasAvbVideoRingBufferFactory::destroyRingBuffer(IasAvbVideoRingBuffer *ringBuf)
{
  if (NULL != ringBuf)
  {
    IasMemoryAllocatorMap::iterator it = mMemoryMap.find(ringBuf);
    if(it != mMemoryMap.end())
    {
      IasMemoryAllocator* mem = (*it).second;
      AVB_ASSERT(nullptr != mem);
      const IasAvbVideoRingBufferShm* shm = ringBuf->getShm();
      if(shm != nullptr)
      {
        mem->deallocate(shm);
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't deallocate ring buffer in shared memory!");
      }
      delete (*it).first;
      delete (*it).second;
      mMemoryMap.erase(it);
    }
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ringBuf == NULL!");
  }
}


IasAvbVideoRingBuffer* IasAvbVideoRingBufferFactory::findRingBuffer(const std::string name)
{
  IasVideoCommonResult res = eIasResultOk;

  IasMemoryAllocator* mem = new IasMemoryAllocator(name, 0, true);
  AVB_ASSERT(nullptr != mem);

  IasAudio::IasAudioCommonResult result = mem->init(IasMemoryAllocator::eIasConnect);
  res = (result == IasAudio::eIasResultOk) ? eIasResultOk : eIasResultInitFailed;
  if (res != eIasResultOk)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "Unable to connect to shared memory:", toString(result));
    delete mem;
    return nullptr;
  }

  uint32_t numItems = 0;
  IasAvbVideoRingBufferShm* ringBufShm = nullptr;
  std::string ringBufName = name +"_ringBufferShm";

  result = mem->find(ringBufName, &numItems, &ringBufShm);
  res = (result == IasAudio::eIasResultOk) ? eIasResultOk : eIasResultObjectNotFound;

  if(eIasResultOk != res || 1 != numItems || nullptr == ringBufShm)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "ringBuf not found");
    delete mem;
    return nullptr;
  }
  else
  {
    IasAvbVideoRingBuffer* ringBuf = new IasAvbVideoRingBuffer();
    AVB_ASSERT(nullptr != ringBuf);

    IasVideoRingBufferResult rbres = ringBuf->setup(ringBufShm);
    if(rbres != eIasRingBuffOk)
    {
      delete ringBuf;
      delete mem;
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_BUFFER, "setup of ring buffer failed");
      return nullptr;
    }

    // ring buffer has been found, save it together with the allocator in the map.
    std::pair<IasAvbVideoRingBuffer*, IasMemoryAllocator*> tmpPair(ringBuf,mem);
    mMemoryMap.insert(tmpPair);
    ringBuf->setName(name);
    return ringBuf;
  }
}


void IasAvbVideoRingBufferFactory::loseRingBuffer(IasAvbVideoRingBuffer* ringBuf)
{
  IasMemoryAllocatorMap::iterator it = mMemoryMap.find(ringBuf);
  if(it != mMemoryMap.end())
  {
    std::string name = ringBuf->getName();
    delete (*it).first;
    delete (*it).second;
    mMemoryMap.erase(it);
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_BUFFER, "Successfully deleted");
  }
}


} // namespace IasMediaTransportAvb

// -- E O F --


