/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaEngine.cpp
 * @brief   This file contains the implementation of the Alsa Engine. The Alsa Engine
 *          administers the Alsa streams and worker threads which provides the timing
 *          generator for the Alsa streams.
 * @details
 * @date    2015
 */

#include "avb_streamhandler/IasAlsaEngine.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_streamhandler/IasAlsaWorkerThread.hpp"
#include <algorithm>
#include <dlt_cpp_extension.hpp>


namespace IasMediaTransportAvb {

const uint32_t IasAlsaEngine::cMinNumberAlsaBuffer = 3u;

static const std::string cClassName = "IasAlsaEngine::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

class IasAvbClockDomain;

/**
 *  Constructor.
 */
IasAlsaEngine::IasAlsaEngine()
  : mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_AEN"))
  , mIsInitialized(false)
  , mIsStarted(false)
  , mLock()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}


/**
 *  Destructor.
 */
IasAlsaEngine::~IasAlsaEngine()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();
}


/**
 *  Initialization method.
 */
IasAvbProcessingResult IasAlsaEngine::init()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!mIsInitialized)
  {
    mAlsaViDevStreams.clear();   // Clear stream map virtual Alsa Devices
    mAlsaHwDevStreams.clear();   // Clear stream map hardware Alsa Devices
    mWorkerThreads.clear();      // Clear thread list.

    mIsInitialized = true;
  }
  else
  {
    /*
     * @log Init failed: Alsa Engine is already initialized.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Already initialized!");
    result = eIasAvbProcInitializationFailed;
  }

  return result;
}


/**
 *  Start method.
 */
IasAvbProcessingResult IasAlsaEngine::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mIsInitialized)
  {
    // Iterate through the list of worker threads and start them
    mLock.lock();

    for (WorkerThreadList::iterator it = mWorkerThreads.begin(); it != mWorkerThreads.end(); it++)
    {
      AVB_ASSERT(NULL != *it);
      (void) (*it)->start();
    }

    mLock.unlock();

    mIsStarted = true;
  }
  else
  {
    /*
     * @log Not Initialized: Alsa Engine could not start because it wasn't initialized.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized! ");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


/**
 *  Stop method.
 */
IasAvbProcessingResult IasAlsaEngine::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (mIsInitialized)
  {
    // Iterate through the list of worker threads and stop them
    mLock.lock();

    for (WorkerThreadList::iterator it = mWorkerThreads.begin(); it != mWorkerThreads.end(); it++)
    {
      AVB_ASSERT(NULL != *it);
      (void) (*it)->stop();
    }

    mLock.unlock();

    mIsStarted = false;
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized! ");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


/**
 *  Cleanup method.
 */
void IasAlsaEngine::cleanup()
{
  for (WorkerThreadList::iterator it = mWorkerThreads.begin(); it != mWorkerThreads.end(); it++)
  {
    AVB_ASSERT(NULL != *it);
    (void) (*it)->stop();
  }

  IasAvbProcessingResult result = eIasAvbProcOK;
  // Delete all Alsa streams and worker thread objects created before
  while(!mAlsaViDevStreams.empty() && (eIasAvbProcOK == result))
  {
    uint16_t id = mAlsaViDevStreams.begin()->first;
    result = destroyAlsaStream( id, true);
    if (eIasAvbProcOK != result)
    {
      // Something is broken here since on cleanup no ALSA streams should be still connected.
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't destroy Alsa stream with ID = ",
              id, "Is it still connected?");
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "ID = ", id);
    }
  }

  while(!mAlsaHwDevStreams.empty() && (eIasAvbProcOK == result))
  {
    uint16_t id = mAlsaHwDevStreams.begin()->first;
    result = destroyAlsaStream( id, true);
    if (eIasAvbProcOK != result)
    {
      // Something is broken here since on cleanup no ALSA streams should be still connected.
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Can't destroy Alsa stream with ID = ",
              id, "Is it still connected?");
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "ID = ", id);
    }
  }

  mAlsaViDevStreams.clear();  // Clear whole stream map
  mAlsaHwDevStreams.clear();  // Clear whole stream map

  // After all streams have been destroyed delete the worker threads
  for (WorkerThreadList::iterator it = mWorkerThreads.begin(); it != mWorkerThreads.end(); it++)
  {
    AVB_ASSERT(NULL != *it);
    delete *it;           // Delete all worker threads
  }
  mWorkerThreads.clear(); // Clear whole worker thread list.

  mIsStarted     = false;
  mIsInitialized = false;
}


IasAvbProcessingResult IasAlsaEngine::createAlsaStream(IasAvbStreamDirection direction, uint16_t numChannels,
                                                       uint32_t sampleFrequency, IasAvbAudioFormat format,
                                                       uint32_t periodSize, uint32_t numAlsaPeriods, uint8_t channelLayout,
                                                       bool hasSideChannel, std::string deviceName, uint16_t streamId,
                                                       IasAvbClockDomain * const clockDomain, IasAlsaDeviceTypes alsaDeviceType, uint32_t sampleFreqASRC)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mIsInitialized)
  {
    mLock.lock();

    if(eIasAlsaVirtualDevice != alsaDeviceType)
    {
      // Check if Alsa HW device stream with this ID is already in use
      if (mAlsaHwDevStreams.find(streamId) !=  mAlsaHwDevStreams.end())
      {
       /*
        * @log Invalid param: StreamId provided is already in use.
        */
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stream (",streamId, ")", " already in use!");
        result = eIasAvbProcInvalidParam;
      }
      else
      {
        AlsaHwDevStreamMap::iterator it_h;
        for (it_h = mAlsaHwDevStreams.begin(); it_h != mAlsaHwDevStreams.end(); it_h++)
        {
          AVB_ASSERT(NULL != it_h->second);
          const std::string * devName = it_h->second->getDeviceName();
          AVB_ASSERT(NULL != devName);

          if (devName->find(deviceName + (IasAvbStreamDirection::eIasAvbTransmitToNetwork == direction ? "_p" : "_c")) != std::string::npos)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Device name (", deviceName.c_str(), ") is already in use!");
            result = eIasAvbProcInvalidParam;
            break;
          }
        }
      }
    }
    else
    {
      // Check if Alsa  virtual device stream with this ID is already in use
      if (mAlsaViDevStreams.find(streamId) !=  mAlsaViDevStreams.end())
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stream (",streamId, ")", " already in use!");
        result = eIasAvbProcInvalidParam;
      }
      else
      {
        // Check whether a stream with requested device name is already available
        AlsaViDevStreamMap::iterator it;
        for (it = mAlsaViDevStreams.begin(); it != mAlsaViDevStreams.end(); it++)
        {
          AVB_ASSERT(NULL != it->second);
          const std::string * devName = it->second->getDeviceName();
          AVB_ASSERT(NULL != devName);

          if (devName->find(deviceName + (IasAvbStreamDirection::eIasAvbTransmitToNetwork == direction ? "_p" : "_c")) != std::string::npos)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Device name (", deviceName.c_str(), ") is already in use!");
            result = eIasAvbProcInvalidParam;
            break;
          }
        }
      }
    }

    if (eIasAvbProcOK == result)
    {

      IasAlsaVirtualDeviceStream *newAlsaVirtualDevStream = nullptr;
      IasAlsaHwDeviceHandler     *newAlsaHwDeviceStream   = nullptr;

      if(eIasAlsaVirtualDevice == alsaDeviceType)
      {
        // create new virtual Alsa Device (use Alsa Plugin)
        newAlsaVirtualDevStream = new (nothrow) IasAlsaVirtualDeviceStream(*mLog, direction, streamId);
      }
      else
      {
        // connect to existing Alsa HW Device

        // ToDo: ABu:
        // Parameters for the audio device configuration. -> must be set by HW device config file
        // See also: .../audio/daemon2/private/tst/alsahandler/src/IasAlsaHandlerTestCaptureAsync.cpp
        const IasAudioDeviceParams cSinkDeviceParams =
        {
          /*.name        = */ "invalid",              // will be overwritten later
          /*.numChannels = */ numChannels,
          /*.samplerate  = */ sampleFreqASRC,
          /*.dataFormat  = */ eIasFormatInt16,        // ABu: ToDo: currently only int16 supported
          /*.clockType   = */ eIasClockReceivedAsync, // ABu:  ToDo: check with Alsa Clock Domain
          /*.periodSize  = */ periodSize,
          /*.numPeriodsAsrcBuffer  = */ 4,            // use default: 4 -> default is set in IasAudioCommonTypes IasAudioDeviceParams() line 328.
          /*.numPeriods  = */ numAlsaPeriods,
        };

        IasAudioDeviceParamsPtr devParams = std::make_shared<IasAudioDeviceParams>();
        *devParams = cSinkDeviceParams;
        devParams->name = deviceName;

        newAlsaHwDeviceStream = new (nothrow) IasAlsaHwDeviceHandler(*mLog, direction, streamId, devParams);
      }

      if (nullptr != newAlsaVirtualDevStream)
      {
        uint32_t totalLocalBufferSize = 0u;

        IasLocalAudioBufferDesc::AudioBufferDescMode timeAwareMode;
        timeAwareMode = IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeOff;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioTstampBuffer, timeAwareMode);

        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "time-aware buffer mode = ",
            IasLocalAudioBufferDesc::getAudioBufferDescModeString(timeAwareMode), "(", uint32_t(timeAwareMode), ")");

        if (IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeOff == timeAwareMode)
        {
          uint32_t multiplier = 1u; // gets overwritten by default value = 12
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaRingBuffer, multiplier);
          totalLocalBufferSize  = periodSize * multiplier;

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local buffer size = period size (", periodSize, " * multiplier (", multiplier, "))");
        }
        else
        {
          uint32_t descPeriodSz = periodSize;

          uint32_t baseFreq   = 48000u;
          uint32_t basePeriod = 128u;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBaseFreq, baseFreq);
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBasePeriod, basePeriod);

          const uint64_t p = uint64_t(periodSize * uint64_t(baseFreq));
          const uint64_t q = uint64_t(basePeriod * sampleFrequency);

          if (0u == p % q)
          {
            const uint32_t cycle = uint32_t(p / q);

            if (0u != cycle)
            {
              descPeriodSz = periodSize / cycle;
            }
          }

          const uint32_t minBufferSize = descPeriodSz * cMinNumberAlsaBuffer;
          totalLocalBufferSize = minBufferSize;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaRingBufferSz, totalLocalBufferSize);

          if (totalLocalBufferSize < minBufferSize)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "local buffer size must be >= ", minBufferSize, "(", descPeriodSz, " * multiplier (", cMinNumberAlsaBuffer, ") )");
            totalLocalBufferSize = minBufferSize;
          }

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local buffer size = ", totalLocalBufferSize, ")");
        }

        const uint32_t optimalFillLevel = (totalLocalBufferSize / 2u);

        // Lookup device name in registry to check whether an override of number of periods is requested for this Alsa device
        const uint32_t tmpNumAlsaPeriods = numAlsaPeriods;
        std::string fullDeviceName = deviceName + (IasAvbStreamDirection::eIasAvbTransmitToNetwork == direction ? "_p" : "_c");
        std::string optName = std::string(IasRegKeys::cAlsaDevicePeriods) + fullDeviceName;
        if (IasAvbStreamHandlerEnvironment::getConfigValue(optName, numAlsaPeriods))
        {
          // if special setting has been found send info DLT message
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Changing number of Alsa periods for device",
                  fullDeviceName.c_str(), "from", tmpNumAlsaPeriods, "to", numAlsaPeriods);
        }

        if (cMinNumberAlsaBuffer > numAlsaPeriods)
        {
          // if the number of periods shall be set to a value < 3 throw a warning message and reset number of periods
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Number of Alsa periods for device", fullDeviceName.c_str(),
                      "too small (< ", cMinNumberAlsaBuffer, "! Resetting!", numAlsaPeriods, "->", cMinNumberAlsaBuffer);
          numAlsaPeriods = cMinNumberAlsaBuffer;
        }

        result = newAlsaVirtualDevStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, periodSize, numAlsaPeriods,
                                     sampleFrequency, format, channelLayout, hasSideChannel, deviceName, alsaDeviceType);
        if (eIasAvbProcOK == result)
        {
          // Assign stream to worker thread
          result = assignToWorkerThread(newAlsaVirtualDevStream, clockDomain);

          if (eIasAvbProcOK == result)
          {
            // Add stream to stream map
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Add stream to map");
            mAlsaViDevStreams[streamId] = newAlsaVirtualDevStream;
          }
          else
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Assigning Alsa stream to worker thread failed!");
            delete newAlsaVirtualDevStream;
            newAlsaVirtualDevStream = NULL;
          }
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to init new stream!");
          delete newAlsaVirtualDevStream;
          newAlsaVirtualDevStream = NULL;
        }
      }
      else if( nullptr != newAlsaHwDeviceStream)
      {
        // ToDo: ABu: this is separate handling of Alsa HW Device Stream - it is just a POC
        //            - handling of HW and virtual Streams could be done better (generic) here are currently many code doublets

        uint32_t totalLocalBufferSize = 0u;

        IasLocalAudioBufferDesc::AudioBufferDescMode timeAwareMode;
        timeAwareMode = IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeOff;
        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioTstampBuffer, timeAwareMode);

        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "time-aware buffer mode = ",
            IasLocalAudioBufferDesc::getAudioBufferDescModeString(timeAwareMode), "(", uint32_t(timeAwareMode), ")");

        if (IasLocalAudioBufferDesc::AudioBufferDescMode::eIasAudioBufferDescModeOff == timeAwareMode)
        {
          uint32_t multiplier = 1u; // gets overwritten by default value = 12
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaRingBuffer, multiplier);
          totalLocalBufferSize  = periodSize * multiplier;

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local buffer size = period size (", periodSize, " * multiplier (", multiplier, "))");
        }
        else
        {
          uint32_t descPeriodSz = periodSize;

          uint32_t baseFreq   = sampleFrequency;
          uint32_t basePeriod = periodSize;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBaseFreq, baseFreq);
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBasePeriod, basePeriod);

          const uint64_t p = uint64_t(periodSize * uint64_t(baseFreq));
          const uint64_t q = uint64_t(basePeriod * sampleFrequency);

          if (0u == p % q)
          {
            const uint32_t cycle = uint32_t(p / q);

            if (0u != cycle)
            {
              descPeriodSz = periodSize / cycle;
            }
          }

          const uint32_t minBufferSize = descPeriodSz * cMinNumberAlsaBuffer;
          totalLocalBufferSize = minBufferSize;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaRingBufferSz, totalLocalBufferSize);

          if (totalLocalBufferSize < minBufferSize)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "local buffer size must be >= ", minBufferSize, "(", descPeriodSz, " * multiplier (", cMinNumberAlsaBuffer, ") )");
            totalLocalBufferSize = minBufferSize;
          }

          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "local buffer size = ", totalLocalBufferSize, ")");
        }

        const uint32_t optimalFillLevel = (totalLocalBufferSize / 2u);

        // Lookup device name in registry to check whether an override of number of periods is requested for this Alsa device
        const uint32_t tmpNumAlsaPeriods = numAlsaPeriods;
        std::string fullDeviceName = deviceName + (IasAvbStreamDirection::eIasAvbTransmitToNetwork == direction ? "_p" : "_c");
        std::string optName = std::string(IasRegKeys::cAlsaDevicePeriods) + fullDeviceName;
        if (IasAvbStreamHandlerEnvironment::getConfigValue(optName, numAlsaPeriods))
        {
          // if special setting has been found send info DLT message
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Changing number of Alsa periods for device",
                  fullDeviceName.c_str(), "from", tmpNumAlsaPeriods, "to", numAlsaPeriods);
        }

        if (cMinNumberAlsaBuffer > numAlsaPeriods)
        {
          // if the number of periods shall be set to a value < 3 throw a warning message and reset number of periods
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Number of Alsa periods for device", fullDeviceName.c_str(),
                      "too small (< ", cMinNumberAlsaBuffer, "! Resetting!", numAlsaPeriods, "->", cMinNumberAlsaBuffer);
          numAlsaPeriods = cMinNumberAlsaBuffer;
        }

        IasDeviceType deviceType = eIasDeviceTypeUndef;
        if (eIasAvbTransmitToNetwork == direction)
        {
          deviceType = eIasDeviceTypeSource;
        }
        else
        {
          deviceType = eIasDeviceTypeSink;
        }

        // setDeviceType before init!
        newAlsaHwDeviceStream->setDeviceType(deviceType);
        result = newAlsaHwDeviceStream->init(numChannels, totalLocalBufferSize, optimalFillLevel, periodSize, numAlsaPeriods,
                                            sampleFrequency, format, channelLayout, hasSideChannel, deviceName, alsaDeviceType);

        if (eIasAvbProcOK == result)
        {
           // Assign stream to worker thread
           result = assignToWorkerThread(newAlsaHwDeviceStream, clockDomain);

           if (eIasAvbProcOK == result)
           {
             // Add stream to stream map
             DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Add Alsa HW device stream to map");
             mAlsaHwDevStreams[streamId] = newAlsaHwDeviceStream;
           }
           else
           {
             DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Assigning Alsa HW device stream to worker thread failed!");
             delete newAlsaHwDeviceStream;
             newAlsaHwDeviceStream = NULL;
           }

           // ABu: ToDo: handling calls like in virtual Stream !?! e.g. connect / disconnect / start / stop....
           // start HW Handler WorkerThread
           IasAlsaHwDeviceHandler::IasResult res = IasAlsaHwDeviceHandler::eIasNotInitialized;
           res = newAlsaHwDeviceStream->start();
           if (IasAlsaHwDeviceHandler::eIasOk != res)
           {
             DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to start HW Alsa device Worker Thread!");
           }
        }
        else
        {
           DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to init new Alsa HW device stream!");
           delete newAlsaHwDeviceStream;
           newAlsaHwDeviceStream = NULL;
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate Alsa stream!");
        result = eIasAvbProcNotEnoughMemory;
      }
    }
    mLock.unlock();
  }
  else
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized! ");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


IasAvbProcessingResult IasAlsaEngine::destroyAlsaStream(uint16_t streamId, bool forceDestruction)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mIsInitialized)
  {
    mLock.lock();
    bool isAlsaVirtualDeviceStream = false;
    bool foundStream               = false;

    // Search for the stream and delete it if found
    AlsaViDevStreamMap::iterator it_d = mAlsaViDevStreams.find(streamId);
    AlsaHwDevStreamMap::iterator it_h = mAlsaHwDevStreams.find(streamId);

    // virtual Alsa Devices Stream found
    if ( it_h != mAlsaHwDevStreams.end())
    {
      isAlsaVirtualDeviceStream = false;
      foundStream               = true;
    }

    if ( (it_d != mAlsaViDevStreams.end() ) && ( false == foundStream )  )
    {
      isAlsaVirtualDeviceStream = true;
      foundStream               = true;
    }

    if (foundStream)
    {
      if(isAlsaVirtualDeviceStream)
      {
        AVB_ASSERT(NULL != it_d->second);
        IasAlsaStreamInterface * stream = it_d->second;

        (void) forceDestruction;
        if (!stream->isConnected()) // not allowed to delete connected streams
        {
          // Remove the stream from the worker thread that handles it
          result = removeFromWorkerThread(stream);
          if (eIasAvbProcOK == result)
          {
            // Erase stream
            mAlsaViDevStreams.erase(it_d);
            delete dynamic_cast<IasAlsaVirtualDeviceStream*>(stream);
            stream = NULL;
          }
        }
        else
        {
          // Stream is still connected to an AVB stream
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stream still connected, disconnect it before (virtual alsa devicce) "
              "removal!");
          result = eIasAvbProcAlreadyInUse;
        }
      }
      // HW Alsa Devices Stream found
      else
      {
        AVB_ASSERT(NULL != it_h->second);
        IasAlsaStreamInterface * stream = it_h->second;

        (void) forceDestruction;
        if (!stream->isConnected()) // not allowed to delete connected streams
        {
         // Remove the stream from the worker thread that handles it
         result = removeFromWorkerThread(stream);
         if (eIasAvbProcOK == result)
         {
           // Erase stream
           mAlsaHwDevStreams.erase(it_h);
           delete dynamic_cast<IasAlsaHwDeviceHandler*>(stream);
           stream = NULL;
         }
        }
        else
        {
        /*
         * @log Already in use: The ALSA stream is still connected and cannot be destroyed.
         */
         DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stream still connected, disconnect it before (alsa HW device) "
             "removal!");
         result = eIasAvbProcAlreadyInUse;
        }
      }
    }
    else
    {
      /*
       * @log Invalid param: StreamId associated with ALSA stream cannot be found in the ALSA stream list.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Stream (", streamId,
                  ") couldn't be destroyed because it can't be found in stream list!");
      result = eIasAvbProcInvalidParam;
    }
    mLock.unlock();
  }
  else
  {
    /*
     * @log Not initialized: The ALSA Engine is not initialized, cannot call destroy stream.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized! ");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


/**
 *  Get local audio stream pointer
 */
IasLocalAudioStream *IasAlsaEngine::getLocalAudioStream(uint16_t streamId)
{
  mLock.lock();

  IasLocalAudioStream *localAudioStream = NULL;

  AlsaViDevStreamMap::iterator it_d = mAlsaViDevStreams.find(streamId);
  AlsaHwDevStreamMap::iterator it_h = mAlsaHwDevStreams.find(streamId);

  if( it_d != mAlsaViDevStreams.end() )
  {
    localAudioStream = static_cast<IasLocalAudioStream*>(it_d->second);
  }

  if( it_h != mAlsaHwDevStreams.end() )
  {
    localAudioStream = static_cast<IasLocalAudioStream*>(it_h->second);
  }

  mLock.unlock();

  return localAudioStream;
}


/**
 * Searches all worker threads in the list for the one that handles the specified stream ID
 */
IasAlsaWorkerThread *IasAlsaEngine::getWorkerThread(uint16_t streamId)
{
  IasAlsaWorkerThread *workerThread = NULL;

  WorkerThreadList::iterator it;
  for (it = mWorkerThreads.begin(); it != mWorkerThreads.end(); it++)
  {
    AVB_ASSERT(NULL != *it);
    if ((*it)->streamIsHandled(streamId))
    {
      workerThread = *it;
      break;
    }
  }

  return workerThread;
}


IasAvbProcessingResult IasAlsaEngine::assignToWorkerThread(IasAlsaStreamInterface *alsaStream,
                                                           IasAvbClockDomain * const clockDomain)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mIsInitialized)
  {
    if ((NULL != alsaStream) && (0u != alsaStream->getPeriodSize()) && (0u != alsaStream->getSampleFrequency()))
    {
      // Search list of available worker threads and check if parameters are matching.
      WorkerThreadList::iterator it;
      for (it = mWorkerThreads.begin(); it != mWorkerThreads.end(); it++)
      {
        AVB_ASSERT(NULL != *it);
        if ((*it)->checkParameter(clockDomain, alsaStream->getPeriodSize(), alsaStream->getSampleFrequency()))
        {
          // Matching worker thread object has been found, so add the stream to that worker thread
          DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Matching worker thread object has been found");
          result = (*it)->addAlsaStream(alsaStream);
          if (eIasAvbProcOK == result)
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Stream (", alsaStream->getStreamId(), ") successfully added to worker thread");
          }
          else
          {
            /*
             * @log Failed to add the stream to a worker thread.
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Adding stream (",
                        alsaStream->getStreamId(), ") to existing worker thread failed!");
          }
          break;
        }
      }

      if (mWorkerThreads.end() == it)
      {
        // Worker thread not found so create new one
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Matching worker thread object hasn't been found, create one");
        IasAlsaWorkerThread *newAlsaWorkerThread = new (nothrow) IasAlsaWorkerThread(*mLog);

        if (NULL != newAlsaWorkerThread)
        {
          uint32_t baseFreq   = 48000u;
          uint32_t basePeriod = 128u;
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBaseFreq, baseFreq);
          (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAlsaBasePeriod, basePeriod);

          /* if this alsaStream can be serviced using the base period time, do so, otherwise
           * create worker thread with the period time of this particular alsaStream
           */
          if (IasAlsaWorkerThread::checkParameter(alsaStream->getPeriodSize(), alsaStream->getSampleFrequency(),
              basePeriod, baseFreq))
          {
            result = newAlsaWorkerThread->init(alsaStream, basePeriod, baseFreq, clockDomain);
          }
          else
          {
            result = newAlsaWorkerThread->init(alsaStream, alsaStream->getPeriodSize(), alsaStream->getSampleFrequency(), clockDomain);
          }

          if (eIasAvbProcOK == result)
          {
            // Add worker thread object to worker thread list and start it
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Add Alsa worker thread to list");
            mWorkerThreads.push_back(newAlsaWorkerThread);
            if (mIsStarted) // start worker thread only if Alsa Engine is started
            {
              result = newAlsaWorkerThread->start();
              if (eIasAvbProcOK != result)
              {
                /*
                 * @log Couldn't start the Alsa worker thread.
                 */
                DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to start new Alsa worker thread!");
              }
            }
          }
          else
          {
            /*
             * @log Couldn't init the Alsa worker thread, parameters NULL or not enough memory to create the thread.
             */
            DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Failed to init new Alsa worker thread!");
            delete newAlsaWorkerThread;
            newAlsaWorkerThread = NULL;
          }
        }
        else
        {
          /*
           * @log Not enough memory: Couldn't create a new Alsa worker thread.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate Alsa Worker Thread!");
          result = eIasAvbProcNotEnoughMemory;
        }
      }
    }
    else
    {
      /*
       * @log Invalid param: AlsaStream == NULL
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid Alsa Stream!!!");
      result = eIasAvbProcInvalidParam;
    }
  }
  else
  {
    /*
     * @log Not initialized: Cannot assign to the worker thread if the stream isn't initialized.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized! ");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}


IasAvbProcessingResult IasAlsaEngine::removeFromWorkerThread(const IasAlsaStreamInterface * const stream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (mIsInitialized)
  {
    if (NULL != stream)
    {
      // Try to find the associated worker thread
      IasAlsaWorkerThread *workerThread = getWorkerThread(stream->getStreamId());
      if (NULL != workerThread)
      {
        // Remove stream from worker thread
        bool lastStream = false;
        result = workerThread->removeAlsaStream(stream, lastStream);
        if (eIasAvbProcOK == result)
        {
          // Special handling if it is the last stream: Stop thread and delete it
          if (lastStream)
          {
            WorkerThreadList::iterator it = std::find(mWorkerThreads.begin(), mWorkerThreads.end(), workerThread);
            if (mWorkerThreads.end() != it)
            {
              workerThread->stop();
              mWorkerThreads.erase(it);
              delete workerThread;
              workerThread = NULL;
              DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Last Alsa stream successfully removed from worker thread and thread has been deleted");
            }
            else
            {
              /*
               * @log Processing error: Could not stop and remove the worker thread for this stream because it is not present in the worker thread list.
               */
              DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Fatal: Couldn't stop and remove worker "
                          "thread because it can't be found in the list!");
              result = eIasAvbProcErr;
            }
          }
          else
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Alsa stream has been successfully removed "
                        "from worker thread!");
          }
        }
        else
        {
          /*
           * @log Processing error: Failed to remove the stream from the worker thread, something is seriously wrong.
           */
          DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Fatal Error: Removing Alsa stream from "
              "worker thread failed!");
          result = eIasAvbProcErr;
        }
      }
      else
      {
        // Should not happen! Something completely messed up here!!!!
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Fatal Error: Couldn't find worker thread!");
        result = eIasAvbProcErr;
      }
    }
    else
    {
      /*
       * @log Invalid param: AlsaStream == NULL
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid parameter! (stream == NULL!");
      result = eIasAvbProcInvalidParam;
    }
  }
  else
  {
    /*
     * @log Not initialized: The ALSA Engine is not initialized, cannot call destroy stream.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not initialized! ");
    result = eIasAvbProcNotInitialized;
  }

  return result;
}

bool IasAlsaEngine::getLocalStreamInfo(const uint16_t &streamId, LocalAudioStreamInfoList &audioStreamInfo) const
{
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Stream Id =", streamId);
  bool found = false;
  for (AlsaViDevStreamMap::const_iterator it = mAlsaViDevStreams.begin(); it != mAlsaViDevStreams.end() && !found; ++it)
  {
    if (0u == streamId || it->first == streamId)
    {
      IasAlsaVirtualDeviceStream *alsaStream = it->second;
      DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "Local Audio");
      IasLocalAudioStreamAttributes att;
      att.setDirection(alsaStream->getDirection());
      att.setNumChannels(alsaStream->getNumChannels());
      att.setSampleFrequency(alsaStream->getSampleFrequency());
      att.setFormat(eIasAvbAudioFormatSaf16); // the only one supported for alsa streams currently
      att.setPeriodSize(alsaStream->getPeriodSize());
      att.setNumPeriods(alsaStream->getNumPeriods());
      att.setChannelLayout(alsaStream->getChannelLayout());
      att.setHasSideChannel(alsaStream->hasSideChannel());
      att.setDeviceName(*(alsaStream->getDeviceName()));
      att.setStreamId(alsaStream->getStreamId());
      att.setConnected(alsaStream->isConnected());
      att.setStreamDiagnostics(*alsaStream->getDiagnostics());

      audioStreamInfo.push_back(att);
    }

    if (0u != streamId && it->first == streamId)
    {
      found = true;
    }

  }

  return found;
}


} // namespace IasMediaTransportAvb




