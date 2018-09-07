/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file   IasAlsaHandlerWorkerThread.cpp
 * @date   2015
 * @brief  This class implements the worker thread that is applied if the ALSA handler works asynchronously.
 */

#include <iomanip>
#include <boost/algorithm/string/replace.hpp>
#include "avb_helper/IasThread.hpp"
#include "internal/audio/common/IasAudioLogging.hpp"
#include "internal/audio/common/audiobuffer/IasAudioRingBuffer.hpp"
#include "internal/audio/common/samplerateconverter/IasSrcFarrow.hpp"
#include "internal/audio/common/samplerateconverter/IasSrcController.hpp"
#include "internal/audio/common/helper/IasCopyAudioAreaBuffers.hpp"
//#include "smartx/IasThreadNames.hpp"
#include "internal/audio/common/helper/IasCopyAudioAreaBuffers.hpp"
#include "avb_streamhandler/IasAlsaHandlerWorkerThread.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbRegistryKeys.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#ifndef RW_TMP_PATH
#define RW_TMP_PATH "/tmp/"
#endif

using namespace IasAudio;
namespace IasMediaTransportAvb {


static const string cClassName = "IasAlsaHandlerWorkerThread::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"
#define LOG_DEVICE "device=" + mParams->name + ":"


IasAlsaHandlerWorkerThread::IasAlsaHandlerWorkerThread(IasAlsaHandlerWorkerThreadParamsPtr params)
  :mLog(IasAudioLogging::registerDltContext("AHD", "ALSA Handler"))
  ,mParams(params)
  ,mDeviceType(eIasDeviceTypeUndef)
  ,mSamplerate(0)
  ,mSamplerateIn(0)
  ,mSamplerateOut(0)
  ,mThread(nullptr)
  ,mThreadIsRunning(false)
  ,mSrc(nullptr)
  ,mSrcController(nullptr)
  ,mSrcInputBuffersFloat32(nullptr)
  ,mSrcInputBuffersInt32(nullptr)
  ,mSrcInputBuffersInt16(nullptr)
  ,mSrcOutputBuffersFloat32(nullptr)
  ,mSrcOutputBuffersInt32(nullptr)
  ,mSrcOutputBuffersInt16(nullptr)
  ,mDiagnosticsFileName("")
  ,mLogCnt(0)
  ,mLogInterval(0)
{
  IAS_ASSERT(mParams != nullptr);
  mNumChannels = params->asrcBufferParams.numChannels;
}

IasAlsaHandlerWorkerThread::~IasAlsaHandlerWorkerThread()
{
  stop();
  delete mThread;
  delete mSrc;
  delete mSrcController;

  delete[] mSrcInputBuffersFloat32;
  delete[] mSrcInputBuffersInt32;
  delete[] mSrcInputBuffersInt16;
  delete[] mSrcOutputBuffersFloat32;
  delete[] mSrcOutputBuffersInt32;
  delete[] mSrcOutputBuffersInt16;

  // Close the diagnostics stream, if it has been opened before.
  if (mDiagnosticsStream.is_open())
  {
    mDiagnosticsStream.close();
    if (mDiagnosticsStream.fail())
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, LOG_DEVICE, "Unable to close diagnostics file:", mDiagnosticsFileName);
    }
  }

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, LOG_DEVICE);
}


IasAlsaHandlerWorkerThread::IasResult IasAlsaHandlerWorkerThread::init(IasDeviceType deviceType, uint32_t avbSampleRate)
{
  IAS_ASSERT((deviceType == eIasDeviceTypeSource) || (deviceType == eIasDeviceTypeSink)); // already checked in IasAlsaHandler::init()
  IAS_ASSERT(mParams != nullptr);

  mDeviceType    = deviceType;
  mSamplerate    = mParams->samplerate;


  if(eIasDeviceTypeSource == deviceType)
  {
    mSamplerateIn  = mSamplerate;
    mSamplerateOut = avbSampleRate;         // AVBSH works for ALSA only with 48 or 24 kHz // ABu: ToDo: add 24 kHz
  }
  else
  {
    mSamplerateIn  = avbSampleRate;         // AVBSH works for ALSA only with 48 or 24 kHz // ABu: ToDo: add 24 kHz
    mSamplerateOut = mSamplerate;
  }

  uint32_t periodTime = mParams->deviceBufferParams.periodSize * 1000 / mSamplerate;
  mLogInterval =  (periodTime == 0) ? 1000 : 1000 / periodTime;

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE, "Initialization of Alsa Handler Worker Thread.");

  if (mThread == nullptr)
  {
    mThread = new IasMediaTransportAvb::IasThread(this, mParams->name);
    if (mThread == nullptr)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error while creating IasMediaTransportAvb::IasThread object");
      return eIasInitFailed;
    }
  }


  mSrc = new IasSrcFarrow();
  IAS_ASSERT(mSrc != nullptr);
  IasSrcFarrow::IasResult srcResult = mSrc->init(mNumChannels);
  if (srcResult != IasSrcFarrow::eIasOk)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                "Error while initializing sample rate converter:", toString(srcResult));
    return eIasInitFailed;
  }

  // ABu: ToDo: check/implement double Buffer with AlsaClock Domain here. (if no ASRC is needed)

  // Set the conversion ratio. Since the ASRC is used only for clock drift
  // compensation, the nominal output rate is equal to the nominal input rate.
  srcResult = mSrc->setConversionRatio(mSamplerateIn, mSamplerateOut);


  if (srcResult != IasSrcFarrow::eIasOk)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                "Error while setting conversion ratio:", toString(srcResult));
    return eIasInitFailed;
  }

  mSrc->setBufferMode(IasSrcFarrow::eIasLinearBufferMode);


  // Allocate the vectors for the pointers to the SRC input/output buffers
  mSrcInputBuffersFloat32  = new const float*[mNumChannels];
  mSrcOutputBuffersFloat32 = new float*[mNumChannels];
  mSrcInputBuffersInt32    = new const int32_t*[mNumChannels];
  mSrcOutputBuffersInt32   = new int32_t*[mNumChannels];
  mSrcInputBuffersInt16    = new const int16_t*[mNumChannels];
  mSrcOutputBuffersInt16   = new int16_t*[mNumChannels];

  // Initialize ASRC the closed-loop controller.
  mSrcController = new IasSrcController();
  IAS_ASSERT(mSrcController != nullptr);
  IasSrcController::IasResult srcControllerResult = mSrcController->init();
  if (srcControllerResult != IasSrcController::eIasOk)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                "Error while initializing closed loop controller for ASRC:", toString(srcControllerResult));
    return eIasInitFailed;
  }

  string diagnosticsFileNameTest  = "IasAlsaHandlerWorkerThread_createDiagnostics";
  string diagnosticsFileNameTrunk = string(RW_TMP_PATH) + "IasAlsaHandlerWorkerThread_Diagnostics";

  // Check whether there is a file whose name matches with diagnosticsFileNameTest.
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE,
              "Checking whether file", diagnosticsFileNameTest, "exists.");

  std::ifstream testStream;
  testStream.open(diagnosticsFileNameTest);
  if (testStream.fail())
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE,
                "ASRC diagnostics won't be created, since file", diagnosticsFileNameTest, "does not exist.");
  }
  else
  {
    testStream.close();

    // Open the file for saving the ASRC diagnostics.
    mDiagnosticsFileName = diagnosticsFileNameTrunk + "_" + toString(deviceType) + "_" + mParams->name + ".txt";

    // Replace characters that are not supported by NTFS,
    // since we want to analyze the diagnostics file on a Windows PC.
    boost::algorithm::replace_all(mDiagnosticsFileName, ":", "_");

    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE,
                "Writing ASRC diagnostics to file:", mDiagnosticsFileName);

    mDiagnosticsStream.open(mDiagnosticsFileName);
    if (mDiagnosticsStream.fail())
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, LOG_DEVICE,
                  "Error while opening ASRC diagnostics file:", mDiagnosticsFileName);
    }
  }

  return eIasOk;
}


IasAlsaHandlerWorkerThread::IasResult IasAlsaHandlerWorkerThread::start()
{
  if (mThread == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error due to non-initialized component");
    return eIasNotInitialized;
  }

  mThreadIsRunning = true;
  mThread->start(true);
  return eIasOk;
}


IasAlsaHandlerWorkerThread::IasResult IasAlsaHandlerWorkerThread::stop()
{
  if (mThread == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error due to non-initialized component (mThread == nullptr)");
    return eIasNotInitialized;
  }

  mThread->stop();
  return eIasOk;
}


IasMediaTransportAvb::IasResult IasAlsaHandlerWorkerThread::beforeRun()
{
  return IasMediaTransportAvb::IasResult::cOk;
}


IasMediaTransportAvb::IasResult IasAlsaHandlerWorkerThread::run()
{
  struct sched_param sparam;
  std::string policyStr = "fifo";   // these values get overwritten by the default settings (fifo, prio=20) or
  int32_t priority      = 1;        // other values are specified in commnand line via '-k' option
  int rc                = 0;        // POSIX return codes

  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPolicy, policyStr);
  (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cSchedPriority, priority);

  const int32_t  policy = (policyStr == "other") ? SCHED_OTHER : (policyStr == "rr") ? SCHED_RR : SCHED_FIFO;
  sparam.sched_priority = priority;

  // Set Thread params(default realtime prio = 20)
  rc = pthread_setschedparam(pthread_self(), policy, &sparam);
  if(0 != rc)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "Error setting scheduler parameter: ", strerror(rc));
  }

  IasAudioRingBufferResult result;
  IasAudioRingBuffer*      deviceBufferHandle = mParams->deviceBufferParams.ringBuffer;
  IasAudioRingBuffer*      asrcBufferHandle   = mParams->asrcBufferParams.ringBuffer;
  uint32_t const           periodSize         = mParams->deviceBufferParams.periodSize;
  uint32_t const           numChannels        = mParams->deviceBufferParams.numChannels;
  uint64_t                 preSamplerateMHz   = 0;

  if(eIasDeviceTypeSource == mDeviceType)
  {
    // Audio sample rate in MHz in 48Q16 representation.
    // We do the division by 1000000 here, so we do not have to do it within the real-time thread.
    preSamplerateMHz = (mSamplerateIn * (1 << 16)) / 1000000;
  }
  else
  {
    // Audio sample rate in MHz in 48Q16 representation.
    // We do the division by 1000000 here, so we do not have to do it within the real-time thread.
    preSamplerateMHz = (mSamplerateOut * (1 << 16)) / 1000000;
  }

  uint64_t const samplerateMHz = preSamplerateMHz;

//  // Audio sample rate in MHz in 48Q16 representation.
//  // We do the division by 1000000 here, so we do not have to do it within the real-time thread.
//  uint64_t const           samplerateMHz      = (mSamplerate * (1 << 16)) / 1000000;

  IasAudioCommonDataFormat asrcBufferDataFormat;
  result = asrcBufferHandle->getDataFormat(&asrcBufferDataFormat);
  IAS_ASSERT(result == eIasRingBuffOk);

  IasAudioCommonDataFormat deviceBufferDataFormat;
  result = deviceBufferHandle->getDataFormat(&deviceBufferDataFormat);
  IAS_ASSERT(result == eIasRingBuffOk);

  // ASRC buffer and device buffer shall have the same dataFormat.
  IAS_ASSERT(asrcBufferDataFormat == deviceBufferDataFormat);

  // Length of the ASRC buffer.
  uint32_t asrcBufferLength = mParams->asrcBufferParams.numPeriods * mParams->asrcBufferParams.periodSize;

  // The target level of the ASRC buffer shall be 50% of the asrcBufferLength plus one half of a periodSize,
  // since the number of virtual samples is somewhere between 0 and periodSize.
  uint32_t    asrcBufferTargetLevel = (asrcBufferLength + mParams->asrcBufferParams.periodSize) >> 1;

  IasSrcController::IasResult srcControllerResult = mSrcController->setJitterBufferParams(asrcBufferLength, asrcBufferTargetLevel);
  (void) srcControllerResult;
  IAS_ASSERT(srcControllerResult == IasSrcController::eIasOk);
  mSrcController->reset();

  uint64_t     cntPeriodsDeviceBuffer = 0;
  uint64_t     cntPeriodsAsrcBuffer   = 0;
  bool         transferActive         = false;
  bool         startupFinished        = false;
  float        ratioAdaptive          = 1.0f;
  uint32_t     numTotalFramesPrevious = 0;

  // Upper limit for numVirtualFrames for being valid. In a perfect environment,
  // numVirtualFrames should be always between 0 and periodSize, but we add a
  // margin of periodSize/4 for increased robustness against thread execution latency.
  const int64_t cNumVirtualFramesUpperLimit = static_cast<int64_t>(periodSize + (periodSize >> 2));

  // Depending on the device type (source or sink), define the access type (read or write)
  // for the ring buffers. For the asrcBuffer, we also define the access type of the remote
  // side, which is the inverse of the access type of the local side.
  IasRingBufferAccess  deviceBufferAccessType;
  IasRingBufferAccess  asrcBufferAccessType;
  IasRingBufferAccess  asrcBufferAccessTypeRemote;
  if (mDeviceType == eIasDeviceTypeSource)
  {
    // Source device: Streaming direction from Device Buffer to ASRC Buffer
    deviceBufferAccessType     = IasRingBufferAccess::eIasRingBufferAccessRead;
    asrcBufferAccessType       = IasRingBufferAccess::eIasRingBufferAccessWrite;
    asrcBufferAccessTypeRemote = IasRingBufferAccess::eIasRingBufferAccessRead;
  }
  else
  {
    // Sink device: Streaming direction from ASRC Buffer to Device Buffer
    asrcBufferAccessTypeRemote = IasRingBufferAccess::eIasRingBufferAccessWrite;
    asrcBufferAccessType       = IasRingBufferAccess::eIasRingBufferAccessRead;
    deviceBufferAccessType     = IasRingBufferAccess::eIasRingBufferAccessWrite;
  }

  // Prefill the ASRC buffer until there is less free space than the target fill level
  asrcBufferHandle->zeroOut();
  if (asrcBufferAccessType == eIasRingBufferAccessRead)
  {
    asrcBufferHandle->resetFromReader();
  }
  else
  {
    asrcBufferHandle->resetFromWriter();
  }
  uint32_t asrcBufferNumFramesAvailable = 0;
  do
  {
    result = asrcBufferHandle->updateAvailable(asrcBufferAccessType, &asrcBufferNumFramesAvailable);
    IAS_ASSERT(result == IasAudioRingBufferResult::eIasRingBuffOk);

    if (asrcBufferNumFramesAvailable >= asrcBufferTargetLevel)
    {
      bufferAdjustFrames(asrcBufferHandle, asrcBufferAccessType, asrcBufferDataFormat, periodSize, numChannels);
    }
    if (asrcBufferNumFramesAvailable >= periodSize)
    {
      asrcBufferNumFramesAvailable -= periodSize;
    }
  }
  while (asrcBufferNumFramesAvailable >= asrcBufferTargetLevel);
  reset();


  while (mThreadIsRunning == true)
  {
    // Call the updateAvailable method of the ALSA device
    // and identify the number of frames that are available.
    uint32_t deviceBufferNumFramesAvailable = 0;
    result = deviceBufferHandle->updateAvailable(deviceBufferAccessType, &deviceBufferNumFramesAvailable);

    // Count the number of periods transferred by the device buffer.
    cntPeriodsDeviceBuffer++;

    if (result == IasAudioRingBufferResult::eIasRingBuffTimeOut)
    {
      if ( (mLogCnt > mLogInterval) || (mLogCnt == 0) )
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, LOG_DEVICE,
                    "Timeout during IasAudioRingBuffer::updateAvailable. Trying to continue.");
        mLogCnt = 0;
      }
      mLogCnt++;

      deviceBufferNumFramesAvailable = 0;
    }
    else if (result != IasAudioRingBufferResult::eIasRingBuffOk)
    {
      if ( (mLogCnt > mLogInterval) || (mLogCnt == 0) )
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                    "Error during IasAudioRingBuffer::updateAvailable:", toString(result));
        mLogCnt = 0;
      }
      mLogCnt++;
      return IasMediaTransportAvb::IasResult::cFailed;
    }
    else
    {
      mLogCnt = 0;
    }

    // Call the updateAvailable method of the ASRC buffer and identify the number
    // of frames that are available. The function call must be successful, since
    // a ring buffer of type real cannot claim a timeout.
    result = asrcBufferHandle->updateAvailable(asrcBufferAccessType, &asrcBufferNumFramesAvailable);
    IAS_ASSERT(result == IasAudioRingBufferResult::eIasRingBuffOk);

    if (asrcBufferNumFramesAvailable >= periodSize)
    {
      // Count the number of periods transferred by the ASRC buffer.
      cntPeriodsAsrcBuffer++;
    }
    else
    {
      // If the ASRC buffer has not provided enough PCM frames, we change into the start-up phase (again).
      // By means of this, we avoid that the drift estimator starts to wobble if the PCM stream stalls.
      startupFinished        = false;
      cntPeriodsAsrcBuffer   = 0;
      cntPeriodsDeviceBuffer = 0;
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE,
                  "Fall back to Start-up phase.");
      asrcBufferHandle->zeroOut();
      if (asrcBufferAccessType == eIasRingBufferAccessRead)
      {
        asrcBufferHandle->resetFromReader();
      }
      else
      {
        asrcBufferHandle->resetFromWriter();
      }
      do
      {
        result = asrcBufferHandle->updateAvailable(asrcBufferAccessType, &asrcBufferNumFramesAvailable);
        IAS_ASSERT(result == IasAudioRingBufferResult::eIasRingBuffOk);

        if (asrcBufferNumFramesAvailable >= asrcBufferTargetLevel)
        {
          bufferAdjustFrames(asrcBufferHandle, asrcBufferAccessType, asrcBufferDataFormat, periodSize, numChannels);
          cntPeriodsAsrcBuffer++;
        }
        if (asrcBufferNumFramesAvailable >= periodSize)
        {
          asrcBufferNumFramesAvailable -= periodSize;
        }
      }
      while (asrcBufferNumFramesAvailable >= asrcBufferTargetLevel);
      reset();
    }

    IasAudioTimestamp  audioTimestampDeviceBuffer;
    IasAudioTimestamp  audioTimestampAsrcBufferLocal;
    IasAudioTimestamp  audioTimestampAsrcBufferRemote;

    result = deviceBufferHandle->getTimestamp(eIasRingBufferAccessUndef, &audioTimestampDeviceBuffer);
    IAS_ASSERT(result == IasAudioRingBufferResult::eIasRingBuffOk);

    result = asrcBufferHandle->getTimestamp(asrcBufferAccessType, &audioTimestampAsrcBufferLocal);
    IAS_ASSERT(result == IasAudioRingBufferResult::eIasRingBuffOk);

    result = asrcBufferHandle->getTimestamp(asrcBufferAccessTypeRemote, &audioTimestampAsrcBufferRemote);
    IAS_ASSERT(result == IasAudioRingBufferResult::eIasRingBuffOk);

    int64_t bufferDifftime = (static_cast<int64_t>(audioTimestampDeviceBuffer.timestamp) -
                                 static_cast<int64_t>(audioTimestampAsrcBufferRemote.timestamp));

    // Calculate the number of virtual PCM frames:
    // bufferDifftime is expressed in microseconds, samplerateMHz is expressed in MHz in 48Q16 representation
    int64_t  numVirtualFrames = (bufferDifftime * samplerateMHz) >> 16;
    uint32_t numTotalFrames   = asrcBufferNumFramesAvailable + static_cast<uint32_t>(numVirtualFrames);

    // To detect spikes, we calculate the difference between the total number of frames
    // during this period and the total number of frames during the previous period.
    uint32_t numTotalFramesDiff = static_cast<uint32_t>(std::abs(static_cast<int32_t>(numTotalFrames) -
                                                                 static_cast<int32_t>(numTotalFramesPrevious)));
    numTotalFramesPrevious = numTotalFrames;

    // Verify whether the number of virtual samples is feasible.
    bool numVirtualFramesValid = ((numVirtualFrames >= 0) &&
                                  (numVirtualFrames <= cNumVirtualFramesUpperLimit) &&
                                  (numTotalFramesDiff < (periodSize >> 2)));

    // Terminate the start-up phase, if both buffers (ASRC buffer and device buffer) have transferred 4 periods.
    if ((!startupFinished) && (cntPeriodsAsrcBuffer > 4) && (cntPeriodsDeviceBuffer >= 4) && numVirtualFramesValid)
    {
      startupFinished = true;
      DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE,
                  "Start-up phase has been finished. ASRC closed-loop controller becomes active now.");

      // Verify whether we have to skip frames (if we read from ASRC buffer)
      // or insert zero-valued frames (if we write to ASRC buffer).
      if (numTotalFrames > asrcBufferTargetLevel)
      {
        uint32_t    numFramesToAdjust = numTotalFrames - asrcBufferTargetLevel;
        DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE,
                    "asrcBufferNumFramesAvailable:", asrcBufferNumFramesAvailable,
                    "Adjusting by", -static_cast<int32_t>(numFramesToAdjust), "frames");
        bufferAdjustFrames(asrcBufferHandle, asrcBufferAccessType, asrcBufferDataFormat, numFramesToAdjust, numChannels);
        asrcBufferNumFramesAvailable -= numFramesToAdjust;
      }
    }

    // Decide whether we shall transfer PCM frames from the ASRC buffer and to sink device buffer
    // (or from the source device buffer to the ASRC buffer). If the start-up phase is terminated,
    // we always transfer PCM frames. During the start-up phase, we do the following:
    // - If the device is a sink device, we stream PCM frames from the ASRC buffer to the sink device,
    //   as soon as the ASRC buffer provides more PCM frames than the target fill level.
    // - If the device is a source device, we stream PCM frames from the source device to the ASRC buffer,
    //   as long as the ASRC buffer provides space for more PCM frames than the target fill level.
    // Therefore, we have the same condition for source devices and sink devices:
    transferActive = (startupFinished || (asrcBufferNumFramesAvailable >= asrcBufferTargetLevel));

    // Execute the ASRC closed loop controller for drift estimation, as soon as the start-up phase is terminated.
    if (startupFinished)
    {
      if (numVirtualFramesValid)
      {
        bool outputActive;
        IasSrcController::IasResult srcControllerResult = mSrcController->process(&ratioAdaptive, &outputActive, numTotalFrames);
        (void) srcControllerResult;
        IAS_ASSERT(srcControllerResult == IasSrcController::eIasOk);
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, LOG_DEVICE,
                    "invalid diff time between ASRC ring buffer read and write access:", bufferDifftime, "us",
                    "device time:", audioTimestampDeviceBuffer.timestamp,
                    "asrc time: ", audioTimestampAsrcBufferRemote.timestamp);
      }
    }

    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, LOG_DEVICE,
                "asrcBufferNumFramesAvailable:", asrcBufferNumFramesAvailable,
                "physical + virtual frames:", numTotalFrames,
                "ratioAdaptive:", ratioAdaptive);

    if (mDiagnosticsStream.is_open())
    {
      // Write ASRC diagnostics into the diagnostics stream.
      mDiagnosticsStream << audioTimestampDeviceBuffer.timestamp          << ", "
                         << audioTimestampAsrcBufferRemote.timestamp      << ","
                         << std::setw(10) << audioTimestampDeviceBuffer.numTransmittedFrames     << ","
                         << std::setw(10) << audioTimestampAsrcBufferRemote.numTransmittedFrames << ","
                         << std::setw(6)  << asrcBufferNumFramesAvailable << ","
                         << std::setw(6)  << numTotalFrames               << ","
                         << std::setw(14) << ratioAdaptive - 1.0f         << std::endl;
    }
    if (deviceBufferNumFramesAvailable >= periodSize)
    {
      uint32_t deviceBufferOffset    = 0;
      uint32_t deviceBufferNumFrames = deviceBufferNumFramesAvailable;
      IasAudio::IasAudioArea* deviceBufferAreas = nullptr;

      result = deviceBufferHandle->beginAccess(deviceBufferAccessType, &deviceBufferAreas,
                                               &deviceBufferOffset, &deviceBufferNumFrames);
      if (result != IasAudioRingBufferResult::eIasRingBuffOk)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                    "Error during IasAudioRingBuffer::beginAccess:", toString(result));
        return IasMediaTransportAvb::IasResult::cFailed;
      }

      // Do not transfer more than periodSize from/to device buffer.
      deviceBufferNumFrames = std::min(deviceBufferNumFrames, periodSize);
      uint32_t deviceBufferNumFramesTransferred = 0;

      while (deviceBufferNumFrames > 0)
      {
        // Ask the ASRC buffer for the number of contiguous frames available.
        uint32_t asrcBufferOffset = 0;
        uint32_t asrcBufferNumFrames = periodSize;
        IasAudioArea* asrcBufferAreas = nullptr;
        result = asrcBufferHandle->beginAccess(asrcBufferAccessType, &asrcBufferAreas,
                                               &asrcBufferOffset, &asrcBufferNumFrames);
        IAS_ASSERT(result == eIasRingBuffOk);

        // Number of frames from device buffer or process buffer, which have
        // been processed (generated or consumed) by the sample rate converter.
        uint32_t deviceBufferNumProcessedFrames = 0;
        uint32_t asrcBufferNumProcessedFrames   = 0;

        if (transferActive)
        {
          transferFrames(deviceBufferAreas,
                         deviceBufferOffset + deviceBufferNumFramesTransferred,
                         deviceBufferNumFrames,
                         asrcBufferAreas,
                         asrcBufferOffset,
                         asrcBufferNumFrames,
                         asrcBufferDataFormat,
                         numChannels,
                         2.0f - ratioAdaptive,
                         mDeviceType,
                         &deviceBufferNumProcessedFrames,
                         &asrcBufferNumProcessedFrames);
          if (startupFinished == false)
          {
            zeroAudioAreaBuffers(asrcBufferAreas, asrcBufferDataFormat, asrcBufferOffset, numChannels, 0, asrcBufferNumProcessedFrames);
          }
        }

        result = asrcBufferHandle->endAccess(asrcBufferAccessType, asrcBufferOffset, asrcBufferNumProcessedFrames);
        IAS_ASSERT(result == eIasRingBuffOk);

        deviceBufferNumFramesTransferred += deviceBufferNumProcessedFrames;
        deviceBufferNumFrames -= deviceBufferNumProcessedFrames;

        // Exit the loop, if the ASRC buffer provided no frames or if the
        // ASRC controller has decided that the number of frames is not enough.
        if ((asrcBufferNumFrames == 0) || (!transferActive))
        {
          break;
        }
      }

      // If the ASRC has not created/consumed enough PCM frames, pad with zeros or skip
      // remaining frames, so that the device buffer gets a complete period.
      if (deviceBufferNumFrames > 0)
      {
        if (mDeviceType == eIasDeviceTypeSink)
        {
          // Pad with zeros, if we write into the device buffer.
          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, LOG_DEVICE,
                      "Padding with", deviceBufferNumFrames, "zero-valued PCM frames.");
          zeroAudioAreaBuffers(deviceBufferAreas, deviceBufferDataFormat,
                               deviceBufferOffset + deviceBufferNumFramesTransferred, numChannels, 0, deviceBufferNumFrames);
        }
        else
        {
          // Skip frames, if we read from the device buffer.
          DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, LOG_DEVICE,
                      "Skipping", deviceBufferNumFrames, "PCM frames.");
        }
        deviceBufferNumFramesTransferred += deviceBufferNumFrames;
      }

      // Call the endAccess method of the ALSA device.
      result = deviceBufferHandle->endAccess(deviceBufferAccessType, deviceBufferOffset, deviceBufferNumFramesTransferred);
      if (result != IasAudioRingBufferResult::eIasRingBuffOk)
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error during IasAudioRingBuffer::endAccess:", toString(result));
        return IasMediaTransportAvb::IasResult::cFailed;
      }

    }
  }
  return IasMediaTransportAvb::IasResult::cOk;
}



/**
 * @brief Private method to transfer PCM frames from asrcBuffer to deviceBuffer or vice versa.
 *
 * Depending on deviceType, this method copies PCM frames
 *
 * @li from from asrcBuffer to deviceBuffer (if deviceType is eIasDeviceTypeSink)
 * @li or from deviceBuffer to asrcBuffer (if deviceType is eIasDeviceTypeSource).
 */
void IasAlsaHandlerWorkerThread::transferFrames(IasAudioArea const       *deviceBufferAreas,
                                                uint32_t                  deviceBufferOffset,
                                                uint32_t                  deviceBufferNumFrames,
                                                IasAudioArea const       *asrcBufferAreas,
                                                uint32_t                  asrcBufferOffset,
                                                uint32_t                  asrcBufferNumFrames,
                                                IasAudioCommonDataFormat  dataFormat,
                                                uint32_t                  numChannels,
                                                float                     ratioAdaptive,
                                                IasDeviceType             deviceType,
                                                uint32_t                 *deviceBufferNumFramesTransferred,
                                                uint32_t                 *asrcBufferNumFramesTransferred)
{
  IAS_ASSERT(deviceBufferAreas != nullptr);
  IAS_ASSERT(asrcBufferAreas   != nullptr);

  // Verify that we do not copy more channels than provided by the source and destination buffers.
  IAS_ASSERT(numChannels <= deviceBufferAreas[0].maxIndex + 1);
  IAS_ASSERT(numChannels <= asrcBufferAreas[0].maxIndex   + 1);

  for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
  {
    IAS_ASSERT(asrcBufferAreas[cntChannels].start   != nullptr);
    IAS_ASSERT(deviceBufferAreas[cntChannels].start != nullptr);
  }

  uint32_t indexNew; // dummy, not really required
  switch (dataFormat)
  {
    case eIasFormatFloat32:
    {
      uint32_t asrcBufferStep   = asrcBufferAreas[0].step   >> 5; // divide by 32
      uint32_t deviceBufferStep = deviceBufferAreas[0].step >> 5;

      if (deviceType == eIasDeviceTypeSink)
      {
        // Sink device: transfer asrcBuffer to deviceBuffer.
        for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
        {
          mSrcInputBuffersFloat32[cntChannels]  = (((float*)asrcBufferAreas[cntChannels].start)
                                                 + (asrcBufferAreas[cntChannels].first / 32) + asrcBufferOffset * asrcBufferStep);
          mSrcOutputBuffersFloat32[cntChannels] = (((float*)deviceBufferAreas[cntChannels].start)
                                                 + (deviceBufferAreas[cntChannels].first / 32) + deviceBufferOffset * deviceBufferStep);
        }
        IasSrcFarrow::IasResult srcResult = mSrc->processPullMode(mSrcOutputBuffersFloat32, // write into deviceBuffers
                                                                  mSrcInputBuffersFloat32,  // read from asrcBuffers
                                                                  deviceBufferStep,
                                                                  asrcBufferStep,
                                                                  deviceBufferNumFramesTransferred,
                                                                  asrcBufferNumFramesTransferred,
                                                                  &indexNew,     // dummy, not really required
                                                                  0,             // not required: write index within asrcBuffer buffer
                                                                  asrcBufferNumFrames,
                                                                  deviceBufferNumFrames,
                                                                  numChannels,
                                                                  ratioAdaptive);
        (void) srcResult;
        IAS_ASSERT(srcResult == IasSrcFarrow::eIasOk);
      }
      else
      {
        // Source device: transfer from deviceBuffer to asrcBuffer.
        for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
        {
          mSrcOutputBuffersFloat32[cntChannels] = (((float*)asrcBufferAreas[cntChannels].start)
                                                 + (asrcBufferAreas[cntChannels].first / 32) + asrcBufferOffset * asrcBufferStep);
          mSrcInputBuffersFloat32[cntChannels]  = (((float*)deviceBufferAreas[cntChannels].start)
                                                 + (deviceBufferAreas[cntChannels].first / 32) + deviceBufferOffset * deviceBufferStep);
        }
        IasSrcFarrow::IasResult srcResult = mSrc->processPushMode(mSrcOutputBuffersFloat32, // write into asrcBuffers
                                                                  mSrcInputBuffersFloat32,  // read from deviceBuffers
                                                                  asrcBufferStep,
                                                                  deviceBufferStep,
                                                                  asrcBufferNumFramesTransferred,
                                                                  deviceBufferNumFramesTransferred,
                                                                  &indexNew,     // dummy, not really required
                                                                  0,             // not required: write index within asrcBuffer buffer
                                                                  asrcBufferNumFrames,
                                                                  deviceBufferNumFrames,
                                                                  numChannels,
                                                                  ratioAdaptive);
        (void) srcResult;
        IAS_ASSERT(srcResult == IasSrcFarrow::eIasOk);
      }
      break;
    }
    case eIasFormatInt32:
    {
      uint32_t asrcBufferStep   = asrcBufferAreas[0].step   >> 5; // divide by 32
      uint32_t deviceBufferStep = deviceBufferAreas[0].step >> 5; // divide by 32

      if (deviceType == eIasDeviceTypeSink)
      {
        // Sink device: transfer asrcBuffer to deviceBuffer.
        for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
        {
          mSrcInputBuffersInt32[cntChannels]  = (((int32_t   *)asrcBufferAreas[cntChannels].start)
                                                 + (asrcBufferAreas[cntChannels].first / 32) + asrcBufferOffset * asrcBufferStep);
          mSrcOutputBuffersInt32[cntChannels] = (((int32_t   *)deviceBufferAreas[cntChannels].start)
                                                 + (deviceBufferAreas[cntChannels].first / 32) + deviceBufferOffset * deviceBufferStep);
        }
        IasSrcFarrow::IasResult srcResult = mSrc->processPullMode(mSrcOutputBuffersInt32, // write into deviceBuffers
                                                                  mSrcInputBuffersInt32,  // read from asrcBuffers
                                                                  deviceBufferStep,
                                                                  asrcBufferStep,
                                                                  deviceBufferNumFramesTransferred,
                                                                  asrcBufferNumFramesTransferred,
                                                                  &indexNew,     // dummy, not really required
                                                                  0,             // not required: write index within asrcBuffer buffer
                                                                  asrcBufferNumFrames,
                                                                  deviceBufferNumFrames,
                                                                  numChannels,
                                                                  ratioAdaptive);
        (void) srcResult;
        IAS_ASSERT(srcResult == IasSrcFarrow::eIasOk);
      }
      else
      {
        // Source device: transfer from deviceBuffer to asrcBuffer.
      for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
        {
          mSrcOutputBuffersInt32[cntChannels] = (((int32_t   *)asrcBufferAreas[cntChannels].start)
                                                 + (asrcBufferAreas[cntChannels].first / 32) + asrcBufferOffset * asrcBufferStep);
          mSrcInputBuffersInt32[cntChannels]  = (((int32_t   *)deviceBufferAreas[cntChannels].start)
                                                 + (deviceBufferAreas[cntChannels].first / 32) + deviceBufferOffset * deviceBufferStep);
        }
        IasSrcFarrow::IasResult srcResult = mSrc->processPushMode(mSrcOutputBuffersInt32, // write into asrcBuffers
                                                                  mSrcInputBuffersInt32,  // read from deviceBuffers
                                                                  asrcBufferStep,
                                                                  deviceBufferStep,
                                                                  asrcBufferNumFramesTransferred,
                                                                  deviceBufferNumFramesTransferred,
                                                                  &indexNew,     // dummy, not really required
                                                                  0,             // not required: write index within asrcBuffer buffer
                                                                  asrcBufferNumFrames,
                                                                  deviceBufferNumFrames,
                                                                  numChannels,
                                                                  ratioAdaptive);
        (void) srcResult;
        IAS_ASSERT(srcResult == IasSrcFarrow::eIasOk);
      }
      break;
    }
    case eIasFormatInt16:
    {
      uint32_t asrcBufferStep   = asrcBufferAreas[0].step   >> 4; // divide by 16;
      uint32_t deviceBufferStep = deviceBufferAreas[0].step >> 4; // divide by 16;

      if (deviceType == eIasDeviceTypeSink)
      {
        // Sink device: transfer asrcBuffer to deviceBuffer.
        for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
        {
          mSrcInputBuffersInt16[cntChannels]  = (((int16_t*)asrcBufferAreas[cntChannels].start)
                                                 + (asrcBufferAreas[cntChannels].first / 16) + asrcBufferOffset * asrcBufferStep);
          mSrcOutputBuffersInt16[cntChannels] = (((int16_t*)deviceBufferAreas[cntChannels].start)
                                                 + (deviceBufferAreas[cntChannels].first / 16) + deviceBufferOffset * deviceBufferStep);
        }
        IasSrcFarrow::IasResult srcResult = mSrc->processPullMode(mSrcOutputBuffersInt16, // write into deviceBuffers
                                                                  mSrcInputBuffersInt16,  // read from asrcBuffers
                                                                  deviceBufferStep,
                                                                  asrcBufferStep,
                                                                  deviceBufferNumFramesTransferred,
                                                                  asrcBufferNumFramesTransferred,
                                                                  &indexNew,     // dummy, not really required
                                                                  0,             // not required: write index within asrcBuffer buffer
                                                                  asrcBufferNumFrames,
                                                                  deviceBufferNumFrames,
                                                                  numChannels,
                                                                  ratioAdaptive);
        (void) srcResult;
        IAS_ASSERT(srcResult == IasSrcFarrow::eIasOk);
      }
      else
      {
        // Source device: transfer from deviceBuffer to asrcBuffer.
        for (uint32_t cntChannels = 0; cntChannels < numChannels; cntChannels++)
        {
          mSrcOutputBuffersInt16[cntChannels] = (((int16_t*)asrcBufferAreas[cntChannels].start)
                                                 + (asrcBufferAreas[cntChannels].first / 16) + asrcBufferOffset * asrcBufferStep);
          mSrcInputBuffersInt16[cntChannels]  = (((int16_t*)deviceBufferAreas[cntChannels].start)
                                                 + (deviceBufferAreas[cntChannels].first / 16) + deviceBufferOffset * deviceBufferStep);
        }
        IasSrcFarrow::IasResult srcResult = mSrc->processPushMode(mSrcOutputBuffersInt16, // write into asrcBuffers
                                                                  mSrcInputBuffersInt16,  // read from deviceBuffers
                                                                  asrcBufferStep,
                                                                  deviceBufferStep,
                                                                  asrcBufferNumFramesTransferred,
                                                                  deviceBufferNumFramesTransferred,
                                                                  &indexNew,     // dummy, not really required
                                                                  0,             // not required: write index within asrcBuffer buffer
                                                                  asrcBufferNumFrames,
                                                                  deviceBufferNumFrames,
                                                                  numChannels,
                                                                  ratioAdaptive);
        (void) srcResult;
        IAS_ASSERT(srcResult == IasSrcFarrow::eIasOk);
      }
      break;
    }
    default:
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, LOG_DEVICE,
                  "Format not supported:", toString(dataFormat));
      break;
    }
  }

  // Verify that we have not transferred more frames than specified.
  IAS_ASSERT( *deviceBufferNumFramesTransferred <= deviceBufferNumFrames);
  IAS_ASSERT( *asrcBufferNumFramesTransferred   <= asrcBufferNumFrames);
}



void IasAlsaHandlerWorkerThread::bufferAdjustFrames(IasAudioRingBuffer*      bufferHandle,
                                                    IasRingBufferAccess      bufferAccessType,
                                                    IasAudioCommonDataFormat bufferDataFormat,
                                                    uint32_t                 numFramesToAdjust,
                                                    uint32_t                 numChannels)
{
  IasAudioRingBufferResult result;
  IasAudio::IasAudioArea*  bufferAreas  = nullptr;
  uint32_t                 bufferOffset = 0;
  uint32_t                 numFramesAvailable = numFramesToAdjust;

  do
  {
    result = bufferHandle->beginAccess(bufferAccessType, &bufferAreas, &bufferOffset, &numFramesAvailable);
    if (result != IasAudioRingBufferResult::eIasRingBuffOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                  "Error during bufferHandle->beginAccess:", toString(result));
    }
    IAS_ASSERT(numFramesAvailable > 0); // avoid that loop is trapped here

    uint32_t numFramesToAdjustNow = std::min(numFramesToAdjust, numFramesAvailable);
    if (bufferAccessType == eIasRingBufferAccessWrite)
    {
      // Generate zeros
      zeroAudioAreaBuffers(bufferAreas, bufferDataFormat, bufferOffset, numChannels, 0, numFramesToAdjustNow);
    }

    result = bufferHandle->endAccess(bufferAccessType, bufferOffset, numFramesToAdjustNow);
    if (result != IasAudioRingBufferResult::eIasRingBuffOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE,
                  "Error during bufferHandle->endAccess:", toString(result));
    }
    numFramesToAdjust -= numFramesToAdjustNow;
  } while (numFramesToAdjust > 0);
}


IasMediaTransportAvb::IasResult IasAlsaHandlerWorkerThread::shutDown()
{
  mThreadIsRunning = false;
  return IasMediaTransportAvb::IasResult::cOk;
}


IasMediaTransportAvb::IasResult IasAlsaHandlerWorkerThread::afterRun()
{
  return IasMediaTransportAvb::IasResult::cOk;
}


void IasAlsaHandlerWorkerThread::reset()
{
  if (mSrc)
  {
    mSrc->reset();
  }
}


/*
 * Function to get a IasAlsaHandlerWorkerThread::IasResult as string.
 */
#define STRING_RETURN_CASE(name) case name: return string(#name); break
#define DEFAULT_STRING(name) default: return string(name)
std::string toString(const IasAlsaHandlerWorkerThread::IasResult& type)
{
  switch(type)
  {
    STRING_RETURN_CASE(IasAlsaHandlerWorkerThread::eIasOk);
    STRING_RETURN_CASE(IasAlsaHandlerWorkerThread::eIasInvalidParam);
    STRING_RETURN_CASE(IasAlsaHandlerWorkerThread::eIasInitFailed);
    STRING_RETURN_CASE(IasAlsaHandlerWorkerThread::eIasNotInitialized);
    STRING_RETURN_CASE(IasAlsaHandlerWorkerThread::eIasFailed);
    DEFAULT_STRING("Invalid IasAlsaHandlerWorkerThread::IasResult => " + std::to_string(type));
  }
}


} //namespace IasMediaTransportAvb
