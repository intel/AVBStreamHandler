/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file   IasAlsaHwDeviceHandler.cpp
 * @date   2018
 * @brief
 */


#include <string.h>
#include <time.h>
#include <alsa/asoundlib.h>
#include "internal/audio/common/IasAudioLogging.hpp"
#include "internal/audio/common/IasAlsaTypeConversion.hpp"
#include "internal/audio/common/audiobuffer/IasAudioRingBuffer.hpp"
#include "internal/audio/common/audiobuffer/IasAudioRingBufferFactory.hpp"
#include "avb_streamhandler/IasAlsaHwDeviceHandler.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/ias_safe.h"


using namespace IasAudio;
namespace IasMediaTransportAvb {


// String with class name used for printing DLT_LOG messages.
static const std::string cClassName = "IasAlsaHwDevice::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"
#define LOG_DEVICE "device=" + mParams->name + ":"

IasAlsaHwDeviceHandler::IasAlsaHwDeviceHandler(DltContext &dltContext, IasAvbStreamDirection direction, uint16_t streamId, IasAudioDeviceParamsPtr params)
  : IasLocalAudioStream(dltContext, direction, eIasAlsaStream, streamId)
  ,mParams(params)
  ,mDeviceType(eIasDeviceTypeUndef)
  ,mRingBuffer(nullptr)
  ,mRingBufferAsrc(nullptr)
  ,mAlsaHandle(nullptr)
  ,mBufferSize(0)
  ,mPeriodSize(0)
  ,mPeriodTime(0)
  ,mSndLogger(nullptr)
  ,mTimeout(-1)
  ,mTimevalUSecLast(0)
  ,mWorkerThread(nullptr)
  ,mOptimalFillLevel(0u)
  ,mDescMode(IasLocalAudioBufferDesc::eIasAudioBufferDescModeOff)
  ,mAlsaDeviceType(IasAlsaDeviceTypes::eIasAlsaHwDevice)
  ,mLastPtpEpoch(0u)
  ,mSampleFreq(0u)
{
  IAS_ASSERT(mParams != nullptr);
  mIsAsynchronous = (mParams->clockType == eIasClockReceivedAsync);
}


/**
 *  Destructor.
 */
IasAlsaHwDeviceHandler::~IasAlsaHwDeviceHandler()
{
  this->stop();

  if (mWorkerThread != nullptr)
  {
    mWorkerThread.reset();
  }

  IasAudioRingBufferFactory* ringBufferFactory = IasAudioRingBufferFactory::getInstance();
  ringBufferFactory->destroyRingBuffer(mRingBuffer);
  mRingBuffer = nullptr;
  cleanup();

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, LOG_DEVICE);
}


IasAlsaHwDeviceHandler::IasResult IasAlsaHwDeviceHandler::getRingBuffer(IasAudio::IasAudioRingBuffer **ringBuffer) const
{
  if (ringBuffer == nullptr)
  {
    return eIasInvalidParam;
  }

  // The interface buffer, which is used by the switch matrix, is either the
  // mirror buffer (if the ALSA handler works synchronously) or the ASRC buffer
  // (if the ALSA handler works asynchronously).
  *ringBuffer = mIsAsynchronous ? mRingBufferAsrc : mRingBuffer;
  if (*ringBuffer == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Invalid nullptr pointer (due to missing initialization?)");
    return eIasNotInitialized;
  }
  return eIasOk;
}


IasAlsaHwDeviceHandler::IasResult IasAlsaHwDeviceHandler::getPeriodSize(uint32_t *periodSize) const
{
  if (periodSize == nullptr)
  {
    return eIasInvalidParam;
  }

  *periodSize = mPeriodSize;
  return eIasOk;
}


IasAlsaHwDeviceHandler::IasResult IasAlsaHwDeviceHandler::initHandler(IasDeviceType deviceType)
{
  IAS_ASSERT(mParams != nullptr);

  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE, "Initialization of ALSA handler.");
  DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE, "Stream parameters are:", mParams->samplerate, "Hz,", mParams->numChannels, "channels,", toString(mParams->dataFormat), ", periodSize:", mParams->periodSize, ", numPeriods:", mParams->numPeriods);

  int err = snd_output_stdio_attach(&mSndLogger, stdout, 0);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_output_stdio_attach:", snd_strerror(err));
    return eIasInitFailed;
  }

  IasAudioRingBufferFactory* ringBufferFactory = IasAudioRingBufferFactory::getInstance();

  // Create the ring buffer that is connected to the ALSA device (mirror buffer).
  string ringBufName = "IasAlsaHandler_" + mParams->name;
  IasAudioCommonResult result = ringBufferFactory->createRingBuffer(&mRingBuffer,
                                                                    0, // period size, not required for type eIasRingBufferLocalMirror
                                                                    mParams->numPeriods,
                                                                    mParams->numChannels,
                                                                    mParams->dataFormat,
                                                                    eIasRingBufferLocalMirror,
                                                                    ringBufName);

  if ((mRingBuffer == nullptr) || (result != IasAudioCommonResult::eIasResultOk))
  {
    /**
     * @log The initialization parameter dataFormat of the IasAudioDeviceParams structure for device with name <NAME> is invalid.
     */
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in IasAudioRingBufferFactory::createRingBuffer:", toString(result));
    return eIasInitFailed;
  }

  if (mIsAsynchronous)
  {
    if (mParams->numPeriodsAsrcBuffer < 1)
    {
      /**
       * @log The initialization parameter numPeriodsAsrcBuffer of the IasAudioDeviceParams structure for device with name <NAME> is invalid.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Invalid parameter numPeriodsAsrcBuffer (must be >= 1):", mParams->numPeriodsAsrcBuffer);
      return eIasInitFailed;
    }

    // Create the ASRC ring buffer (real buffer).
    string ringBufNameAsrc = "IasAlsaHandler_" + mParams->name + "_asrc";
    result = ringBufferFactory->createRingBuffer(&mRingBufferAsrc,
                                                 mParams->periodSize,
                                                 mParams->numPeriodsAsrcBuffer,
                                                 mParams->numChannels,
                                                 mParams->dataFormat,
                                                 eIasRingBufferLocalReal,
                                                 ringBufNameAsrc);
    if ((mRingBufferAsrc == nullptr) || (result != IasAudioCommonResult::eIasResultOk))
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in IasAudioRingBufferFactory::createRingBuffer:", toString(result));
      return eIasInitFailed;
    }

    IasAlsaHandlerWorkerThread::IasAudioBufferParams deviceBufferParams(mRingBuffer, mParams->numChannels,
                                                                        mParams->dataFormat, mParams->periodSize,
                                                                        mParams->numPeriods);
    IasAlsaHandlerWorkerThread::IasAudioBufferParams asrcBufferParams(mRingBufferAsrc, mParams->numChannels,
                                                                      mParams->dataFormat, mParams->periodSize,
                                                                      mParams->numPeriodsAsrcBuffer);

    // Create an object containing the parameters for initializing the worker thread object.
    IasAlsaHandlerWorkerThread::IasAlsaHandlerWorkerThreadParamsPtr workerThreadParams
      = std::make_shared<IasAlsaHandlerWorkerThread::IasAlsaHandlerWorkerThreadParams>(mParams->name,
                                                                                       mParams->samplerate,
                                                                                       deviceBufferParams,
                                                                                       asrcBufferParams);

    // Create and initialize the worker thread object.
    if (mWorkerThread == nullptr)
    {
      mWorkerThread = std::make_shared<IasAlsaHandlerWorkerThread>(workerThreadParams);
      IAS_ASSERT(mWorkerThread != nullptr);
      IasAlsaHandlerWorkerThread::IasResult result = mWorkerThread->init(deviceType, mSampleFreq);
      (void) result;
      IAS_ASSERT(result == IasAlsaHandlerWorkerThread::eIasOk);
    }
  }
  return eIasOk;
}


IasAvbProcessingResult IasAlsaHwDeviceHandler::init(uint16_t numChannels, uint32_t totalLocalBufferSize, uint32_t optimalFillLevel,
                                        uint32_t alsaPeriodSize, uint32_t numAlsaPeriods, uint32_t alsaSampleFrequency,
                                        IasAvbAudioFormat format, uint8_t channelLayout, bool hasSideChannel,
                                        std::string deviceName, IasAlsaDeviceTypes alsaDeviceType)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;
  IasResult iasRes = eIasInitFailed;

  if ( (eIasAlsaVirtualDevice == alsaDeviceType) || (0u == alsaPeriodSize) || (0u == numAlsaPeriods) || (deviceName.empty()))
  {
    // other parameter range checks are done by base class
    ret = eIasAvbProcInvalidParam;
  }
  else if (IasAvbAudioFormat::eIasAvbAudioFormatSaf16 != format)
  {
    ret = eIasAvbProcUnsupportedFormat;
  }
  else
  {

    mSampleFreq = alsaSampleFrequency;
    iasRes = IasAlsaHwDeviceHandler::initHandler(mDeviceType);

    if (eIasOk != iasRes)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "init AlsaHandler failed! reason=", int32_t(iasRes));
      ret = eIasAvbProcErr;
    }
    else
    {
       // init additional  AVB Stuff
       ret = IasLocalAudioStream::init(channelLayout, numChannels, hasSideChannel, totalLocalBufferSize,
                                       alsaSampleFrequency, alsaPeriodSize);
       if (eIasAvbProcOK == ret)
       {
         IasLibPtpDaemon* mPtpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
         mOptimalFillLevel = optimalFillLevel;
         mAlsaDeviceType   = alsaDeviceType;
         mLastPtpEpoch     = mPtpProxy->getEpochCounter();
         IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioTstampBuffer, mDescMode);
       }
       else
       {
         DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "AlsaHandler init failed! reason=", int32_t(ret));
       }
     }
   }

   if (eIasAvbProcOK == ret)
   {
     mPeriodSize = alsaPeriodSize;
   }
   else
   {
     DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ALSA stream init failed, reason=", int32_t(ret));
     cleanup();
   }

   return ret;
}


IasAlsaHwDeviceHandler::IasResult IasAlsaHwDeviceHandler::start()
{
  if (mRingBuffer == nullptr)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error, mRingBuffer is nullptr");
    return eIasNotInitialized;
  }

  snd_pcm_stream_t alsaStreamDirection;
  switch (mDeviceType)
  {
    case eIasDeviceTypeSource:
      alsaStreamDirection = SND_PCM_STREAM_CAPTURE;
      break;
    case eIasDeviceTypeSink:
      alsaStreamDirection = SND_PCM_STREAM_PLAYBACK;
      break;
    default:
      /**
       * @log Error when trying to open the ALSA device <NAME>.
       *      Either the name is incorrect or the device does not exist yet.
       */
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error, stream direction is not defined");
      return eIasInvalidParam;
  }

  int err = snd_pcm_open(&mAlsaHandle, mParams->name.c_str(), alsaStreamDirection, 0);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_open:", snd_strerror(err));
    return eIasInvalidParam;
  }

  int32_t  actualBufferSize = 0;
  int32_t  actualPeriodSize = 0;
  err = set_hwparams(mAlsaHandle,
                     mParams->dataFormat, mParams->samplerate, mParams->numChannels,
                     mParams->numPeriods, mParams->periodSize,
                     &actualBufferSize, &actualPeriodSize);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in private function set_hwparams:", snd_strerror(err));
    return eIasInvalidParam;
  }

  err = set_swparams(mAlsaHandle, actualBufferSize, actualPeriodSize);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in private function set_swparams:", snd_strerror(err));
    return eIasInvalidParam;
  }

  mPeriodSize = static_cast<uint32_t>(actualPeriodSize);    // expressed in PCM frames
  mPeriodTime = static_cast<uint32_t>((static_cast<uint64_t>(mPeriodSize) * 1000000) / mParams->samplerate); // microseconds

  // Timeout after 10 * period size (expressed in milliseconds). This rather 'huge' timeout is required for systems were many real-time scheduled threads are running
  // and the system might not be fine-tuned correctly.
  mTimeout    = (10 * mPeriodTime) / 1000;

  mBufferSize = mParams->numPeriods * mPeriodTime;      // 7 * 4ms = 28ms


  // Assign the ALSA device handle to the ring buffer.
  IasAudioRingBufferResult ringBufRes = mRingBuffer->setDeviceHandle(mAlsaHandle, mPeriodSize, mTimeout);
  if (ringBufRes != eIasRingBuffOk)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in IasAudioRingBuffer::setDeviceHandle:", toString(ringBufRes));
    return eIasRingBufferError;
  }


  if (mIsAsynchronous)
  {
    if (mWorkerThread == nullptr)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error, ALSA handler has not been initialized (mWorkerThread == nullptr)");
      return eIasNotInitialized;
    }

    // reset the ASRC buffer, so we always start with a clean buffer in case we were stopped and started again in the same lifecycle
    mRingBufferAsrc->resetFromReader();

    // Start the worker thread.
    IasAlsaHandlerWorkerThread::IasResult ahwtResult = mWorkerThread->start();
    if (ahwtResult != IasAlsaHandlerWorkerThread::eIasOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error during IasAlsaHandlerWorkerThread::start:", toString(ahwtResult));
      return eIasFailed;
    }
  }
  return eIasOk;
}


void IasAlsaHwDeviceHandler::stop()
{
  // Stop the worker thread, if it exists.
  if (mWorkerThread != nullptr)
  {
    mWorkerThread->stop();
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, LOG_DEVICE, "asynchronous thread successfully stopped");
  }

  if (mAlsaHandle != nullptr)
  {
    snd_pcm_close(mAlsaHandle);
    mAlsaHandle = nullptr;
  }
}


IasAlsaHwDeviceHandler::IasResult IasAlsaHwDeviceHandler::setNonBlockMode(bool isNonBlocking)
{
  if (mRingBuffer == nullptr)
  {
    return IasResult::eIasNotInitialized;
  }

  // Set the ring buffer behavior only if the ALSA handler is synchronous. For asyncrhonous
  // ALSA handlers, we need the blocking behavior, because otherwise the worker thread
  // won't be paused.
  if (!mIsAsynchronous)
  {
    IasAudioRingBufferResult ringBufRes = mRingBuffer->setNonBlockMode(isNonBlocking);
    if (ringBufRes != eIasRingBuffOk)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in IasAudioRingBuffer::setNonBlockMode:", toString(ringBufRes));
      return eIasRingBufferError;
    }
  }
  return eIasOk;
}


/*
 * Private function: Set the hardware parameters of an ASLA PCM device.
 */
int IasAlsaHwDeviceHandler::set_hwparams(snd_pcm_t        *pcmHandle,
                                 IasAudioCommonDataFormat  dataFormat,
                                 unsigned int              rate,
                                 unsigned int              channels,
                                 unsigned int              numPeriods,
                                 unsigned int              period_size,
                                 int32_t                  *actualBufferSize,
                                 int32_t                  *actualPeriodSize)
{
  int err = 0;
  int dir = 0;
  const int cResample = 1; // enable alsa-lib resampling

  snd_pcm_access_t  access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
  snd_pcm_format_t  format = convertFormatIasToAlsa(dataFormat);

  snd_pcm_hw_params_t *hwparams;
  snd_pcm_hw_params_alloca(&hwparams);

  // Init complete hw params to be able to set them.
  err = snd_pcm_hw_params_any(pcmHandle, hwparams);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_any:", snd_strerror(err));
    return err;
  }

  // Set hardware resampling.
  err = snd_pcm_hw_params_set_rate_resample(pcmHandle, hwparams, cResample);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_set_rate_resample:", snd_strerror(err));
    return err;
  }

  // Find out what accestype the ALSA device supports.
  snd_pcm_access_mask_t *accessMask;
  snd_pcm_access_mask_alloca(&accessMask);
  err = snd_pcm_hw_params_get_access_mask(hwparams, accessMask);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_access_mask:", snd_strerror(err));
    return err;
  }

  // Check whether the ALSA device supports the requested access type.
  if (!(snd_pcm_access_mask_test(accessMask, access)))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_access_mask_test: requested access type is not supported");
    return -EINVAL;
  }

  // Set the access type.
  err = snd_pcm_hw_params_set_access(pcmHandle, hwparams, access);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_set_access:", snd_strerror(err));
    return err;
  }

  // Find out what sample format the ALSA device supports.
  snd_pcm_format_mask_t *formatMask;
  snd_pcm_format_mask_alloca(&formatMask);

  // Check whether the ALSA device supports the requested sample format.
  snd_pcm_hw_params_get_format_mask(hwparams, formatMask);

  // Check whether format is UNKNOWN, since snd_pcm_format_mask_test would crash in this case.
  if (format == SND_PCM_FORMAT_UNKNOWN)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error, unknown format: SND_PCM_FORMAT_UNKNOWN");
    return -EINVAL;
  }

  if (!(snd_pcm_format_mask_test(formatMask, format)))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_format_mask_test: requested format is not supported");
    return -EINVAL;
  }

  // Set the sample format.
  err = snd_pcm_hw_params_set_format(pcmHandle, hwparams, format);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_set_format:", snd_strerror(err));
    return err;
  }

  // Check which sample rates are supported by the ALSA device.
  uint32_t rate_min, rate_max;
  uint32_t adapted_samplerate;
  err = snd_pcm_hw_params_get_rate_min(hwparams, &rate_min, &dir);
  if (err != 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_rate_min:", snd_strerror(err));
    return err;
  }
  err = snd_pcm_hw_params_get_rate_max(hwparams, &rate_max, &dir);
  if (err != 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_rate_max:", snd_strerror(err));
    return err;
  }

  if ((rate < rate_min) || (rate > rate_max))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error: requested rate of",
                rate, "Hz is outside the range supported by the ALSA device [", rate_min, "Hz to", rate_max, "Hz ]");
    return -EINVAL;
  }

  // Set sample rate. We want the exact sample rate so set dir to zero
  dir = 0;
  err = snd_pcm_hw_params_set_rate(pcmHandle, hwparams, rate, dir);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_set_rate:", snd_strerror(err));
    return err;
  }

  // Get the actual set samplerate.
  err = snd_pcm_hw_params_get_rate(hwparams, &adapted_samplerate, &dir);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_rate:", snd_strerror(err));
    return err;
  }

  if (rate != adapted_samplerate)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, LOG_DEVICE, "The ALSA device does not support the desired sample rate of",
                rate, "Hz. Using", adapted_samplerate, "Hz instead.");
  }

  // Set the number of channels.
  err = snd_pcm_hw_params_set_channels(pcmHandle, hwparams, channels);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "ALSA device does not support the requested number of", channels, "channels:", snd_strerror(err));
    return err;
  }

  uint32_t valmax, valmin;
  err = snd_pcm_hw_params_get_periods_max(hwparams, &valmax, &dir);
  if (err != 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_periods_max:", snd_strerror(err));
    return err;
  }
  err = snd_pcm_hw_params_get_periods_min(hwparams, &valmin, &dir);
  if (err != 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_periods_min:", snd_strerror(err));
    return err;
  }

  if (numPeriods < valmin)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Requested periods of", numPeriods, "is less than minimum of driver", valmin);
    return -EINVAL;
  }
  if (numPeriods > valmax)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Requested periods of", numPeriods, "is more than maximum of driver", valmax);
    return -EINVAL;
  }

  err = snd_pcm_hw_params_set_periods(pcmHandle, hwparams, numPeriods, 0);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_set_periods:", snd_strerror(err));
    return err;
  }

  uint32_t calculated_buffersize = period_size * numPeriods;
  err = snd_pcm_hw_params_set_buffer_size(pcmHandle, hwparams, calculated_buffersize);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_set_buffer_size:", snd_strerror(err));
    return err;
  }

  snd_pcm_uframes_t size;
  err = snd_pcm_hw_params_get_buffer_size(hwparams, &size);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_buffer_size:", snd_strerror(err));
    return err;
  }
  *actualBufferSize = static_cast<int32_t>(size);

  err = snd_pcm_hw_params_get_period_size(hwparams, &size, &dir);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params_get_period_size:", snd_strerror(err));
    return err;
  }
  *actualPeriodSize = static_cast<int32_t>(size);

  // Set the hw params for the device
  err = snd_pcm_hw_params(pcmHandle, hwparams);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_hw_params:", snd_strerror(err));
    snd_pcm_hw_params_dump(hwparams, mSndLogger);
    return err;
  }

  return 0;
}


/*
 * Private function: Set the software parameters of an ASLA PCM device.
 */
int IasAlsaHwDeviceHandler::set_swparams(snd_pcm_t   *pcmHandle,
                                         int32_t      bufferSize,
                                         int32_t      periodSize)
{
  int err;
  snd_pcm_sw_params_t *swparams;
  snd_pcm_sw_params_alloca(&swparams);

  // Get the current swparams.
  err = snd_pcm_sw_params_current(pcmHandle, swparams);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_sw_params_current:", snd_strerror(err));
    return err;
  }
  // Start the transfer when the buffer is almost full: (bufferSize / avail_min) * avail_min
  err = snd_pcm_sw_params_set_start_threshold(pcmHandle, swparams, static_cast<snd_pcm_sframes_t>((bufferSize / periodSize) * periodSize));
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_sw_params_set_start_threshold:", snd_strerror(err));
    return err;
  }
  // Allow the transfer when at least periodSize samples can be processed.
  err = snd_pcm_sw_params_set_avail_min(pcmHandle, swparams, static_cast<snd_pcm_sframes_t>(periodSize));
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_sw_params_set_avail_min:", snd_strerror(err));
    return err;
  }
  // Write the parameters to the playback device.
  err = snd_pcm_sw_params(pcmHandle, swparams);
  if (err < 0)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, LOG_DEVICE, "Error in snd_pcm_sw_params:", snd_strerror(err));
    return err;
  }
  return 0;
}


IasAvbProcessingResult IasAlsaHwDeviceHandler::setDeviceType(IasDeviceType deviceType)
{
  IasAvbProcessingResult retVal = eIasAvbProcInvalidParam;
  if ((deviceType == eIasDeviceTypeSource) || (deviceType == eIasDeviceTypeSink))
  {
    mDeviceType = deviceType;
    retVal = eIasAvbProcOK;
  }
  else
  {
    mDeviceType = eIasDeviceTypeUndef;
  }

  return retVal;
}


void IasAlsaHwDeviceHandler::reset()
{
  if (mWorkerThread)
  {
    mWorkerThread->reset();
  }
}


/* add functions */
/*
 *  Reset method.
 */
IasAvbProcessingResult IasAlsaHwDeviceHandler::resetBuffers()
{
  IasAvbProcessingResult error = eIasAvbProcOK;

  DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

  IasLocalAudioBufferDesc *descQ = NULL;
  if (true == hasBufferDesc())
  {
    descQ = getBufferDescQ();
    AVB_ASSERT(NULL != descQ);
    descQ->lock();
  }

  for(int channelIdx=0; channelIdx< mNumChannels; channelIdx++)
  {
    IasLocalAudioBuffer *buffer = IasLocalAudioStream::getChannelBuffers()[channelIdx];
    AVB_ASSERT(NULL != buffer);

    if (true == hasBufferDesc())
    {
      IasLocalAudioBuffer::AudioData * dummyData = NULL;

      uint32_t fillLevel = buffer->getFillLevel();
      if (mOptimalFillLevel == fillLevel)
      {
        continue;
      }
      else if (mOptimalFillLevel < fillLevel)
      {
        // discard samples
        uint32_t bufferSize  = fillLevel - mOptimalFillLevel;
        uint16_t samplesRead = 0;

        dummyData = new (nothrow) IasLocalAudioBuffer::AudioData[bufferSize];
        AVB_ASSERT(dummyData != NULL);

        /*
         * dumpFromLocalAudioBuffer() should not be used to discard samples here
         * because we need to discard corresponding descriptors as well.
         */
        uint64_t timeStamp = 0u;
        (void) readLocalAudioBuffer(uint16_t(channelIdx), dummyData, bufferSize, samplesRead, timeStamp);
      }
      else // fillLevel < mOptimalFillLevel
      {
        /*
         * The time-aware buffer accumulates samples up to half-full before allowing initial read access.
         * The resetBuffers() is typically called when the local stream becomes active. If we reset the buffer
         * with mOptimalFillLevel, since it is typically half-full of the total buffer size, it would immediately
         * become readable. To avoid the unexpected buffer filling we do not fill the buffer here.
         *
         * Note: if the buffer is underrun the AVB tx stream will make a dummy packet. And the ALSA interface
         * freewheels. We don't need to fill the buffer with dummy samples here.
         */
      }

      if (NULL != dummyData)
      {
        delete[] dummyData;
        dummyData = NULL;
      }
    }
    else
    {
      buffer->reset(mOptimalFillLevel);
    }
  }

  if (true == hasBufferDesc())
  {
    descQ->unlock();
  }

  return error;
}


void IasAlsaHwDeviceHandler::updateBufferStatus()
{
  IasLocalAudioStreamClientInterface * const client = getClient();
  if (NULL != client)
  {
    AVB_ASSERT(mChannelBuffers.size() > 0u);
    IasLocalAudioBuffer * buf = mChannelBuffers[0];
    AVB_ASSERT(NULL != buf);

    int32_t fill = buf->getRelativeFillLevel();

    client->updateRelativeFillLevel(fill);

    if (eIasActive == getClientState())
    {
      IasLocalAudioStreamClientInterface::DiscontinuityEvent event = IasLocalAudioStreamClientInterface::eIasUnspecific;

      if (buf->getFillLevel() == buf->getTotalSize())
      {
        event = IasLocalAudioStreamClientInterface::eIasOverrun;
      }
      else if (0 == buf->getFillLevel())
      {
        event = IasLocalAudioStreamClientInterface::eIasUnderrun;
      }
      else
      {
        // do nothing
      }

      if ((IasLocalAudioStreamClientInterface::eIasUnspecific != event) && client->signalDiscontinuity(event, 0u))
      {
        resetBuffers();
      }
    }
  }
}


/**
 *  Cleanup method.
 */
void IasAlsaHwDeviceHandler::cleanup()
{
   DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

   mSampleFrequency  = 0u;
   mPeriodSize       = 0u;
   mOptimalFillLevel = 0u;
}



IasAvbProcessingResult IasAlsaHwDeviceHandler::writeLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer,
                                                                     uint32_t bufferSize, uint16_t &samplesWritten, uint32_t timestamp)
{
  return IasLocalAudioStream::writeLocalAudioBuffer(channelIdx, buffer, bufferSize, samplesWritten, timestamp);
}


void IasAlsaHwDeviceHandler::copyJob(uint64_t timestamp)
{
  IasAudio::IasAudioArea *shmAreas; // Pointer to the source areas already created by the SHM
  const IasLocalAudioStream::LocalAudioBufferVec & buffers = getChannelBuffers();
  IasLocalAudioBufferDesc*                           descQ = getBufferDescQ();
  IasLocalAudioBuffer::AudioData                *mNullData = nullptr;
  IasAudioRingBufferResult                          bufRes = IasAudioRingBufferResult::eIasRingBuffOk;
  IasAvbProcessingResult                            result = eIasAvbProcOK;
  uint32_t                                       numFrames = mPeriodSize;
  uint32_t                                       shmFrames = numFrames;
  uint32_t                                     numChannels = mParams->numChannels;
  uint32_t                                       shmOffset = 0u;
  uint64_t                                             now = 0u;
  bool                                            isLocked = false;
  bool                                             useDesc = hasBufferDesc();
  bool                                      resetRequested = false;

  AVB_ASSERT(0 == mCycle);  // see old call
  (void) result;

  if ((0u == numFrames) || (mParams->periodSize % numFrames != 0u))
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "bad number of frames:", numFrames);
    result = eIasAvbProcErr;
  }
  else
  {
    // Determine transmission direction
    bool mDirWriteToShm = getDirection() == IasAvbStreamDirection::eIasAvbTransmitToNetwork ? false : true;
    IasAudio::IasRingBufferAccess accessDirection =
    mDirWriteToShm ? IasAudio::eIasRingBufferAccessWrite : IasAudio::eIasRingBufferAccessRead;

    IasAudio::IasAudioRingBuffer *shmBuffer = nullptr;
    getRingBuffer(&shmBuffer);

    uint32_t readAvailable = 0u;
    // updateAvailable necessary for ALSA HW Device
    bufRes = shmBuffer->updateAvailable(IasAudio::eIasRingBufferAccessRead, &readAvailable);
    AVB_ASSERT(IasAudioRingBufferResult::eIasRingBuffOk == bufRes);

    bufRes = shmBuffer->beginAccess(accessDirection, &shmAreas, &shmOffset, &shmFrames);

    if (IasAudioRingBufferResult::eIasRingBuffOk != bufRes)
    // shmFrames returned here is the overall numbers of frames available for read or write, but not more than requested
    {
       // this could happen when multiple threads concurrently access the ring buffer in the same direction
       DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "error in beginAccess of ring buffer error Code: " , uint8_t(bufRes));
       result = eIasAvbProcErr;
    }
    else
    {
       isLocked = true;

       DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "beginAccess() succeeded, shmFrames=", shmFrames,
                   " periodSize=", mParams->periodSize, " prefill=", mParams->numPeriods/2);
    }

    IasLibPtpDaemon* mPtpProxy = IasAvbStreamHandlerEnvironment::getPtpProxy();
    uint64_t maxPtGap   = 0u; // max presentation time gap
    uint64_t readIndex  = 0u;
    uint64_t writeIndex = 0u;
    bool isReadReady    = true;
    bool toBePresented  = false;
    IasLocalAudioBufferDesc::AudioBufferDesc desc;

    bool   alsaRxSyncStart = false;
    uint64_t skippedTime   = 0u;
    (void) skippedTime;

    if (true == useDesc)
    {
      AVB_ASSERT(NULL != descQ);
      AVB_ASSERT(NULL != buffers[0]);
      // prevent AvbTxWrk & AvbRxWrk from accessing the local audio stream
      descQ->lock();

      alsaRxSyncStart = descQ->getAlsaRxSyncStartMode();

      if (nullptr != mPtpProxy)
      {
        now = mPtpProxy->getLocalTime();
      }
      else
      {
        // fatal error, output an error message but no way to recover
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "ptp proxy == NULL!");
      }

      uint32_t epoch = (nullptr != mPtpProxy) ? mPtpProxy->getEpochCounter() : 0u;
      if (epoch != mLastPtpEpoch)
      {
        mLastPtpEpoch = epoch;
        /*
         * ptp negative timejump: Discard old samples derived from former ptp timebase.
         * Old samples would be seen placed in future because those samples derived from
         * ptp time before the negative time jump happened. Unless resetting the buffer,
         * newly produced samples based on current ptp timebase might be delayed for
         * unpredictable time due to old samples.
         */
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "ptp time warp detected - resetting local audio buffer");
        resetRequested = true;
      }

      if (IasAudio::eIasRingBufferAccessWrite == accessDirection) // receive stream
      {
        /*
         * wait until buffer fill level reaches at least half-full in time-aware buffering mode
         */
        isReadReady = buffers[0]->isReadReady();

        if (true == isReadReady)
        {
          uint32_t totalSz = buffers[0]->getTotalSize() - 1u;
          maxPtGap = uint32_t(double(totalSz) / double(mParams->samplerate) * 1e9);

          if (eIasAvbProcOK == descQ->peek(desc))
          {
            readIndex  = buffers[0]->getMonotonicReadIndex();
            writeIndex = buffers[0]->getMonotonicWriteIndex();
            uint32_t readIndexPerSampleRate = uint32_t(readIndex % mParams->samplerate);

            // detect a far-away timestamp
            uint64_t timeGap = (now > desc.timeStamp) ? (now - desc.timeStamp) : (desc.timeStamp - now);
            if ((maxPtGap < timeGap) && (IasLocalAudioBufferDesc::eIasAudioBufferDescModeHard == mDescMode))
            {
              DLT_LOG(*mLog, DLT_LOG_WARN, DLT_STRING("[IasAvbAudioShmProvider::copyJob]"),
                  DLT_STRING("detected out-of-bound presentation timestamp"), DLT_STRING("timestamp="),
                  DLT_UINT64(desc.timeStamp), DLT_STRING("now="), DLT_UINT64(now));
              resetRequested = true;
            }
            else
            {
              if (desc.timeStamp <= now)
              {
                toBePresented = true; // to be presented now
              }
              else
              {
                // we have samples but the time is not ripe yet
                if (writeIndex < mParams->samplerate) // keep checking until the AVB layer writes samples of 1 sec
                {
                  // strictly check timestamp at the startup time regardless of the fail-safe mode
                  toBePresented = false;
                }
                else
                {
                  const uint32_t fillLevel = buffers[0]->getFillLevel();
                  const uint32_t threshold = buffers[0]->getReadThreshold();

                  if (threshold <= fillLevel)
                  {
                    // now we are at the risk of buffer overrun if we don't allow ALSA grab samples

                    if (IasLocalAudioBufferDesc::eIasAudioBufferDescModeFailSafe == mDescMode)
                    {
                      // allow presenting samples but give it warn
                      toBePresented = true;
                    }

                    if ((0u == descQ->getDbgPresentationWarningTime()) ||
                        ((now - descQ->getDbgPresentationWarningTime()) > uint64_t(1e9))) // output once per second
                    {
                      DltLogLevelType logLevel = DLT_LOG_INFO;
                      uint64_t samplesToPresentOnAhead = (desc.timeStamp - now) / mParams->samplerate;
                      if (numFrames < samplesToPresentOnAhead)
                      {
                        // warn if we're presenting samples which are not supposed to be presented in the current period
                        logLevel = DLT_LOG_WARN;
                      }

                      DLT_LOG_CXX(*mLog, logLevel, LOG_PREFIX, toBePresented ? "force" : "waive", "audio presentation",
                          ": now / pt / fillLevel / time-aware-mode =",
                          now, "/", desc.timeStamp, "/", fillLevel, "/", descQ->getAudioBufferDescModeString(mDescMode));

                      descQ->setDbgPresentationWarningTime(now);
                    }
                  }
                }
              }
            }
            // debug code
            if (readIndexPerSampleRate < mParams->periodSize)
            {
              // output the debug message once per second
              DLT_LOG_CXX(*mLog, DLT_LOG_DEBUG, LOG_PREFIX, "now =", now, "pt =", desc.timeStamp, "lag =", int64_t(now - desc.timeStamp), "ns");
            }
          } // descQ->peek()
        }   // isReadReady
      }     // receive stream
      descQ->unlock();
    } // time-aware mode

    for (uint16_t channel = 0; channel < numChannels; channel++)
    {
      uint32_t gap            = 0u;
      uint32_t nrSamples      = 0u;
      uint32_t samplesRead    = 0u;
      uint32_t samplesWritten = 0u;
      uint32_t step           = 0u; // Step between samples belonging to the same channel in bytes
      char*    shmData        = nullptr;

      IasLocalAudioBuffer *buffer = buffers[channel];
      AVB_ASSERT(NULL != buffer);

      IasAudio::IasAudioArea* area = &(shmAreas[channel]);
      AVB_ASSERT(nullptr != area);

      // Calculate step and pointer to the shared memory location
      step    = area->step / 8;            // stride to next sample
      shmData = static_cast<char*>(area->start) + area->first / 8 + shmOffset * step;

      if (IasAudio::eIasRingBufferAccessRead == accessDirection)
      {
        if (0u == timestamp)
        {
          /*
           * reference clock is not available yet, let the ALSA interface freewheel
           *
           * Note: if we write samples and descriptors while reference clock is not
           * available, we need to set launch time with estimated timestamp. Once the
           * clock becomes available the tx sequencer found this old timestamp is
           * invalid and resets the stream. To avoid unneeded reseting we should let
           * the ALSA freewheel if clock reference is not available.
           */
          continue;
        }

        // write data from shm ringbuffer to AVB buffer
        samplesWritten = buffer->write(reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData), shmFrames, step);

        if (true == useDesc)
        {
          descQ->lock();

          if ((0u != samplesWritten) && (0 == channel))
          {
            // store timestamp to FIFO
            desc.timeStamp = timestamp;
            desc.bufIndex  = buffer->getMonotonicWriteIndex() - samplesWritten;
            desc.sampleCnt = samplesWritten;
            descQ->enqueue(desc);
          }

          descQ->unlock();

          if ((shmFrames != samplesWritten) ) // && (0 == channel))
          {
            DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX, "tx buffer overrun", "written =", samplesWritten, "expected =",
                        shmFrames);
          }
        }
      }
      else  // (IasAudio::eIasRingBufferAccessWrite == accessDirection)
      {
        if (true == useDesc)
        {
          descQ->lock();

         // read a descriptor w/o dequeuing
         if (eIasAvbProcOK == descQ->peek(desc))
         {
           readIndex = buffer->getMonotonicReadIndex();
           if (readIndex < desc.bufIndex)
           {
             /*
              * lost a descriptor due to overrun or reset
              */
             if (0u == channel) // output message once per stream
             {
               DLT_LOG(*mLog, DLT_LOG_ERROR, DLT_STRING("[IasAvbAudioShmProvider::copyJob]"),
                   DLT_STRING("detected unexpected timestamp"), DLT_STRING("bufIndex="), DLT_UINT64(desc.bufIndex),
                   DLT_STRING("sampleCnt="), DLT_UINT64(desc.sampleCnt), DLT_STRING("readIndex="), DLT_UINT64(readIndex));
             }

             // discard samples which lost mapping to the descriptor
             gap = uint32_t(desc.bufIndex - readIndex);
             while (0u != gap)
             {
               // need to discard samples of 'gap' size but mNullData has only buffer of mParams->periodSize
               nrSamples = std::min(gap, mParams->periodSize);
               samplesRead = buffer->read(mNullData, nrSamples);
               if (0u == samplesRead)
               {
                 /*
                  * this will not happen but just in case infinite loop happened
                  * due to unanticipated problem
                  */
                 break;
               }
               readIndex = buffer->getMonotonicReadIndex();
               gap = uint32_t(desc.bufIndex - readIndex);
             }
           }

            if (desc.timeStamp <= now)
            {
              // to be presented now
              buffer->read(reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData), shmFrames, step);

              if ((desc.bufIndex + desc.sampleCnt) <= buffer->getMonotonicReadIndex())
              {
                // delete the descriptor because all of its samples were read
                descQ->dequeue(desc);
              }
            }
            else
            {
              if (buffer->getMonotonicReadIndex() < desc.bufIndex)
              {
                // lost the previous descriptor due to buffer overrun? How to deal with the case?
                buffer->read(reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData), shmFrames, step);
              }
            }

           if (true == toBePresented)
           {
             // remaining samples which are not yet read
             nrSamples = desc.sampleCnt - uint32_t(readIndex - desc.bufIndex);
             if (alsaRxSyncStart)
             {
               nrSamples = buffer->getFillLevel();
             }
             nrSamples = std::min(shmFrames, nrSamples);

             buffer->read(reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData), nrSamples, step);

             // ABu: ToDo: This part is not tested yet
#if defined(PERFORMANCE_MEASUREMENT)
             if (IasAvbStreamHandlerEnvironment::isAudioFlowLogEnabled()) // latency analysis
             {
               uint32_t state = 0u;
               uint64_t logtime = 0u;
               (void) IasAvbStreamHandlerEnvironment::getAudioFlowLoggingState(state, logtime);

               if ((1u == state) && (0u == channel))
               {
                 if (0 != mNullData[0]) // quick check to see if it is really a null data buffer
                 {
                   (void) std::memset(mNullData, 0, mParams->periodSize * sizeof(AudioData));
                 }

                 const uint32_t bufSize = static_cast<uint32_t>(shmFrames * sizeof(AudioData));

                 if ((0 != reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData)[0]) || // quick check to avoid memcmp as much as possible
                     (0 != std::memcmp(shmData, mNullData, bufSize)))
                 {
                   uint64_t tscNow = mPtpProxy->getTsc();
                   DLT_LOG_CXX(*mLog, DLT_LOG_WARN, LOG_PREFIX,
                               "latency-analysis(2): passed samples to Reference Plane (ALSA) system time =", tscNow,
                               "delta =", (double)(tscNow - logtime) / 1e6, "ms", "fill-level =", buffer->getFillLevel(),
                               "pts-delta =", int64_t(now - (desc.timeStamp + skippedTime)));
                   IasAvbStreamHandlerEnvironment::setAudioFlowLoggingState(2u, tscNow);
                 }
               }
             }
#endif

             readIndex = buffer->getMonotonicReadIndex();
             // delete used descriptors when the samples of the last channel were grabbed
             if (channel == (numChannels - 1))
             {
               do
               {
                 if ((desc.bufIndex + desc.sampleCnt) <= readIndex)
                 {
                   // delete the descriptor because all of its samples were read
                   descQ->dequeue(desc);
                 }
                 else
                 {
                   // nop: the descriptor must remain in fifo since it still has remaining samples
                   break;
                 }
                 // read the next descriptor
               } while (eIasAvbProcOK == descQ->peek(desc));
             }
           }
         }
         else
         {
           // underrun: fill the shared memory with silence
           if (sizeof(IasLocalAudioBuffer::AudioData) == step) //not interleaved
           {
             size_t size = shmFrames * sizeof(IasLocalAudioBuffer::AudioData);
             avb_safe_result copyResult = avb_safe_memcpy(shmData, size, mNullData, size);
             (void) copyResult;
           }
           else  // interleaved
           {
             IasLocalAudioBuffer::AudioData * audioBuffer = reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData);
             for (uint32_t sample = 0u; sample < shmFrames; sample++)
             {
               *audioBuffer = 0u;
               audioBuffer += step/sizeof(IasLocalAudioBuffer::AudioData);
             }
           }
         }
         descQ->unlock();
        }
        else /* !useDesc */
        {
         buffer->read(reinterpret_cast<IasLocalAudioBuffer::AudioData*>(shmData), shmFrames, step);
        }
      }    // (IasAudio::eIasRingBufferAccessWrite == accessDirection)
    }      // end for each channel

    if (true == useDesc)
    {
      descQ->lock();

      if (true == resetRequested)
      {
        // completely reset buffer and fifo
        for (uint32_t channel = 0; channel < numChannels; channel++)
        {
          IasLocalAudioBuffer *ringBuf = buffers[channel];
          AVB_ASSERT(NULL != ringBuf);
          ringBuf->reset(0u);
        }
        descQ->reset();
      }

      descQ->unlock();
    }

    if (isLocked)
    {
      /*
       * call encAccess() when beginAccess() was successful
       *
       * beginAccess() might return eIasRingBuffNotAllowed while the ring buffer is updated by the IPC thread.
       * We must not call encAccess() in that case otherwise it will corrupt the state of the internal mutex.
       */
      if (shmBuffer->endAccess(accessDirection, shmOffset, shmFrames))
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, LOG_PREFIX, "error in endAccess of sink ring buffer");
        result = eIasAvbProcErr;
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX, "endAccess() succeeded, shmFrames =", shmFrames);
      }
    }
  }       // frame check
}         // copyJob()


/*
 * Function to get a IasAlsaHandler::IasResult as string.
 */
#define STRING_RETURN_CASE(name) case name: return string(#name); break
#define DEFAULT_STRING(name) default: return string(name)
std::string toString(const IasAlsaHwDeviceHandler::IasResult& type)
{
  switch(type)
  {
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasOk);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasInvalidParam);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasInitFailed);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasNotInitialized);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasAlsaError);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasTimeOut);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasRingBufferError);
    STRING_RETURN_CASE(IasAlsaHwDeviceHandler::eIasFailed);
    DEFAULT_STRING("Invalid IasAlsaHandler::IasResult => " + std::to_string(type));
  }
}


} // namespace IasMediaTransportAvb
