/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbTransmitEngine.cpp
 * @brief   The definition of the IasAvbTransmitEngine class.
 * @date    2013
 */

#include "avb_streamhandler/IasAvbTransmitEngine.hpp"

#include "avb_streamhandler/IasAvbAudioStream.hpp"
#include "avb_streamhandler/IasAvbVideoStream.hpp"
#include "avb_streamhandler/IasAvbClockReferenceStream.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_streamhandler/IasAvbPacketPool.hpp"
#include "avb_streamhandler/IasAvbTransmitSequencer.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEventInterface.hpp"
// TO BE REPLACED #include "core_libraries/btm/ias_dlt_btm.h"

#include <sstream>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <sstream>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbTransmitEngine::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"


/*
 *  Constructor.
 */
IasAvbTransmitEngine::IasAvbTransmitEngine()
  : mIgbDevice(NULL)
  , mAvbStreams()
  , mUseShaper(false)
  , mUseResume(false)
  , mRunning(false)
  , mSequencers() // inits to NULL
  , mEventInterface(NULL)
  , mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_TXE"))
  , mBTMEnable(false)
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
}

/*
 *  Destructor.
 */
IasAvbTransmitEngine::~IasAvbTransmitEngine()
{
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  cleanup();
}


/*
 *  Initialization method.
 */
IasAvbProcessingResult IasAvbTransmitEngine::init()
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cBootTimeMeasurement, mBTMEnable);
  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  if (isInitialized())
  {
    result = eIasAvbProcInitializationFailed;
  }

  mIgbDevice = IasAvbStreamHandlerEnvironment::getIgbDevice();
  if (NULL == mIgbDevice)
  {
    /**
     * @log Init failed: Returned igbDevice == NULL
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mIgbDevice == NULL!");
    result = eIasAvbProcInitializationFailed;
  }

  if (eIasAvbProcOK == result)
  {
    uint64_t val = 0u;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitUseShaper, val);
    mUseShaper = (0u != val);

    int32_t err = -1;
    uint32_t errCount = 0u;
    uint32_t timeoutCnt  = 0u;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cIgbAccessTimeoutCnt, timeoutCnt);

    // Retry until igb_avb is ready.
    while ((0 != err) && (timeoutCnt > errCount))
    {
      err = igb_set_class_bandwidth( mIgbDevice, 0u, 0u, 1500u, 64u ); /* Qav disabled*/
      if (0u != err)
      {
        ::usleep(cIgbAccessSleep);
        errCount ++;
      }
      else
      {
        if (0u != errCount)
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Couldn't configure shaper!(",
              strerror(err), " error count:", (errCount), ")");
          // do not abort, try running without config
        }
      }
    }
  }

  // this should actually be redundant
  for (uint32_t i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
  {
    mSequencers[i] = NULL;
  }
  if (eIasAvbProcOK != result)
  {
    cleanup();
  }
  return result;
}


void IasAvbTransmitEngine::cleanup()
{
  (void) stop();

  for (uint32_t i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
  {
    delete mSequencers[i];
    mSequencers[i] = NULL;
  }

  for (AvbStreamMap::iterator it = mAvbStreams.begin(); it != mAvbStreams.end(); it++)
  {
    IasAvbStream * s = it->second;
    AVB_ASSERT( NULL != s );

    std::stringstream ssStreamId;
    ssStreamId << "0x" << std::hex << s->getStreamId();
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "destroying stream", ssStreamId.str());
    delete s;
  }

  mIgbDevice = NULL;
  mAvbStreams.clear();
}

IasAvbProcessingResult IasAvbTransmitEngine::registerEventInterface( IasAvbStreamHandlerEventInterface * eventInterface )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized()) // Is transmit engine initialized?
  {
    result = eIasAvbProcNotInitialized;
  }
  else if (NULL == eventInterface)
  {
    result = eIasAvbProcInvalidParam;
  }
  else if (NULL != mEventInterface) // Already registered ?
  {
    result = eIasAvbProcAlreadyInUse;
  }
  else
  {
    mEventInterface = eventInterface;
  }

  return result;
}

IasAvbProcessingResult IasAvbTransmitEngine::unregisterEventInterface( IasAvbStreamHandlerEventInterface * eventInterface )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized()) // Is transmit engine initialized?
  {
    result = eIasAvbProcNotInitialized;
  }
  else if ((NULL == eventInterface) || (mEventInterface != eventInterface))
  {
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    mEventInterface = NULL;
  }

  return result;
}

IasAvbTransmitSequencer * IasAvbTransmitEngine::getSequencerByStream(IasAvbStream *stream) const
{
  AVB_ASSERT(NULL != stream);
  return getSequencerByClass(stream->getTSpec().getClass());
}

IasAvbTransmitSequencer * IasAvbTransmitEngine::getSequencerByClass(IasAvbSrClass qavClass) const
{
  IasAvbTransmitSequencer * ret = NULL;

  for (uint32_t i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
  {
    if ((NULL == mSequencers[i]) || (mSequencers[i]->getClass() == qavClass))
    {
      ret = mSequencers[i];
      break;
    }
  }

  return ret;
}


void IasAvbTransmitEngine::updateLinkStatus(const bool linkIsUp)
{
  AVB_ASSERT(isInitialized());

  if (linkIsUp)
  {
    if (mBTMEnable)
    {
      // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb Link check: UP",NULL);
    }
    if (mUseShaper)
    {
      // after link is back, igb_avb will reset the shapers
      for (uint32_t i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
      {
        IasAvbTransmitSequencer *seq = mSequencers[i];
        if (NULL != seq)
        {
          seq->updateShaper();
        }
      }
    }
    else
    {
      if (mIgbDevice)
      {
        const int32_t err = igb_set_class_bandwidth( mIgbDevice, 0u, 0u, 1500u, 64u ); /* Qav disabled*/
        if (err < 0)
        {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Couldn't configure shaper: ",
                strerror(err));
        }
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "mIgbDevice == NULL!");
      }
    }
  }

  if (NULL != mEventInterface)
  {
    mEventInterface->updateLinkStatus(linkIsUp);
  }
}

void IasAvbTransmitEngine::updateStreamStatus(uint64_t streamId, IasAvbStreamState status)
{
  if (NULL != mEventInterface)
  {
    mEventInterface->updateStreamStatus(streamId, status);
  }
}


IasAvbProcessingResult IasAvbTransmitEngine::start()
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else if (isRunning())
  {
  // nothing to do
  }
  else
  {
    if (mUseResume)
    {
      if (mIgbDevice)
      {
        int32_t err = -1;
        uint32_t errCount = 0u;
        uint32_t timeoutCnt = 0u;

        (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cIgbAccessTimeoutCnt, timeoutCnt);
        // Retry until igb_avb is ready.
        err = -1;
        while ((0 != err) && (timeoutCnt > errCount))
        {
          err = igb_set_class_bandwidth( mIgbDevice, 0u, 0u, 1500u, 64u ); /* Qav disabled*/
          if (0u != err)
          {
            ::usleep(cIgbAccessSleep);
            errCount ++;
          }
          else
          {
            if (0u != errCount)
            {
              DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Couldn't configure shaper!(",
                  strerror(err), " error count:", errCount, ")");
              // do not abort, try running without config
            }
          }
        }
      }
    }
    mUseResume = true;

    for (uint32_t i = 0u; (i < IasAvbTSpec::cIasAvbNumSupportedClasses) && (eIasAvbProcOK == result); i++)
    {
      IasAvbTransmitSequencer *seq = mSequencers[i];
      if (NULL != seq)
      {
        result = seq->start();
      }
      else
      {
        break;
      }
    }
  }

  if (eIasAvbProcOK != result)
  {
    (void) stop();
  }
  else
  {
    mRunning = true;
    if (mBTMEnable)
    {
      // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb TX Engine started",NULL);
    }
  }


  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::stop()
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX);

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else if (!isRunning())
  {
    // nothing to do
  }
  else
  {
    for (uint32_t i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
    {
      IasAvbTransmitSequencer *seq = mSequencers[i];
      if (NULL != seq)
      {
        IasAvbProcessingResult res = seq->stop();
        if (eIasAvbProcOK != res)
        {
          result = res;
        }
      }
      else
      {
        break;
      }
    }
  }

  mRunning = false;

  return result;
}

IasAvbProcessingResult IasAvbTransmitEngine::createSequencerOnDemand(IasAvbSrClass qavClass)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  AVB_ASSERT(isInitialized());

  // sequencer already existing?
  if (NULL == getSequencerByClass(qavClass))
  {
    // search list for free entry
    uint32_t i;
    for (i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
    {
      if (NULL == mSequencers[i])
      {
        break;
      }
    }

    if (IasAvbTSpec::cIasAvbNumSupportedClasses == i)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Trying to create TX sequencers for more than", int32_t(IasAvbTSpec::cIasAvbNumSupportedClasses), "SR classes");
      result = eIasAvbProcNoSpaceLeft;
    }

    if (eIasAvbProcOK == result)
    {
      /*
       *  determine I210 queue for this SR class: "high" class always goes to Q0, "low" class B always to Q1
       */
      uint32_t q = 0;
      std::string dltCtxName = "_TX";
      switch (qavClass)
      {
      case IasAvbSrClass::eIasAvbSrClassHigh:
        dltCtxName += "1";
        q = 0u;
        break;

      case IasAvbSrClass::eIasAvbSrClassLow:
        dltCtxName += "2";
        q = 1u;
        break;

      default:
        result = eIasAvbProcErr;
        AVB_ASSERT(false);
      }

      DltContext &ctx = IasAvbStreamHandlerEnvironment::getDltContext(dltCtxName);
      IasAvbTransmitSequencer *seq = new (nothrow) IasAvbTransmitSequencer(ctx);

      if (NULL == seq)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Couldn't create transmit sequencer");
        result = eIasAvbProcNotEnoughMemory;
      }
      else
      {
        if (eIasAvbProcOK == result)// does this condition make sense here with assert in line 449?
        {
          result = seq->init(q, qavClass, (i == 0u));
        }

        // use the first sequencer created to get link status events
        if ((0u == i) && (eIasAvbProcOK == result))
        {
          result = seq->registerEventInterface(this);
        }

        if ((eIasAvbProcOK == result) && isRunning())
        {
          result = seq->start();
        }

        if (eIasAvbProcOK == result)
        {
          mSequencers[i] = seq;
          if (mBTMEnable)
          {
            // TO BE REPLACED ias_dlt_log_btm_mark(mLog, "avb started TX sequencer on demand",NULL);
          }
        }
        else
        {
          delete seq;
        }
      }
    }
  }
  return result;
}



IasAvbProcessingResult IasAvbTransmitEngine::createTransmitAudioStream(IasAvbSrClass srClass,
                                            uint16_t const maxNumberChannels,
                                            uint32_t const sampleFreq,
                                            IasAvbAudioFormat const format,
                                            IasAvbClockDomain * const clockDomain,
                                            const IasAvbStreamId & streamId,
                                            const IasAvbMacAddress & destMacAddr,
                                            bool preconfigured)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);

  if (mAvbStreams.end() != mAvbStreams.find(streamId))
  {
    /**
     * @log Invalid param: The streamId parameter provided is already assigned to another stream.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "streamId already exists!");
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    IasAvbAudioStream *newAudioStream = new (nothrow) IasAvbAudioStream();
    if (NULL != newAudioStream)
    {
      result = newAudioStream->initTransmit(srClass, maxNumberChannels, sampleFreq, format, streamId, 60u,
              clockDomain, destMacAddr, preconfigured);

      if (eIasAvbProcOK == result)
      {
        result = createSequencerOnDemand(newAudioStream->getTSpec().getClass());
      }

      if (eIasAvbProcOK == result)
      {
        // add stream to vector
        mAvbStreams[streamId] = newAudioStream;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "init transmit stream failed! Error =", int32_t(result));
        delete newAudioStream;
      }
    }
    else
    {
      /**
       * @log Not enough memory: Couldn't create a new IasAvbAudioStream.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate AudioStream!\n");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::createTransmitVideoStream(IasAvbSrClass srClass,
                                            uint16_t const maxPacketRate,
                                            uint16_t const maxPacketSize,
                                            IasAvbVideoFormat const format,
                                            IasAvbClockDomain * const clockDomain,
                                            const IasAvbStreamId & streamId,
                                            const IasAvbMacAddress & destMacAddr,
                                            bool preconfigured)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);

  if (mAvbStreams.end() != mAvbStreams.find(streamId))
  {
    /**
     * @log Invalid param: The streamId parameter is already being used by another stream.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "streamId already exists!");
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    IasAvbVideoStream *newVideoStream = new (nothrow) IasAvbVideoStream();
    if (NULL != newVideoStream)
    {
      uint32_t poolSize = 1000u;
      (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cXmitVideoPoolsize, poolSize);

      result = newVideoStream->initTransmit(srClass, maxPacketRate, maxPacketSize, format, streamId, poolSize,
              clockDomain, destMacAddr, preconfigured);

      if (eIasAvbProcOK == result)
      {
        result = createSequencerOnDemand(newVideoStream->getTSpec().getClass());
      }

      if (eIasAvbProcOK == result)
      {
        // add stream to vector
        mAvbStreams[streamId] = newVideoStream;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "init transmit stream failed! Error =", int32_t(result));
        delete newVideoStream;
      }
    }
    else
    {
      /**
       * @log Not enough memory: Could not create a new instance of the Transmit Video stream
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate video stream!\n");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::createTransmitClockReferenceStream(IasAvbSrClass srClass,
                                            IasAvbClockReferenceStreamType type,
                                            uint16_t crfStampsPerPdu,
                                            uint16_t crfStampInterval,
                                            uint32_t baseFreq,
                                            IasAvbClockMultiplier pull,
                                            IasAvbClockDomain * const clockDomain,
                                            const IasAvbStreamId & streamId,
                                            const IasAvbMacAddress & destMacAddr)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX);

  if (mAvbStreams.end() != mAvbStreams.find(streamId))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "streamId already exists!");
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    IasAvbClockReferenceStream *newStream = new (nothrow) IasAvbClockReferenceStream();
    if (NULL != newStream)
    {
      uint32_t poolSize = 10u;

      result = newStream->initTransmit(srClass, type, crfStampsPerPdu, crfStampInterval, baseFreq, pull, streamId, poolSize,
              clockDomain, destMacAddr);

      if (eIasAvbProcOK == result)
      {
        result = createSequencerOnDemand(newStream->getTSpec().getClass());
      }

      if (eIasAvbProcOK == result)
      {
        // add stream to vector
        mAvbStreams[streamId] = newStream;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "init transmit stream failed! Error =", int32_t(result));
        delete newStream;
      }
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Not enough memory to instantiate stream!\n");
      result = eIasAvbProcNotEnoughMemory;
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::destroyAvbStream(IasAvbStreamId const & streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;
  AvbStreamMap::iterator it = mAvbStreams.find(streamId);

  if (it == mAvbStreams.end())
  {
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    IasAvbStream *avbStream = it->second;

    result = deactivateAvbStream(streamId);

    if (eIasAvbProcOK == result)
    {
      mAvbStreams.erase(streamId);

      delete avbStream;
    }
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::activateAvbStream(IasAvbStreamId const & streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // search for specified stream ID in the map
  AvbStreamMap::iterator mIt = mAvbStreams.find(streamId);
  if (mIt != mAvbStreams.end())
  {
    AVB_ASSERT(NULL != mIt->second);
    IasAvbStream *stream = mIt->second;

    if (!stream->isActive())
    {
      IasAvbTransmitSequencer *seq = getSequencerByStream(stream);
      AVB_ASSERT(NULL != seq);

      result = seq->addStreamToTransmitList(stream);

      if (eIasAvbProcOK == result)
      {
        stream->activate();

        if ((mUseShaper) && (IasAvbSrClass::eIasAvbSrClassHigh == seq->getClass()))
        {
          IasAvbTransmitSequencer *seqLow = getSequencerByClass(IasAvbSrClass::eIasAvbSrClassLow);
          if (NULL != seqLow)
          {
            seqLow->setMaxFrameSizeHigh(seq->getMaxFrameSizeHigh());
          }
        }
      }
    }
  }
  else
  {
    result = eIasAvbProcInvalidParam;
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::deactivateAvbStream(IasAvbStreamId const & streamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // search for specified stream ID in the map
  AvbStreamMap::iterator mIt = mAvbStreams.find(streamId);
  if (mIt != mAvbStreams.end())
  {
    AVB_ASSERT(NULL != mIt->second);
    IasAvbStream *stream = mIt->second;

    if (stream->isActive())
    {
      IasAvbTransmitSequencer *seq = getSequencerByStream(stream);
      AVB_ASSERT(NULL != seq);

      stream->deactivate();

      result = seq->removeStreamFromTransmitList(stream);

      if (result == eIasAvbProcOK)
      {
        if (stream->isActive())
        {
          /*
           * Fix for defect#230217
           *
           * Stream state could revert to active by TX sequencer if stream reset happened for any reasons
           * between the prior deactivate() call and the removeStreamFromTransmitList() call because
           * the stream being removed was still in the sequencer list during the period. Make sure to
           * deactivate the stream here in order to avoid redundant call of removeStreamFromTransmitList()
           * which may happen on next DestroyStream request otherwise it would corrupt bandwidth calculation.
           * (i.e. deactivateAvbStream() can be called twice if application cleans up TX stream by issuing
           * both setStreamActive(deactivate) and destroyStream API calls.)
           */
          stream->deactivate();

          std::ostringstream strStreamId;
          strStreamId << std::hex << streamId;
          DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "stream", strStreamId.str(), "state reverted, deactivating again");
        }

        if ((mUseShaper) && (IasAvbSrClass::eIasAvbSrClassHigh == seq->getClass()))
        {
          IasAvbTransmitSequencer *seqLow = getSequencerByClass(IasAvbSrClass::eIasAvbSrClassLow);
          if (NULL != seqLow)
          {
            seqLow->setMaxFrameSizeHigh(seq->getMaxFrameSizeHigh());
          }
        }
      }
      else
      {
        // restore the stream otherwise next call of the method will not remove the stream from the list
        stream->activate();
      }
    }
  }
  else
  {
    result = eIasAvbProcInvalidParam;
  }

  return result;
}

void IasAvbTransmitEngine::updateShapers()
{
  uint32_t bwHigh = 0;
  uint32_t bwLow = 0;
  for (uint32_t i = 0u; i < IasAvbTSpec::cIasAvbNumSupportedClasses; i++)
  {
    IasAvbTransmitSequencer *seq = mSequencers[i];
    if (NULL != seq)
    {
      switch (seq->getClass())
      {
      case IasAvbSrClass::eIasAvbSrClassHigh:
        bwHigh = seq->getCurrentBandwidth();
        break;

      case IasAvbSrClass::eIasAvbSrClassLow:
        bwLow = seq->getCurrentBandwidth();
        break;

      default:
        // unsupported configuration should have been caught already
        AVB_ASSERT(false);
      }
    }
  }

  /**
    * @log The values for the current bandwidth, class A -> High, class B -> Low.
    */
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "class A:", bwHigh,
      "class B:", bwLow);


  /**
   * Convert bandwidth values from "per second" to "per observation interval",
   * always rounding up. Note: libigb has the observation intervals
   * hard-coded for class A and B!
   */
  if (bwHigh > 0u)
  {
    bwHigh = (bwHigh + 7999u) / 8000u;
  }

  if (bwLow > 0u)
  {
    bwLow = (bwLow + 3999u) / 4000u;
  }

  /* hack: call the igb function with a pseudo packet size of 83 bytes payload, since this would results
   * in a packet on the wire of exactly 1000bits, which enables us to use the class_a and class_b
   * parameters to specify the bandwidth in kbit/observationInterval
   */
  const int32_t err = igb_set_class_bandwidth(mIgbDevice, bwHigh, bwLow, 83u, 83u);
  if (err < 0)
  {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "Couldn't configure shaper: ",
          strerror(err));
  }
}

IasAvbProcessingResult IasAvbTransmitEngine::connectAudioStreams(const IasAvbStreamId & avbStreamId, IasLocalAudioStream * localStream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // look-up AVB stream using the specified AVB stream id
  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);
  if (it != mAvbStreams.end())
  {
    // if AVB stream has been found, hand-over instance of local
    // audio stream to the AVB audio stream so they can connect
    if (eIasAvbAudioStream == it->second->getStreamType())
    {
      result = static_cast<IasAvbAudioStream*>(it->second)->connectTo(localStream);
    }
    else
    {
      /**
       * @log Invalid param: Local stream Id parameter cannot be found in the audio stream list.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Wrong type! AvbStreamId does not correspond to an AvbAudioStream");

      result = eIasAvbProcErr;
    }
  }
  else // no stream has been found related to specified stream id
  {
    uint64_t sid = avbStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid streamId", sid);

    result = eIasAvbProcErr;
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::connectVideoStreams(const IasAvbStreamId & avbStreamId, IasLocalVideoStream * localStream)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // look-up AVB stream using the specified AVB stream id
  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);
  if (it != mAvbStreams.end())
  {
    // if AVB stream has been found, hand-over instance of local
    // audio stream to the AVB audio stream so they can connect
    if (eIasAvbVideoStream == it->second->getStreamType())
    {
      result = static_cast<IasAvbVideoStream*>(it->second)->connectTo(localStream);
    }
    else
    {
      /**
       * @log The StreamId parameter provided does not have a VideoStream type.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Wrong type! AvbStreamId does not correspond to an AvbVideoStream");

      result = eIasAvbProcErr;
    }
  }
  else // no stream has been found related to specified stream id
  {
    uint64_t sid = avbStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Invalid streamId", sid);

    result = eIasAvbProcErr;
  }

  return result;
}


IasAvbProcessingResult IasAvbTransmitEngine::disconnectStreams(const IasAvbStreamId & avbStreamId)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  // look-up AVB stream using the specified AVB stream id
  AvbStreamMap::iterator it = mAvbStreams.find(avbStreamId);
  if (it != mAvbStreams.end())
  {
    AVB_ASSERT(NULL != it->second);

    // if AVB stream has been found, check type and connect to NULL to disconnect,
    // else return error.
    IasAvbStreamType streamType = it->second->getStreamType();
    if (eIasAvbVideoStream == streamType)
    {
      result = static_cast<IasAvbVideoStream*>(it->second)->connectTo(NULL);
    }
    else if (eIasAvbAudioStream == streamType)
    {
      result = static_cast<IasAvbAudioStream*>(it->second)->connectTo(NULL);
    }
    else
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX,
        "Wrong type! AvbStreamId does not correspond to an AvbVideo or AvbAudio stream.");

      result = eIasAvbProcInvalidParam;
    }
  }
  else
  {
    uint64_t sid = avbStreamId;
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "::disconnectStreams: Invalid streamId", sid);

    result = eIasAvbProcInvalidParam;
  }

  return result;
}


bool IasAvbTransmitEngine::getAvbStreamInfo(const IasAvbStreamId &id,
                                            AudioStreamInfoList &audioStreamInfo,
                                            VideoStreamInfoList &videoStreamInfo,
                                            ClockReferenceStreamInfoList &clockRefStreamInfo) const
{
  uint64_t streamId = uint64_t(id);
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, " Stream Id =", streamId);
  bool found = false;
  for (AvbStreamMap::const_iterator it = mAvbStreams.begin(); it != mAvbStreams.end() && !found; ++it)
  {
    if (uint64_t(0u) == streamId || it->first == id)
    {
      IasAvbStream *stream = it->second;
      IasAvbStreamDiagnostics diag = stream->getDiagnostics();
      bool preConfigured = stream->getPreconfigured();
      // Add info into list
      if (IasAvbStreamType::eIasAvbAudioStream == stream->getStreamType())
      {
        IasAvbAudioStream *audioStream = static_cast<IasAvbAudioStream *>(stream);
        IasAvbAudioStreamAttributes att;
        att.setStreamId(it->first);
        att.setDirection(stream->getDirection());

        const IasAvbMacAddress &smac_array = audioStream->getSmac();
        const IasAvbMacAddress &dmac_array = audioStream->getDmac();
        uint64_t dmac = 0;
        uint64_t smac = 0;
        for (unsigned int i=0; i < cIasAvbMacAddressLength; ++i)
        {
          dmac = dmac << 8;
          dmac += dmac_array[i];

          smac = smac << 8;
          smac += smac_array[i];
        }
        att.setDmac(dmac);
        att.setSourceMac(smac);

        att.setTxActive(audioStream->isTransmitStream() && audioStream->isActive());
        att.setMaxNumChannels(audioStream->getMaxNumChannels());
        att.setSampleFreq(audioStream->getSampleFrequency());
        att.setFormat(audioStream->getAudioFormat());
        att.setNumChannels(audioStream->getLocalNumChannels());
        att.setLocalStreamId(audioStream->getLocalStreamId());

        if (NULL != audioStream->getAvbClockDomain())
        {
          att.setClockId(audioStream->getAvbClockDomain()->getClockDomainId());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " Avb Clock Domain was NULL!");
          att.setClockId(-1); // Debug hint of an issue.
        }

        att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
        att.setPreconfigured(preConfigured);

        att.setDiagnostics(diag);
        audioStreamInfo.push_back(att);
      }
      else if (IasAvbStreamType::eIasAvbVideoStream == stream->getStreamType()) // videoStream
      {
        IasAvbVideoStreamAttributes att;
        IasAvbVideoStream *videoStream = static_cast<IasAvbVideoStream *>(stream);
        att.setStreamId(it->first);
        att.setDirection(stream->getDirection());

        const IasAvbMacAddress &smac_array = videoStream->getSmac();
        const IasAvbMacAddress &dmac_array = videoStream->getDmac();
        uint64_t dmac = 0;
        uint64_t smac = 0;
        for (unsigned int i=0; i < cIasAvbMacAddressLength; ++i)
        {
          dmac = dmac << 8;
          dmac += dmac_array[i];

          smac = smac << 8;
          smac += smac_array[i];
        }
        att.setDmac(dmac);
        att.setSourceMac(smac);

        att.setTxActive(videoStream->isTransmitStream() && videoStream->isActive());

        att.setLocalStreamId(videoStream->getLocalStreamId());
        att.setFormat(videoStream->getAvbVideoFormat());
        att.setMaxPacketRate(videoStream->getMaxPacketRate());
        att.setMaxPacketSize(videoStream->getMaxPacketSize());

        if (NULL != videoStream->getAvbClockDomain())
        {
          att.setClockId(videoStream->getAvbClockDomain()->getClockDomainId());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " Avb Clock Domain was NULL!");
          att.setClockId(-1); // Debug hint of an issue.
        }

        att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
        att.setPreconfigured(preConfigured);

        att.setDiagnostics(diag);
        videoStreamInfo.push_back(att);
      }
      else if (IasAvbStreamType::eIasAvbClockReferenceStream == stream->getStreamType())
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " - Clock Reference");
        IasAvbClockReferenceStreamAttributes att;
        IasAvbClockReferenceStream *crfStream = static_cast<IasAvbClockReferenceStream *>(stream);
        att.setDirection(stream->getDirection());
        att.setType(crfStream->getCrfStreamType());
        att.setCrfStampsPerPdu(crfStream->getTimeStampsPerPdu());
        att.setCrfStampInterval(crfStream->getTimeStampInterval());
        att.setBaseFreq(crfStream->getBaseFrequency());
        att.setPull(crfStream->getClockMultiplier());
        att.setStreamId(it->first);

        if (NULL != crfStream->getAvbClockDomain())
        {
          att.setClockId(crfStream->getAvbClockDomain()->getClockDomainId());
        }
        else
        {
          DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " Avb Clock Domain was NULL!");
          att.setClockId(-1); // Debug hint of an issue.
        }

        const IasAvbMacAddress &smac_array = crfStream->getSmac();
        const IasAvbMacAddress &dmac_array = crfStream->getDmac();
        uint64_t dmac = 0;
        uint64_t smac = 0;
        for (unsigned int i=0; i < cIasAvbMacAddressLength; ++i)
        {
          dmac = dmac << 8;
          dmac += dmac_array[i];

          smac = smac << 8;
          smac += smac_array[i];
        }
        att.setDmac(dmac);
        att.setSourceMac(smac);

        att.setTxActive(crfStream->isTransmitStream() && crfStream->isActive());

        att.setAssignMode(IasAvbIdAssignMode::eIasAvbIdAssignModeStatic);
        att.setPreconfigured(preConfigured);

        att.setDiagnostics(diag);
        clockRefStreamInfo.push_back(att);
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, " Unknown stream type!");
      }

      if (it->first == id)
      {
        found = true;
      }
    }
  }

  return found;

}

} // namespace IasMediaTransportAvb
