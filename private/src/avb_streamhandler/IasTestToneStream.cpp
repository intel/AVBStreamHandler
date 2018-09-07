/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasTestToneStream.cpp
 * @brief   Implementation of the local stream class for test tones
 * @details
 * @date    2013
 */

#include "avb_streamhandler/IasTestToneStream.hpp"

#include <cmath>
#include <dlt/dlt_cpp_extension.hpp>

#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#ifdef __SSE__
#include <xmmintrin.h>
#include <emmintrin.h>
#endif // __SSE__

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasTestToneStream::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

/*
 *  Constructor.
 */
IasTestToneStream::IasTestToneStream(DltContext &dltContext, uint16_t streamId)
  : IasLocalAudioStream(dltContext, IasAvbStreamDirection::eIasAvbTransmitToNetwork, eIasTestToneStream, streamId)
  , mConversionGain(AudioData(0x7FFF))
  , mUseSaturation(true)
{
  // nothing to do
}

IasTestToneStream::GeneratorParams::GeneratorParams()
  : mode(IasAvbTestToneMode::eIasAvbTestToneSine)
  , userParam(0)
  , signalFreq(48000u)
  , level(0)
  , peak(1.0f)
  , coeff(1.0f)
  , buf1(1.0f)
  , buf2(0.0f)
{
  // nothing to do
}


/*
 *  Destructor.
 */
IasTestToneStream::~IasTestToneStream()
{
  cleanup();
}

/*
 *  Initialization method.
 */
IasAvbProcessingResult IasTestToneStream::init(uint16_t numChannels, uint32_t sampleFrequency, uint8_t  channelLayout)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;

  if (0u == numChannels)
  {
    // further arg checking done in IasLocalAudioStream::init
    ret = eIasAvbProcInvalidParam;
  }
  else
  {
    ret = IasLocalAudioStream::init(channelLayout, numChannels, false, 0u, sampleFrequency);
  }

  if (eIasAvbProcOK == ret)
  {
    /* reserve space for one set of channel data per channel,
     * init default setting and push it to the vector once for each channel
     */
    mChannels.reserve(numChannels);
    ChannelData c;
    calcCoeff(mSampleFrequency, c.params);
    setProcessMethod(c);

    for (int32_t channel = 0u; channel < numChannels; channel++)
    {
      mChannels.push_back(c);
    }

    uint32_t val = 0x7FFFu; // format specific
    if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioFloatGain, val))
    {
      mConversionGain = AudioData(int32_t(val));
    }
    if (IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cAudioSaturate, val))
    {
      mUseSaturation = (val != 0u);
    }
  }

  return ret;
}


/*
 *  Cleanup method.
 */
void IasTestToneStream::cleanup()
{
   DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
   mSampleFrequency = 0u;
}

void IasTestToneStream::calcCoeff(uint32_t sampleFreq, GeneratorParams & params)
{
  AVB_ASSERT(0u != sampleFreq);
  params.peak = powf(10.0f, float(params.level) / 20.0f);

  switch (params.mode)
  {
  case IasAvbTestToneMode::eIasAvbTestToneSine:
    {
      const float omega = static_cast<float>(2.0f * M_PI * AudioData(params.signalFreq) / AudioData(sampleFreq));
      params.coeff = 2.0f * cosf(omega);
      params.buf1 = params.peak * sinf(omega);
      params.buf2 = 0.0f;
    }
    break;
  case IasAvbTestToneMode::eIasAvbTestTonePulse:
    {
      params.coeff = AudioData(params.signalFreq) / AudioData(sampleFreq);
      params.buf1 = 0.0f;
      params.buf2 = 0.01f * float(params.userParam);
    }
    break;
  case IasAvbTestToneMode::eIasAvbTestToneSawtooth:
    {
      params.buf2 = 2.0f * params.peak;
      params.coeff = params.buf2 * AudioData(params.signalFreq) / AudioData(sampleFreq);
      params.buf1 = 0.0f;
    }
    break;
  default:
    AVB_ASSERT(false);
    break;
  }
}

void IasTestToneStream::setProcessMethod(ChannelData & ch)
{
  switch (ch.params.mode)
  {
  case IasAvbTestToneMode::eIasAvbTestToneSine:
    ch.method = &IasMediaTransportAvb::IasTestToneStream::generateSineWave;
    break;
  case IasAvbTestToneMode::eIasAvbTestTonePulse:
    ch.method = &IasMediaTransportAvb::IasTestToneStream::generatePulseWave;
    break;
  case IasAvbTestToneMode::eIasAvbTestToneSawtooth:
    if (ch.params.userParam >= 0)
    {
      ch.method = &IasMediaTransportAvb::IasTestToneStream::generateSawtoothWaveRising;
    }
    else
    {
      ch.method = &IasMediaTransportAvb::IasTestToneStream::generateSawtoothWaveFalling;
    }
    break;
  default:
    AVB_ASSERT(false);
    break;
  }
}


IasAvbProcessingResult IasTestToneStream::writeLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer, uint32_t bufferSize, uint16_t &samplesWritten, uint32_t timeStamp)
{
  (void) channelIdx;
  (void) buffer;
  (void) bufferSize;
  (void) samplesWritten;
  (void) timeStamp;

  return eIasAvbProcNotImplemented;
}

IasAvbProcessingResult IasTestToneStream::readLocalAudioBuffer(uint16_t channelIdx, IasLocalAudioBuffer::AudioData *buffer, uint32_t bufferSize, uint16_t &samplesRead, uint64_t &timeStamp)
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  timeStamp = 0u;

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else if ((channelIdx >= mNumChannels) || (NULL == buffer) || (0u == bufferSize))
  {
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    AVB_ASSERT(mChannels.size() == mNumChannels);

    samplesRead = uint16_t( (this->*(mChannels[channelIdx].method))(buffer, bufferSize, mChannels[channelIdx].params) );
  }

  return result;
}

uint32_t IasTestToneStream::generateSineWave(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params)
{
  for (uint32_t sample = 0u; sample < numSamples; sample++)
  {
    AudioData tempData = params.coeff * params.buf1 - params.buf2;
    if (mUseSaturation)
    {
      buf[sample] = convertFloatToPcm16(tempData);
    }
    else
    {
      buf[sample] = IasLocalAudioBuffer::AudioData(tempData * mConversionGain);
    }
    params.buf2 = params.buf1;
    params.buf1 = tempData;
  }

  return numSamples;
}

uint32_t IasTestToneStream::generatePulseWave(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params)
{
  for (uint32_t sample = 0u; sample < numSamples; sample++)
  {
    if (mUseSaturation)
    {
      buf[sample] = convertFloatToPcm16(params.peak);
    }
    else
    {
      buf[sample] = IasLocalAudioBuffer::AudioData(params.peak * mConversionGain);
    }

    if (params.buf1 > params.buf2)
    {
      buf[sample] = IasLocalAudioBuffer::AudioData(-buf[sample]);
    }

    params.buf1 += params.coeff;
    if (params.buf1 >= 1.0f)
    {
      // do not reset to start value, but rather subtract 1.0
      // retaining the fractional part will ensure frequency resolution
      params.buf1 -= 1.0f;
    }
  }

  return numSamples;
}

uint32_t IasTestToneStream::generateSawtoothWaveRising(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params)
{
  for (uint32_t sample = 0u; sample < numSamples; sample++)
  {
    if (mUseSaturation)
    {
      buf[sample] = convertFloatToPcm16(params.buf1);
    }
    else
    {
      buf[sample] = IasLocalAudioBuffer::AudioData(params.buf1 * mConversionGain);
    }

    params.buf1 += params.coeff;
    if (params.buf1 > params.peak)
    {
      // do not reset to start value, but rather subtract 2 * peak
      // retaining the fractional part will ensure frequency resolution
      params.buf1 -= params.buf2;
    }
  }

  return numSamples;
}

uint32_t IasTestToneStream::generateSawtoothWaveFalling(IasLocalAudioBuffer::AudioData *buf, uint32_t numSamples, GeneratorParams & params)
{
  for (uint32_t sample = 0u; sample < numSamples; sample++)
  {
    if (mUseSaturation)
    {
      buf[sample] = convertFloatToPcm16(params.buf1);
    }
    else
    {
      buf[sample] = static_cast<IasLocalAudioBuffer::AudioData>(params.buf1 * mConversionGain);
    }

    params.buf1 -= params.coeff;
    if (params.buf1 < -params.peak)
    {
      // do not reset to start value, but rather add 2 * peak
      // retaining the fractional part will ensure frequency resolution
      params.buf1 += params.buf2;
    }
  }

  return numSamples;
}

IasAvbProcessingResult IasTestToneStream::setChannelParams(uint16_t channel,
    uint32_t signalFrequency,
    int32_t level,
    IasAvbTestToneMode mode,
    int32_t userParam
    )
{
  IasAvbProcessingResult result = eIasAvbProcOK;

  if (!isInitialized())
  {
    result = eIasAvbProcNotInitialized;
  }
  else if ((channel >= mNumChannels) || ((2u * signalFrequency) > mSampleFrequency))
  {
    result = eIasAvbProcInvalidParam;
  }
  else
  {
    AVB_ASSERT(mChannels.size() == mNumChannels);

    switch (mode)
    {
    case IasAvbTestToneMode::eIasAvbTestToneSine:
      // no params to validate
      break;
    case IasAvbTestToneMode::eIasAvbTestTonePulse:
      // userParam is duty cycle and must be between 0 and 100
      if ((userParam < 0) || (userParam > 100))
      {
        result = eIasAvbProcInvalidParam;
      }
      break;
    case IasAvbTestToneMode::eIasAvbTestToneSawtooth:
      // userParam is -1 for falling and +1 for rising
      if (((userParam * userParam) != 1))
      {
        result = eIasAvbProcInvalidParam;
      }
      break;
    case IasAvbTestToneMode::eIasAvbTestToneFile:
      // not supported yet
      result = eIasAvbProcUnsupportedFormat;
      break;
    default:
      result = eIasAvbProcInvalidParam;
      break;
    }

    if (eIasAvbProcOK == result)
    {
      ChannelData & c = mChannels[channel];
      c.params.signalFreq = signalFrequency;
      c.params.level = level;
      c.params.mode = mode;
      c.params.userParam = userParam;
      calcCoeff(mSampleFrequency, c.params);
      setProcessMethod(c);
    }
  }

  return result;
}

inline int16_t IasTestToneStream::convertFloatToPcm16(AudioData val)
{
  /**
   * Apply output gain and store the lowest single-precision floating-point value
   * of a __m128 register to a int16_t* memory location. This function does a
   * conversion from float to int16_t.
   */

  /** NOTE:
   * This is not fully optimized yet, as the SSE instructions could convert up to four samples
   * in parallel. Let's wait for the performance toll to decide if it's needed.
   */

#ifndef __SSE__ // avoid compilation errors on machines used for unit-testing
  val = val > 1.0f ? 1.0f : (val < -1.0f ? -1.0f : val);
  return int16_t(val * mConversionGain);
#else

  const __m128  cConversionFactor_mm  = _mm_load1_ps(&mConversionGain);
  __m128 a = _mm_load1_ps(&val);;
  a = _mm_mul_ps(a, cConversionFactor_mm); // multiply with 2^15-1 and apply output gain
  __m128i b  = _mm_cvtps_epi32(a);         // convert from float to int32_t
  b = _mm_packs_epi32(b,b);                // convert int32_t to int16_t with saturation
  int32_t intValue = _mm_cvtsi128_si32(b);   // move the lowest 32 bit to scalar intValue
  return int16_t(intValue);

#endif
}

} // namespace IasMediaTransportAvb
