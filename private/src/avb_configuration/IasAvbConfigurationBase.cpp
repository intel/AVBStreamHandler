/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbConfigurationBase.cpp
 * @brief   Base class to facilitate StreamHandler configuration.
 * @date    2013
 */

#include "media_transport/avb_configuration/IasAvbConfigurationBase.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbConfigRegistryInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbRegistryKeys.hpp"
#include "avb_helper/ias_visibility.h"
#include "avb_helper/ias_debug.h"
#include <cstring>
#include <algorithm>

using namespace IasMediaTransportAvb;

StreamParamsAvbRx IasAvbConfigurationBase::DefaultSetupAvbRx[] =
{
    { 'H', 2u, 48000u, 0x0u, 0x91E0F0000000u, 2u, 0u, 0u },
    cTerminator_StreamParamsAvbRx
};

StreamParamsAvbTx IasAvbConfigurationBase::DefaultSetupAvbTx[] =
{
    { 'H', 2u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE010000u, 0x91E0F000FE01u, 1u, true },
    cTerminator_StreamParamsAvbTx
};

IasAvbConfiguratorInterface * IasAvbConfigurationBase::instance = NULL;


IasAvbConfigurationBase::IasAvbConfigurationBase()
  : mRegistry(NULL)
  , mAvbStreamsRx(DefaultSetupAvbRx)
  , mAvbStreamsTx(DefaultSetupAvbTx)
  , mAvbVideoStreamsRx(NULL)
  , mAvbVideoStreamsTx(NULL)
  , mAvbClkRefStreamRx(NULL)
  , mAvbClkRefStreamTx(NULL)
  , mAlsaStreams(NULL)
  , mVideoStreams(NULL)
  , mTestStreams(NULL)
  , mNumAvbStreamsRx(static_cast<uint32_t>((sizeof DefaultSetupAvbRx) / (sizeof DefaultSetupAvbRx[0])))
  , mNumAvbStreamsTx(static_cast<uint32_t>((sizeof DefaultSetupAvbTx) / (sizeof DefaultSetupAvbTx[0])))
  , mNumAvbVideoStreamsRx(0u)
  , mNumAvbVideoStreamsTx(0u)
  , mNumAvbClkRefStreamsRx(0u)
  , mNumAvbClkRefStreamsTx(0u)
  , mNumAlsaStreams(0u)
  , mNumVideoStreams(0u)
  , mNumTestStreams(0u)
  , mUseDefaultChannelLayout(true)
  , mUseDefaultDmac(true)
  , mUseFixedClock(0)
  , mUseHwC(0)
  , mUseClkRec(false)
  , mVerbosity(0)
  , mProfileSet(false)
  , mTargetSet(false)
{
  if (NULL == instance)
  {
    instance = this;
  }
  else
  {
    std::cerr << "AVB_ERR:Warning: configuration already instantiated\n";
  }
}


bool IasAvbConfigurationBase::passArguments(int32_t argc, char** argv, int32_t verbosity, IasAvbConfigRegistryInterface & registry)
{
  int32_t c;
  uint32_t index = 0u;
  bool isTxStream = false;
  ContinueStatus cont = eContinue;

  mVerbosity = verbosity;
  mRegistry = &registry;

  // these need reinit to enable config to be used by multiple subsequent unit tests
  mUseDefaultChannelLayout = true;
  mUseDefaultDmac = true;
  mUseFixedClock = 0;
  mUseHwC = 0;
  mProfileSet = false;
  mTargetSet = false;
  mUseClkRec = false;

  static const struct option defaultOptions[] =
  {
      { "system",       required_argument, 0, 's' },
      { "profile",      required_argument, 0, 'p' },
      { "target",       required_argument, 0, 't' },
      { "ifname",       required_argument, 0, 'n' },
      { "clockdriver",  required_argument, 0, 'e' },
#if IAS_PREPRODUCTION_SW
      { "numch",        required_argument, 0, 'c' },
      { "ch_layout",    required_argument, 0, 'l' },
      { "streamId",     required_argument, 0, 'i' },
      { "dmac",         required_argument, 0, 'm' },
      { "local",        required_argument, 0, 'o' },
      { "index_rx",     required_argument, 0, 'x' },
      { "index_tx",     required_argument, 0, 'X' },
      { "numstreams",   required_argument, 0, 'a' },
      { "fixed_clock",  no_argument,       &mUseFixedClock, 1 },
      { "hwcapture",    no_argument,       &mUseHwC, 1 },
      { "nohwcapture",  no_argument,       &mUseHwC, 0 },
      { "config",       required_argument, 0, 'k' },
      { "query",        no_argument,       0, 'q' },
#endif
      { "help",         no_argument,       0, 'h' },
      { NULL,           0,                 0,  0  }
  };

  const struct option * options = defaultOptions;
  cont = preParseArguments(&options);

  if (eContinue == cont)
  {
    std::string optStr = "+";
    for (int32_t i = 0u; (NULL != options[i].name); i++)
    {
      if (0 == options[i].flag)
      {
        optStr += char(options[i].val);
        switch (options[i].has_arg)
        {
        case optional_argument:
          optStr += "::";
          break;

        case required_argument:
          optStr += ":";
          break;

        case no_argument:
        default:
          break;
        }
      }
      else
      {
        if (options[i].has_arg != no_argument)
        {
          std::cerr << "AVB_ERR:Warning: inconsistent options table entry: " << options[i].name << std::endl;
        }
      }
    }

    optind = 0;
    for (;;)
    {
      int32_t optIdx = 0;
      c = getopt_long(argc, argv, optStr.c_str(), options, &optIdx);

      if ((-1 == c) || (eError == cont))
      {
        break;
      }

      if (0 == c && NULL != options[optIdx].flag)
      {
        // Flags have already been handled.
        continue;
      }


      // required_argument in options structure prevents situation where optarg is NULL
      // if argument is missing, the next option becomes an argument and pointer of optarg is still valid
      // the last opt without an argument is caught and reported as "?" sign
      switch (c)
      {
        case 't':
        {
          if (mTargetSet)
          {
            std::cerr << "AVB_WARNING: More than one --target option, ignored " << optarg << std::endl;
          }
          else
          {
            cont = handleTargetOption(optarg);
          }
        }
        break;

        case 's':
          std::cerr << "AVB_WARNING: option '-s/--system' is deprecated! Use '-p/--profile' for option '" << optarg << "' instead" << std::endl;
          // fall-through
        case 'p':
        {
          if (mProfileSet)
          {
            std::cerr << "AVB_WARNING: More than one --profile option, ignored " << optarg << std::endl;
          }
          else
          {
            cont = handleProfileOption(optarg);
          }
        }
        break;

        case 'x': // set index for rx streams
        {
          std::stringstream(std::string(optarg)) >> index;
          isTxStream = false;
          if (mVerbosity > 0)
          {
            std::cout << "AVB_LOG:Index for rx or local stream set to " << index << std::endl;
          }
        }
        break;

        case 'X': // set index for tx streams (or local streams)
        {
          std::stringstream(std::string(optarg)) >> index;
          isTxStream = true;
          if (mVerbosity > 0)
          {
            std::cout << "AVB_LOG:Index for tx or local stream set to " << index << std::endl;
          }
        }
        break;

        case 'a':
        {
          std::cerr << "AVB_ERR:Option '-a' is deprecated " << std::endl;
        }
        break;

        case 'c':
        {
          int32_t maxIndex = 0;
          uint16_t numCh;
          std::stringstream(std::string(optarg)) >> numCh;
          cont = eError;
          if (isTxStream)
          {
            maxIndex = mNumAvbStreamsTx - 1;
            if (index < mNumAvbStreamsTx)
            {
              AVB_ASSERT(NULL != mAvbStreamsTx);
              mAvbStreamsTx[index].maxNumChannels = numCh;
              cont = eContinue;
            }
          }
          else
          {
            maxIndex = mNumAvbStreamsRx - 1;
            if (index < mNumAvbStreamsRx)
            {
              AVB_ASSERT(NULL != mAvbStreamsRx);
              mAvbStreamsRx[index].maxNumChannels = numCh;
              cont = eContinue;
            }
          }
          if (eError == cont)
          {
            std::cerr << "AVB_ERR:Option '-c':Invalid index (" << index << ") for AVB "
                << (isTxStream ? "tx" : "rx") << " stream, max index = " << maxIndex << std::endl;
          }
          else
          {
            if (mVerbosity > 0)
            {
              std::cout << "AVB_LOG:" << (isTxStream ? "tx" : "rx") << " stream at index "
                  << index << " has set number of channels to " << numCh << std::endl;
            }
          }
        }
        break;

        case 'o':
        {
          cont = eError;
          int32_t maxIndex = 0;
          if (isTxStream)
          {
            maxIndex = mNumAvbStreamsTx - 1;
            if (index < mNumAvbStreamsTx)
            {
              AVB_ASSERT(NULL != mAvbStreamsTx);
              (void) getHexVal(mAvbStreamsTx[index].localStreamdIDToConnect, std::string("local stream id"));
              cont = eContinue;
            }
          }
          else
          {
            maxIndex = mNumAvbStreamsRx - 1;
            if (index < mNumAvbStreamsRx)
            {
              AVB_ASSERT(NULL != mAvbStreamsRx);
              (void) getHexVal(mAvbStreamsRx[index].localStreamdIDToConnect, std::string("local stream id"));
              cont = eContinue;
            }
          }
          if (eError == cont)
          {
            std::cerr << "AVB_ERR:Option '-o':Invalid index (" << index << ") for AVB "
                << (isTxStream ? "tx" : "rx") << " stream, max index = " << maxIndex << std::endl;
          }
        }
        break;

        case 'l':
        {
          int32_t maxIndex = mNumAlsaStreams - 1;
          if (index < mNumAlsaStreams)
          {
            AVB_ASSERT(NULL != mAlsaStreams);
            (void) getHexVal(mAlsaStreams[index].layout, std::string("channel layout"));
          }
          else
          {
            std::cerr << "AVB_ERR:Option '-l':Invalid index (" << index << ") for local stream, max index = "
                << maxIndex << std::endl;
            cont = eError;
          }
        }
        break;

        case 'i':
        {
          cont = eError;
          int32_t maxIndex = 0;
          if (isTxStream)
          {
            maxIndex = mNumAvbStreamsTx - 1;
            if (index < mNumAvbStreamsTx)
            {
              AVB_ASSERT(NULL != mAvbStreamsTx);
              (void) getHexVal(mAvbStreamsTx[index].streamId, std::string("streamId"));
              cont = eContinue;
            }
          }
          else
          {
            maxIndex = mNumAvbStreamsRx - 1;
            if (index < mNumAvbStreamsRx)
            {
              AVB_ASSERT(NULL != mAvbStreamsRx);
              (void) getHexVal(mAvbStreamsRx[index].streamId, std::string("streamId"));
              cont = eContinue;
            }
          }
          if (eError == cont)
          {
            std::cerr << "AVB_ERR:Option '-i':Invalid index (" << index << ") for AVB "
                << (isTxStream ? "tx" : "rx") << " stream, max index = " << maxIndex << std::endl;
          }
        }
        break;

        case 'm':
        {
          cont = eError;
          int32_t maxIndex = 0;
          if (isTxStream)
          {
            maxIndex = mNumAvbStreamsTx - 1;
            if (index < mNumAvbStreamsTx)
            {
              AVB_ASSERT(NULL != mAvbStreamsTx);
              (void) getHexVal(mAvbStreamsTx[index].dMac, std::string("destination mac"), 0xFFFFFFFFFFFFu);
              cont = eContinue;
            }
          }
          else
          {
            maxIndex = mNumAvbStreamsRx - 1;
            if (index < mNumAvbStreamsRx)
            {
              AVB_ASSERT(NULL != mAvbStreamsRx);
              (void) getHexVal(mAvbStreamsRx[index].dMac, std::string("destination mac"), 0xFFFFFFFFFFFFu);
              cont = eContinue;
            }
          }
          if (eError == cont)
          {
            std::cerr << "AVB_ERR:Option '-m':Invalid index (" << index << ") for AVB "
                << (isTxStream ? "tx" : "rx") << " stream, max index = " << maxIndex << std::endl;
          }
        }
        break;

        case 'n':
        {
          registry.setConfigValue(IasRegKeys::cNwIfName, optarg);
          if (mVerbosity > 0)
          {
            std::cout << "AVB_LOG:Network interface name set to " << optarg << std::endl;
          }
        }
        break;

        case 'e':
        {
          registry.setConfigValue(IasRegKeys::cClockDriverFileName, optarg);
          mUseClkRec = true;
  #ifdef IAS_HOST_BUILD
          registry.setConfigValue("clockdriver.config.i2cdevice", "/dev/null");
  #else
          registry.setConfigValue("clockdriver.config.i2cdevice", "/dev/i2c-0");
  #endif
        }
        break;

        case 'k':
        {
          std::string arg = optarg;
          size_t delim = arg.find('=');
          if ((std::string::npos == delim) || (arg.length() == (delim + 1u)))
          {
            std::cerr << "AVB_ERR:Config: invalid argument '" << arg << "'" << std::endl;
            cont = eError;
          }
          else
          {
            std::string key = arg.substr(0, delim);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            std::string valText = arg.substr(delim + 1u);
            std::stringstream parser(valText);
            uint64_t valNum;

            if (valText.substr(0, 2) == "0x")
            {
              parser.ignore(2);
              parser.setf(std::ios::hex, std::ios::basefield);
            }

            if ((parser >> valNum).fail())
            {
              if (mVerbosity > 0)
              {
                std::cout << "AVB_LOG:Setting " << key << " to '" << valText << "'" << std::endl;
              }
              registry.setConfigValue(key, valText);
            }
            else
            {
              if (mVerbosity > 0)
              {
                std::cout << "AVB_LOG:Setting " << key << " to " << valNum << " ("
                    << std::hex << valNum << ')' << std::endl;
              }
              registry.setConfigValue(key, valNum);
            }
          }
        }
        break;

        case 'q':
        {
          std::cout << "AVB_LOG:Number of rx streams:    " << mNumAvbStreamsRx << std::endl;
          std::cout << "AVB_LOG:Number of tx streams:    " << mNumAvbStreamsTx << std::endl;
        }
        break;

        case 'h':
        {
          cont = eError;
        }
        break;

        case '?':
        default:
        {
          cont = handleDerivedOptions(c, index);
        }
        break;
      }
    }
  }

  if (eContinue == cont)
  {
    if (!mProfileSet)
    {
      std::cerr << "AVB_ERR:Profile parameter not provided" << std::endl;
      cont = eError;
    }
    else if (!mTargetSet)
    {
      std::cerr << "AVB_ERR:Target parameter not provided" << std::endl;
      cont = eError;
    }
  }

  if (eContinue == cont)
  {
    cont = postParseArguments();
  }

  if ((eError != cont) && (optind < argc))
  {
    std::cerr << "unrecognized argument: " << argv[optind] << "\n" << std::endl;
    cont = eError;
  }

  if (eError == cont)
  {
    std::cout << "AVB_LOG:Options for config module:" << std::endl;
    for (uint32_t i = 0u; (NULL != options[i].name); i++)
    {
      if (0 != options[i].flag)
      {
        std::cout << "\t--" << options[i].name << std::endl;
      }
      else
      {
        std::cout << "\t--" << options[i].name << " or -" << char(options[i].val) << std::endl;
      }
    }
  }

  return (eError != cont);
}


IasAvbConfigurationBase::ContinueStatus IasAvbConfigurationBase::handleDerivedOptions(int32_t c, uint32_t index)
{
  (void) index;
  std::cerr << "AVB_ERR:Unrecognized option: " << c << std::endl;
  return eError;
}


bool IasAvbConfigurationBase::setup(IasAvbStreamHandlerInterface* streamHandler)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultOk;
  uint32_t i;
  int32_t clkIdx = -1;
  ContinueStatus cont = eContinue;

  if (NULL == streamHandler)
  {
    cont = eError;
  }
  else
  {
    cont = preSetup(streamHandler);
  }

  if (eContinue == cont)
  {
    cont = setupTestStreams(streamHandler);
  }

  uint32_t rxClockId = 0u;

  if (eContinue == cont)
  {
    //
    // create and connect the AVB streams
    //

    if (NULL != mAvbClkRefStreamRx)
    {
      if (1u < mNumAvbClkRefStreamsRx)
      {
        // creating more than one clock ref stream not supported currently
        result = IasAvbResult::eIasAvbResultNotSupported;
      }
      else
      {
        for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbClkRefStreamsRx); i++)
        {
          result = streamHandler->createReceiveClockReferenceStream(
              (mAvbClkRefStreamRx[i].srClass == 'H') ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow,
              mAvbClkRefStreamRx[i].type,
              mAvbClkRefStreamRx[i].maxCrfStampsPerPdu,
              mAvbClkRefStreamRx[i].streamId,
              mAvbClkRefStreamRx[i].dMac,
              mAvbClkRefStreamRx[i].clockId // might be changed
              );

          rxClockId = mAvbClkRefStreamRx[i].clockId;
          clkIdx = i;
        }
      }
    }

    if (NULL != mAvbClkRefStreamTx)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbClkRefStreamsTx); i++)
      {
        result = streamHandler->createTransmitClockReferenceStream(
            (mAvbClkRefStreamTx[i].srClass == 'H') ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow,
            IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio,
            mAvbClkRefStreamTx[i].crfStampsPerPdu,
            mAvbClkRefStreamTx[i].crfStampInterval,
            mAvbClkRefStreamTx[i].baseFreq,
            mAvbClkRefStreamTx[i].pull,
            mAvbClkRefStreamTx[i].clockId,
            mAvbClkRefStreamTx[i].assignMode,
            mAvbClkRefStreamTx[i].streamId,
            mAvbClkRefStreamTx[i].dMac,
            mAvbClkRefStreamTx[i].activate
            );
        std::cout << "AVB_LOG:Result of createTransmitClockReferenceStream = " << result << std::endl;
      }
    }

    if (NULL != mAvbStreamsRx)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbStreamsRx); i++)
      {
        // create regular audio stream
        result = streamHandler->createReceiveAudioStream(
            (mAvbStreamsRx[i].srClass == 'H') ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow,
            mAvbStreamsRx[i].maxNumChannels,
            mAvbStreamsRx[i].sampleFreq,
            mAvbStreamsRx[i].streamId,
            mAvbStreamsRx[i].dMac
            );
      }
    }

    if (NULL != mAvbStreamsTx)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbStreamsTx); i++)
      {
        uint32_t clockId = mAvbStreamsTx[i].clockId;

        result = streamHandler->createTransmitAudioStream(
            (mAvbStreamsTx[i].srClass == 'H') ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow,
            mAvbStreamsTx[i].maxNumChannels,
            mAvbStreamsTx[i].sampleFreq,
            IasAvbAudioFormat::eIasAvbAudioFormatSaf16,
            clockId,
            IasAvbIdAssignMode::eIasAvbIdAssignModeStatic,
            mAvbStreamsTx[i].streamId,
            mAvbStreamsTx[i].dMac,
            mAvbStreamsTx[i].activate
            );
      }
    }

    if (NULL != mAvbVideoStreamsRx)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbVideoStreamsRx); i++)
      {
        result = streamHandler->createReceiveVideoStream(
            (mAvbVideoStreamsRx[i].srClass == 'H') ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow,
            mAvbVideoStreamsRx[i].maxPacketRate,
            static_cast<uint16_t>(mAvbVideoStreamsRx[i].maxPacketSize),
            mAvbVideoStreamsRx[i].format,
            mAvbVideoStreamsRx[i].streamId,
            mAvbVideoStreamsRx[i].dMac
            );
      }
    }

    if (NULL != mAvbVideoStreamsTx)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbVideoStreamsTx); i++)
      {
        result = streamHandler->createTransmitVideoStream(
            (mAvbVideoStreamsTx[i].srClass == 'H') ? IasAvbSrClass::eIasAvbSrClassHigh : IasAvbSrClass::eIasAvbSrClassLow,
            mAvbVideoStreamsTx[i].maxPacketRate,
            static_cast<uint16_t>(mAvbVideoStreamsTx[i].maxPacketSize),
            mAvbVideoStreamsTx[i].format,
            mAvbVideoStreamsTx[i].clockId,
            IasAvbIdAssignMode::eIasAvbIdAssignModeStatic,
            mAvbVideoStreamsTx[i].streamId,
            mAvbVideoStreamsTx[i].dMac,
            mAvbVideoStreamsTx[i].activate
            );
      }
    }

    if (NULL != mAlsaStreams)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAlsaStreams); i++)
      {
        result = streamHandler->createAlsaStream(
            mAlsaStreams[i].streamDirection,
            mAlsaStreams[i].numChannels,
            mAlsaStreams[i].sampleFreq,
            IasAvbAudioFormat::eIasAvbAudioFormatSaf16,
            mAlsaStreams[i].clockId,
            mAlsaStreams[i].periodSize,
            mAlsaStreams[i].numPeriods,
            mAlsaStreams[i].layout,
            mAlsaStreams[i].hasSideChannel,
            mAlsaStreams[i].deviceName,
            mAlsaStreams[i].streamId,
            mAlsaStreams[i].alsaDeviceType,
            mAlsaStreams[i].sampleFreqASRC
            );
      }
    }

    if (NULL != mVideoStreams)
    {
      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumVideoStreams); i++)
      {
        result = streamHandler->createLocalVideoStream(
            mVideoStreams[i].streamDirection,
            mVideoStreams[i].maxPacketRate,
            mVideoStreams[i].maxPacketSize,
            mVideoStreams[i].format,
            mVideoStreams[i].ipcName,
            mVideoStreams[i].streamId
            );
      }
    }

    // setup for clock recovery.
    if (NULL != mAvbStreamsRx)
    {
      IasAvbResult result = IasAvbResult::eIasAvbResultOk;

      bool recoverFromAudioStream = false;
      if (0u == rxClockId)
      {
        // no RX clock ref stream defined yet, scan RX streams
        for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumAvbStreamsRx); i++)
        {
          // If a slave clock id is specified for an RX stream, a new clock domain is derived based on this RX stream.
          // Therefore a clock driver library has to be provided.
          if (0 != mAvbStreamsRx[i].slaveClockId)
          {
            result = streamHandler->deriveClockDomainFromRxStream(mAvbStreamsRx[i].streamId, rxClockId);
            if (IasAvbResult::eIasAvbResultOk != result)
            {
              std::cerr << "warning: could not derive clock domain (" << result << ")" << std::endl;
            }
            else
            {
              std::cout << "created RX clock domain 0x" << std::hex << rxClockId << std::endl;
              recoverFromAudioStream = true;
            }
            clkIdx = i;
            break;
          }
        }
      }

      if ((IasAvbResult::eIasAvbResultOk == result) && (0u != rxClockId) && mUseClkRec)
      {
        AVB_ASSERT(clkIdx >= 0);

        uint32_t slaveClockId = 0u;
        uint32_t clockDriverId = 0u;
        if (recoverFromAudioStream)
        {
          AVB_ASSERT(clkIdx < int32_t(getNumEntries(mAvbStreamsRx)));
          slaveClockId = mAvbStreamsRx[clkIdx].slaveClockId;
          clockDriverId = mAvbStreamsRx[clkIdx].clockDriverId;
        }
        else
        {
          AVB_ASSERT(clkIdx < int32_t(getNumEntries(mAvbClkRefStreamRx)));
          slaveClockId = mAvbClkRefStreamRx[clkIdx].slaveClockId;
          clockDriverId = mAvbClkRefStreamRx[clkIdx].clockDriverId;
        }
        // if hardware capture is requested (option is selected in command line) use HW capture clock domain
        if (0 != mUseHwC)
        {
          slaveClockId = cIasAvbHwCaptureClockDomainId;
        }
        result = streamHandler->setClockRecoveryParams(rxClockId, slaveClockId, clockDriverId);
        if (IasAvbResult::eIasAvbResultOk != result)
        {
            std::cerr << "warning: could not set clock recovery params (" << result << ")" << std::endl;
        }
      }
    }

    // Start connecting the streams

    if (IasAvbResult::eIasAvbResultOk == result)
    {
      if (mVerbosity > 0)
      {
        std::cout << "AVB_LOG:Starting auto connect" << std::endl;
      }

      for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < (mNumAvbStreamsRx)); i++)
      {
        AVB_ASSERT(NULL != mAvbStreamsRx); // if mNumAvbStreamsRx > 0 mAvbStreamsRx is never NULL!
        if (0u != mAvbStreamsRx[i].localStreamdIDToConnect)
        {
          result = streamHandler->connectStreams(mAvbStreamsRx[i].streamId, mAvbStreamsRx[i].localStreamdIDToConnect);
        }
      }
      if (IasAvbResult::eIasAvbResultOk == result)
      {
        for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < (mNumAvbStreamsTx)); i++)
        {
          AVB_ASSERT(NULL != mAvbStreamsTx); // if mNumAvbStreamsTx > 0 mAvbStreamsTx is never NULL!
          if (0u != mAvbStreamsTx[i].localStreamdIDToConnect)
          {
            result = streamHandler->connectStreams(mAvbStreamsTx[i].streamId, mAvbStreamsTx[i].localStreamdIDToConnect);
          }
        }
      }
      if (IasAvbResult::eIasAvbResultOk == result)
      {
        for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < (mNumAvbVideoStreamsRx)); i++)
        {
          AVB_ASSERT(NULL != mAvbVideoStreamsRx); // if mNumAvbStreamsVideoRx > 0 mAvbStreamsVideoRx is never NULL!
          if (0u != mAvbVideoStreamsRx[i].localStreamdIDToConnect)
          {
            result = streamHandler->connectStreams(mAvbVideoStreamsRx[i].streamId, mAvbVideoStreamsRx[i].localStreamdIDToConnect);
          }
        }
      }
      if (IasAvbResult::eIasAvbResultOk == result)
      {
        for (i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < (mNumAvbVideoStreamsTx)); i++)
        {
          AVB_ASSERT(NULL != mAvbVideoStreamsTx); // if mNumAvbStreamsTx > 0 mAvbStreamsTx is never NULL!

          if (0u != mAvbVideoStreamsTx[i].localStreamdIDToConnect)
          {
            result = streamHandler->connectStreams(mAvbVideoStreamsTx[i].streamId, mAvbVideoStreamsTx[i].localStreamdIDToConnect);
          }
        }
      }
    }
  }

  if ((eContinue == cont) && (IasAvbResult::eIasAvbResultOk == result))
  {
    cont = postSetup(streamHandler);
  }

  return (eError != cont) && (IasAvbResult::eIasAvbResultOk == result);
}


IasAvbConfigurationBase::ContinueStatus IasAvbConfigurationBase::handleTargetOption(const std::string & name)
{
  (void) name;
  ContinueStatus ret = eContinue;
  bool found = false;

  TargetParams *targets = NULL;
  uint32_t numTargets = getTargets(targets);

  AVB_ASSERT(NULL != targets);

  if (NULL != targets)
  {
    for (uint32_t i = 0; i < numTargets; i++)
    {
      if ((targets[i].targetName == name) && (NULL != mRegistry))
      {
        mRegistry->setConfigValue(IasRegKeys::cNwIfName, targets[i].ifName);

        // if additional registry entries are available get and set them
        if (NULL != targets[i].configReg)
        {
          setRegistryValues(targets[i].configReg);
        }

        std::cout << "AVB_LOG:Target set to " << targets[i].targetName << std::endl;

        found = true;
        mTargetSet = true;
        break;
      }
    }

    if (!found)
    {
      std::cerr << "unknown target name " << name << std::endl;
      ret = eError;
    }
  }

  return ret;
}

bool IasAvbConfigurationBase::setRegistryValues(const RegistryEntries* regValues)
{
  bool ret = false;
  IasAvbResult result;

  if ((NULL != regValues) && (NULL != mRegistry))
  {
    uint32_t numRegValues = getNumEntries(regValues);
    for (uint32_t i = 0; i < numRegValues; i++)
    {
      if (regValues[i].isNum)
      {
        result = mRegistry->setConfigValue(regValues[i].keyName, regValues[i].numValue);
      }
      else
      {
        AVB_ASSERT(NULL != regValues[i].textValue);
        result = mRegistry->setConfigValue(regValues[i].keyName, regValues[i].textValue);
      }

      if (IasAvbResult::eIasAvbResultOk != result)
      {
        std::cerr << "AVB_ERR:Couldn't set registry value (Error " << result << "):" << regValues[i].keyName << "=";
        regValues[i].isNum ? std::cerr << regValues[i].numValue : std::cerr << regValues[i].textValue;
        std::cerr << std::endl;
      }
    }
  }

  return ret;
}


void IasAvbConfigurationBase::getProfileInfo(const ProfileParams &theProfile)
{
  // get AVB Rx stream info
  mNumAvbStreamsRx = 0;
  if (NULL != theProfile.configAvbRx)
  {
    mNumAvbStreamsRx = getNumEntries(theProfile.configAvbRx);
    mAvbStreamsRx = (0 == mNumAvbStreamsRx) ? NULL : theProfile.configAvbRx;
  }

  // get AVB Tx stream info
  mNumAvbStreamsTx = 0;
  if (NULL != theProfile.configAvbTx)
  {
    mNumAvbStreamsTx = getNumEntries(theProfile.configAvbTx);
    mAvbStreamsTx = (0 == mNumAvbStreamsTx) ? NULL : theProfile.configAvbTx;
  }

  // get AVB Video Rx stream info
  mNumAvbVideoStreamsRx = 0;
  if (NULL != theProfile.configAvbVideoRx)
  {
    mNumAvbVideoStreamsRx = getNumEntries(theProfile.configAvbVideoRx);
    mAvbVideoStreamsRx = (0 == mNumAvbVideoStreamsRx) ? NULL : theProfile.configAvbVideoRx;
  }

  // get AVB Video Tx stream info
  mNumAvbVideoStreamsTx = 0;
  if (NULL != theProfile.configAvbVideoTx)
  {
    mNumAvbVideoStreamsTx = getNumEntries(theProfile.configAvbVideoTx);
    mAvbVideoStreamsTx = (0 == mNumAvbVideoStreamsTx) ? NULL : theProfile.configAvbVideoTx;
  }

  // get AVB Clock Reference Tx stream info
  mNumAvbClkRefStreamsTx = 0;
  if (NULL != theProfile.configAvbClkRefStreamTx)
  {
    mNumAvbClkRefStreamsTx = getNumEntries(theProfile.configAvbClkRefStreamTx);
    mAvbClkRefStreamTx = (0 == mNumAvbClkRefStreamsTx) ? NULL : theProfile.configAvbClkRefStreamTx;
  }

  // get AVB Clock Reference Rx stream info
  mNumAvbClkRefStreamsRx = 0;
  if (NULL != theProfile.configAvbClkRefStreamRx)
  {
    mNumAvbClkRefStreamsRx = getNumEntries(theProfile.configAvbClkRefStreamRx);
    mAvbClkRefStreamRx = (0 == mNumAvbClkRefStreamsRx) ? NULL : theProfile.configAvbClkRefStreamRx;
  }

  // get Alsa stream info
  mNumAlsaStreams = 0;
  if (NULL != theProfile.configAlsa)
  {
    mNumAlsaStreams = getNumEntries(theProfile.configAlsa);
    mAlsaStreams = (0 == mNumAlsaStreams) ? NULL : theProfile.configAlsa;
  }

  mNumVideoStreams = 0;
  if (NULL != theProfile.configVideo)
  {
    mNumVideoStreams = getNumEntries(theProfile.configVideo);
    mVideoStreams = (0 == mNumVideoStreams) ? NULL : theProfile.configVideo;
  }

  // get Test stream info
  mNumTestStreams = 0;
  if (NULL != theProfile.configTestTone)
  {
    mNumTestStreams = getNumEntries(theProfile.configTestTone);
    mTestStreams = (0 == mNumTestStreams) ? NULL : theProfile.configTestTone;
  }
}


IasAvbConfigurationBase::ContinueStatus IasAvbConfigurationBase::handleProfileOption(const std::string & name)
{
  ContinueStatus ret = eContinue;
  bool found = false;
  ProfileParams *profiles = NULL;
  uint32_t numProfiles = getProfiles(profiles);

  AVB_ASSERT(profiles);

  if (NULL != profiles)
  {
    for (uint32_t i = 0; i < numProfiles; i++)
    {
      if (name == profiles[i].profileName)
      {
        getProfileInfo(profiles[i]);

        // if additional registry entries are available get and set them
        if (NULL != profiles[i].configReg)
        {
          setRegistryValues(profiles[i].configReg);
        }

        mProfileSet = true;
        found = true;

        if (mVerbosity > 0)
        {
          std::cout << "AVB_LOG:Profile set to " << name << std::endl;
        }

        break;
      }
    }
  }

  if (!found)
  {
    std::cerr << "AVB_LOG:Unknown profile name " << name << std::endl;
    ret = eError;
  }

  return ret;
}


IasAvbConfigurationBase::ContinueStatus IasAvbConfigurationBase::setupTestStreams(IasAvbStreamHandlerInterface* api)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultOk;

  // create test streams
  if (NULL != mTestStreams)
  {
    for (uint32_t i = 0u; (IasAvbResult::eIasAvbResultOk == result) && (i < mNumTestStreams); i++)
    {
      // create the stream
      AVB_ASSERT(NULL != &mTestStreams[i]);
      uint16_t testToneStreamId = mTestStreams[i].streamId;
      result = api->createTestToneStream(mTestStreams[i].numberOfChannels, mTestStreams[i].sampleFreq,
          mTestStreams[i].format, mTestStreams[i].channelLayout, testToneStreamId);

      if (IasAvbResult::eIasAvbResultOk == result)
      {
        // set parameter for channels
        uint32_t numParams = getNumEntries(mTestStreams[i].toneParams);
        if (0 < numParams)
        {
          for (uint32_t n = 0u; (IasAvbResult::eIasAvbResultOk == result) && (n < numParams); n++)
          {
            AVB_ASSERT(NULL != mTestStreams[i].toneParams);
            TestToneParams *param = mTestStreams[i].toneParams;
            result = api->setTestToneParams(mTestStreams[i].streamId, param[n].channel,
                param[n].signalFrequency, param[n].level, param[n].mode, param[n].userParam);
            if (IasAvbResult::eIasAvbResultOk != result)
            {
              std::cerr << "ERROR: Couldn't set parameter for test tone stream " << i << "set" << n << " ("
                  << int32_t(result) << ")" << std::endl;
            }
          }
        }
        else
        {
          std::cerr << "ERROR: Parameter for test tone stream not available!" << std::endl;
          result = IasAvbResult::eIasAvbResultOk;
        }
      }

      if (IasAvbResult::eIasAvbResultOk == result)
      {
        if (mVerbosity > 0)
        {
          std::cout << "Test tone stream creation successful, id=" << testToneStreamId << "(decimal)" << std::endl;
        }
      }
      else
      {
        std::cerr << "ERROR: test tone stream creation failed (" << int32_t(result) << ")" << std::endl;
      }
    } // for
  }
  else
  {
    if (mVerbosity > 0)
    {
      std::cout << "No test streams specified" << std::endl;
    }
  }

  return (IasAvbResult::eIasAvbResultOk == result) ? eContinue : eError;
}


IasAvbConfiguratorInterface & IasAvbConfigurationBase::getInstance()
{
  return *instance;
}

// implementation of instance getter function (only function exported by the config lib)
// the only function directly exported by the shared library
IAS_DSO_PUBLIC IasAvbConfiguratorInterface & getIasAvbConfiguratorInterfaceInstance()
{
  return IasAvbConfigurationBase::getInstance();
}
