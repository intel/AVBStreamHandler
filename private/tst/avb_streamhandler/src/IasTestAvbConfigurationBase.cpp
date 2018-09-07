/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
* @file IasTestAvbConfigurationBase.cpp
* @brief Test file for IasAvbConfigurationBase
* @date 2018
*/

#include "gtest/gtest.h"

#define private public
#define protected public
#include "media_transport/avb_configuration/IasAvbConfigurationBase.hpp"
#undef protected
#undef private

#include "avb_streamhandler/IasAvbStreamHandler.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbConfigRegistryInterface.hpp"
#include "test_common/IasAvbConfigurationInfo.hpp"
#include "test_common/IasSpringVilleInfo.hpp"

#include <string>
#include <iostream>

extern std::string cIasChannelNamePolicyTest;
extern int  verbosity;

namespace IasMediaTransportAvb
{

static const uint32_t cRefClockId = 0x80864711u;

class TestRegistry : public IasAvbConfigRegistryInterface
{
  public:
    virtual ~TestRegistry() {}

    virtual IasAvbResult setConfigValue(const std::string &key, uint64_t value)
    {
      IasAvbResult ret = IasAvbResult::eIasAvbResultOk;
      (void) value;
      if (key.empty())
        ret = IasAvbResult::eIasAvbResultInvalidParam;

      return ret;
    }

    virtual IasAvbResult setConfigValue(const std::string &key, const std::string &value)
    {
      IasAvbResult ret = IasAvbResult::eIasAvbResultOk;
      (void) value;
      if (key.empty())
        ret = IasAvbResult::eIasAvbResultInvalidParam;

      return ret;
    }
};

class IasAvbStreamHandlerInterfaceImpl: public IasAvbStreamHandlerInterface
{
  public:

    IasAvbStreamHandlerInterfaceImpl():
      mRegistry(NULL)
    {}

    virtual ~IasAvbStreamHandlerInterfaceImpl()
    {
      delete mRegistry;
    }

    IasAvbProcessingResult init( const std::string& configName, bool runSetup, int setupArgc, char** setupArgv )
    {
      (void) configName;
      IasAvbProcessingResult result = eIasAvbProcOK;

      IasAvbConfiguratorInterface & config = IasAvbConfigurationBase::getInstance();
      mRegistry = new TestRegistry();

      if(!config.passArguments(setupArgc, setupArgv, verbosity, *mRegistry))
      {
        std::cerr << "[IasAvbStreamHandlerInterfaceImpl::init] Configuration library failed to parse args\n";
        result = eIasAvbProcInitializationFailed;
      }
      if ((eIasAvbProcOK == result) && runSetup)
      {
        if (!config.setup(this))
        {
          std::cerr << "[IasAvbStreamHandlerInterfaceImpl::init] config.setup failed!\n";
          result = eIasAvbProcInitializationFailed;
        }
      }
      return result;
    }

    virtual IasAvbResult createReceiveAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
        AvbStreamId streamId, MacAddress destMacAddr)
    {
      (void) srClass;
      (void) maxNumberChannels;
      (void) streamId;
      (void) destMacAddr;

      if (16000u == sampleFreq)
        return IasAvbResult::eIasAvbResultInvalidParam;

      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createTransmitAudioStream(IasAvbSrClass srClass, uint16_t maxNumberChannels, uint32_t sampleFreq,
        IasAvbAudioFormat format, uint32_t clockId, IasAvbIdAssignMode assignMode, AvbStreamId &streamId,
        MacAddress &destMacAddr, bool active)
    {
      (void) srClass;
      (void) maxNumberChannels;
      (void) format;
      (void) clockId;
      (void) assignMode;
      (void) streamId;
      (void) destMacAddr;
      (void) active;

      if (16000u == sampleFreq)
        return IasAvbResult::eIasAvbResultInvalidParam;

      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult destroyStream(AvbStreamId streamId)
    {
      (void) streamId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult setStreamActive(AvbStreamId streamId, bool active)
    {
      (void) streamId;
      (void) active;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createAlsaStream(IasAvbStreamDirection direction, uint16_t numberOfChannels,
        uint32_t sampleFreq, IasAvbAudioFormat format, uint32_t clockId, uint32_t periodSize, uint32_t numPeriods, uint8_t channelLayout,
        bool hasSideChannel, std::string const &deviceName, uint16_t &streamId, IasAlsaDeviceTypes alsaDeviceType, uint32_t sampleFreqASRC)
    {
      (void) direction;
      (void) numberOfChannels;
      (void) sampleFreq;
      (void) format;
      (void) clockId;
      (void) periodSize;
      (void) numPeriods;
      (void) channelLayout;
      (void) hasSideChannel;
      (void) deviceName;
      (void) streamId;
      (void) alsaDeviceType;
      (void) sampleFreqASRC;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createTestToneStream(uint16_t numberOfChannels,
        uint32_t sampleFreq, IasAvbAudioFormat format, uint8_t channelLayout, uint16_t &streamId)
    {
      (void) numberOfChannels;
      (void) sampleFreq;
      (void) format;
      (void) channelLayout;
      (void) streamId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult destroyLocalStream(uint16_t streamId)
    {
      (void) streamId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult connectStreams(AvbStreamId networkStreamId, uint16_t localStreamId)
    {
      (void) networkStreamId;
      (void) localStreamId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult disconnectStreams(AvbStreamId networkStreamId)
    {
      (void) networkStreamId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult setChannelLayout(uint16_t localStreamId, uint8_t channelLayout)
    {
      (void) localStreamId;
      (void) channelLayout;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult setTestToneParams(uint16_t localStreamId, uint16_t channel, uint32_t signalFrequency,
        int level, IasAvbTestToneMode mode, int userParam )
    {
      (void) localStreamId;
      (void) channel;
      (void) signalFrequency;
      (void) level;
      (void) mode;
      (void) userParam;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult deriveClockDomainFromRxStream(AvbStreamId rxStreamId, uint32_t &clockId)
    {
      (void) rxStreamId;
      (void) clockId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult setClockRecoveryParams(uint32_t masterClockId, uint32_t slaveClockId, uint32_t driverId)
    {
      (void) masterClockId;
      (void) slaveClockId;
      (void) driverId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult getAvbStreamInfo(AudioStreamInfoList &audioStreamInfo, VideoStreamInfoList &videoStreamInfo,
                                          ClockReferenceStreamInfoList &clockRefStreamInfo)
    {
      (void) audioStreamInfo;
      (void) videoStreamInfo;
      (void) clockRefStreamInfo;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult getLocalStreamInfo(LocalAudioStreamInfoList &audioStreamInfo, LocalVideoStreamInfoList &videoStreamInfo)
    {
      (void) audioStreamInfo;
      (void) videoStreamInfo;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createTransmitVideoStream(IasAvbSrClass srClass,
                                               uint16_t maxPacketRate,
                                               uint16_t maxPacketSize,
                                               IasAvbVideoFormat format,
                                               uint32_t clockId,
                                               IasAvbIdAssignMode assignMode,
                                               uint64_t &streamId,
                                               uint64_t &dmac,
                                               bool active)
    {
      (void) srClass;
      (void) maxPacketRate;
      (void) maxPacketSize;
      (void) clockId;
      (void) assignMode;
      (void) streamId;
      (void) dmac;
      (void) active;

      if (IasAvbVideoFormat::eIasAvbVideoFormatIec61883 == format)
        return IasAvbResult::eIasAvbResultInvalidParam;

      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createReceiveVideoStream(IasAvbSrClass srClass,
                                              uint16_t maxPacketRate,
                                              uint16_t maxPacketSize,
                                              IasAvbVideoFormat format,
                                              uint64_t streamId,
                                              uint64_t destMacAddr)
    {
      (void) srClass;
      (void) maxPacketRate;
      (void) maxPacketSize;
      (void) format;
      (void) streamId;
      (void) destMacAddr;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createLocalVideoStream(IasAvbStreamDirection direction,
                                           uint16_t maxPacketRate,
                                           uint16_t maxPacketSize,
                                           IasAvbVideoFormat format,
                                           const std::string &ipcName,
                                           uint16_t &streamId )
    {
      (void) direction;
      (void) maxPacketRate;
      (void) maxPacketSize;
      (void) format;
      (void) ipcName;
      (void) streamId;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createTransmitClockReferenceStream(IasAvbSrClass srClass,
                                                IasAvbClockReferenceStreamType type,
                                                uint16_t crfStampsPerPdu,
                                                uint16_t crfStampInterval,
                                                uint32_t baseFreq,
                                                IasAvbClockMultiplier pull,
                                                uint32_t clockId,
                                                IasAvbIdAssignMode assignMode,
                                                uint64_t &streamId,
                                                uint64_t &dmac,
                                                bool active)
    {
      (void) srClass;
      (void) type;
      (void) crfStampsPerPdu;
      (void) crfStampInterval;
      (void) baseFreq;
      (void) pull;
      (void) clockId;
      (void) assignMode;
      (void) streamId;
      (void) dmac;
      (void) active;
      return IasAvbResult::eIasAvbResultOk;
    }

    virtual IasAvbResult createReceiveClockReferenceStream(IasAvbSrClass srClass,
                                                IasAvbClockReferenceStreamType type,
                                                uint16_t maxCrfStampsPerPdu,
                                                uint64_t streamId,
                                                uint64_t dmac,
                                                uint32_t &clockId)
    {
      (void) srClass;
      (void) type;
      (void) maxCrfStampsPerPdu;
      (void) streamId;
      (void) dmac;
      (void) clockId;

      if (IasAvbSrClass::eIasAvbSrClassLow == srClass)
        return IasAvbResult::eIasAvbResultInvalidParam;

      return IasAvbResult::eIasAvbResultOk;
    }

    TestRegistry * mRegistry;
};

class IasAvbConfigurationBaseImpl : public IasAvbConfigurationBase
{
public:

  IasAvbConfigurationBaseImpl()
  {
    mMyTestSetupAlsaBothAvbRx = new StreamParamsAvbRx[2];
    mMyTestSetupAlsaBothAvbTx = new StreamParamsAvbTx[4];
    mAlsaTestBoth           = new StreamParamsAlsa[4];
    mRegTest            = new RegistryEntries[3];
    mTestConfigAvbVideoRx   = new StreamParamsAvbVideoRx[3];
    mTestConfigAvbVideoTx   = new StreamParamsAvbVideoTx[3];
    mTestConfigVideo        = new StreamParamsVideo[3];
    mConfigClkRefRx         = new StreamParamsAvbClockReferenceRx[2];
    mConfigClkRefTx         = new StreamParamsAvbClockReferenceTx[2];
    mNullCfgRegProfile      = new ProfileParams();
    mTestTargets            = new TargetParams[3];
    mTestProfiles           = new ProfileParams();
    init();
  }
  virtual ~IasAvbConfigurationBaseImpl()
  {
    delete[] mMyTestSetupAlsaBothAvbRx;
    delete[] mMyTestSetupAlsaBothAvbTx;
    delete[] mAlsaTestBoth;
    delete[] mRegTest;
    delete[] mTestConfigAvbVideoRx;
    delete[] mTestConfigAvbVideoTx;
    delete[] mTestConfigVideo;
    delete[] mConfigClkRefRx;
    delete[] mConfigClkRefTx;
    delete   mNullCfgRegProfile;
    delete[] mTestTargets;
    delete   mTestProfiles;
    IasAvbConfigurationBase::instance = nullptr;
  }

  virtual uint32_t getTargets(TargetParams *& targets)
  {
    targets = mTestTargets;
    return 3;
  }

  virtual uint32_t getProfiles(ProfileParams *& profiles)
  {
    profiles = mTestProfiles;
    return 1;
  }

public:

  StreamParamsAvbRx               * mMyTestSetupAlsaBothAvbRx;
  StreamParamsAvbTx               * mMyTestSetupAlsaBothAvbTx;
  StreamParamsAlsa                * mAlsaTestBoth;
  RegistryEntries                 * mRegTest;
  StreamParamsAvbVideoRx          * mTestConfigAvbVideoRx;
  StreamParamsAvbVideoTx          * mTestConfigAvbVideoTx;
  StreamParamsVideo               * mTestConfigVideo;
  StreamParamsAvbClockReferenceRx * mConfigClkRefRx;
  StreamParamsAvbClockReferenceTx * mConfigClkRefTx;
  ProfileParams                   * mNullCfgRegProfile;
  TargetParams                    * mTestTargets;
  ProfileParams                   * mTestProfiles;

public:
  void init();
  void setRxAudioSampleFreq(uint32_t freq);
}theConfigObject;

void IasAvbConfigurationBaseImpl::init()
{
  mMyTestSetupAlsaBothAvbRx[0] = { 'H', 8u, 48000u, 0x10u, 0x91E0F0000000u,  1u, 0, 0 };
  mMyTestSetupAlsaBothAvbRx[1] = cTerminator_StreamParamsAvbRx;

  mRegTest[0] = { "tspec.a.presentation.time.offset", true, 1000000u, NULL };
  mRegTest[1] = { "compatibility.audio", false, 0u, "SAF" };
  mRegTest[2] = cTerminator_RegistryEntries;

  mTestConfigAvbVideoRx[0] = { 'L', 500u, 760u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007862647u, 0x91E0F0000786u, 508u };
  mTestConfigAvbVideoRx[1] = { 'L', 500u, 760u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007882649u, 0x91E0F0000788u, 0u };
  mTestConfigAvbVideoRx[2] = cTerminator_StreamParamsAvbVideoRx;

  mTestConfigAvbVideoTx[0] = { 'L', 500u, 760, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007812642u, 0x91E0F0000781u, 501u, false };
  mTestConfigAvbVideoTx[1] = { 'L', 500u, 760, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007822643u, 0x91E0F0000782u, 0u, false };
  mTestConfigAvbVideoTx[2] = cTerminator_StreamParamsAvbVideoTx;

  mTestConfigVideo[0] = { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,500u, 760u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.1", 508u };
  mTestConfigVideo[1] = { IasAvbStreamDirection::eIasAvbTransmitToNetwork,500u, 760u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.2", 501u };
  mTestConfigVideo[2] = cTerminator_StreamParamsVideo;

  mConfigClkRefRx[0] = { 'H', IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio, (1500u - 20u) / 8u, 0x91e0f000fef91111u, 0x91e0f000fef9u, cRefClockId, 0u, 0u };
  mConfigClkRefRx[1] = cTerminator_StreamParamsAvbClockReferenceRx;

  mConfigClkRefTx[0] = { 'H', 6u, 48000u / (50u * 6u), 48000u, IasAvbClockMultiplier::eIasAvbCrsMultFlat, cIasAvbPtpClockDomainId, IasAvbIdAssignMode::eIasAvbIdAssignModeStatic, 0x91E0F000FE000000u, 0x91E0F000FE00u, true };
  mConfigClkRefTx[1] = cTerminator_StreamParamsAvbClockReferenceTx;

  mNullCfgRegProfile = new ProfileParams(
  {
      "Null_cfg_reg", mMyTestSetupAlsaBothAvbRx, mMyTestSetupAlsaBothAvbTx, mTestConfigAvbVideoRx, mTestConfigAvbVideoTx, mConfigClkRefRx, mConfigClkRefTx, /*mMyTest2chTestSetupJack,*/ mAlsaTestBoth, mTestConfigVideo, NULL, NULL
  });

  mTestTargets[0] = { "NGIO",    0x1531u, 3u, "eth1", NULL };
  mTestTargets[1] = { "BSample", 0x157Cu, 1u, "eth0", NULL };
  mTestTargets[2] = { "CvH",     0x1531u, 1u, "eth0", NULL };


  mTestProfiles[0] = { "mytest", mMyTestSetupAlsaBothAvbRx, mMyTestSetupAlsaBothAvbTx, mTestConfigAvbVideoRx, mTestConfigAvbVideoTx, mConfigClkRefRx, mConfigClkRefTx, /*mMyTest2chTestSetupJack,*/ mAlsaTestBoth, mTestConfigVideo, mRegTest, NULL };
}
void IasAvbConfigurationBaseImpl::setRxAudioSampleFreq(uint32_t freq)
{
  mMyTestSetupAlsaBothAvbRx->sampleFreq = freq;
}

class IasTestAvbConfigurationBase : public ::testing::Test
{
protected:
  IasTestAvbConfigurationBase()
    : mRegistry()
    , mConfig(NULL)
  {
  }

  virtual ~IasTestAvbConfigurationBase()
  {
  }

  // Sets up the test fixture.
  virtual void SetUp()
  {
    mConfig = new (IasAvbConfigurationBaseImpl);
    dlt_enable_local_print();
  }

  virtual void TearDown()
  {
    delete mConfig;
    mConfig = NULL;
  }

  TestRegistry mRegistry;
  IasAvbConfigurationBaseImpl* mConfig;
};

#if IAS_PREPRODUCTION_SW
TEST_F(IasTestAvbConfigurationBase, setup_hw_cap_positive)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName(),
    "--hwcapture",
    "-q"
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;
  mConfig->mTestProfiles[0].configAvbTx[0].clockId = cIasAvbHwCaptureClockDomainId;
  // 0 != mUseHwC && cIasAvbJackClockDomainId != clockId
  ASSERT_EQ(eIasAvbProcOK, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}
#endif

TEST_F(IasTestAvbConfigurationBase, Testing_OneLineMethods)
{
  const option** optionTable = NULL;
  IasAvbStreamHandlerInterface* api = NULL;

  ASSERT_EQ(IasAvbConfigurationBase::eContinue, theConfigObject.preParseArguments(optionTable));
  ASSERT_EQ(IasAvbConfigurationBase::eContinue, theConfigObject.postParseArguments());
  ASSERT_EQ(IasAvbConfigurationBase::eContinue, theConfigObject.postSetup(api));
  ASSERT_EQ(IasAvbConfigurationBase::eContinue, theConfigObject.preSetup(api));

  ASSERT_FALSE(theConfigObject.passArguments(0, NULL, 0, mRegistry));

  int c = 0;
  uint32_t index = 0;
  ASSERT_EQ(IasAvbConfigurationBase::eError, theConfigObject.handleDerivedOptions(c, index));
}

TEST_F(IasTestAvbConfigurationBase, getNumEntries)
{
  StreamParamsAvbRx      * nullRxStreamParams = NULL;
  StreamParamsAvbTx      * nullTxStreamParams = NULL;
  StreamParamsAvbVideoRx * nullVideoRxStreamParams = NULL;
  StreamParamsAvbVideoTx * nullVideoTxStreamParams = NULL;
  StreamParamsAlsa       * nullAlsaStreamParams = NULL;
  StreamParamsVideo      * nullVideoStreamParams = NULL;
  StreamParamsTestTone   * nullTestToneStreamParams = NULL;
  TestToneParams         * nullTestToneParams = NULL;
  PartitionParams        * nullPartitionParams = NULL;
  RegistryEntries        * nullRegistryEntries = NULL;

  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullRxStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullTxStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullVideoRxStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullVideoTxStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullAlsaStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullVideoStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullTestToneStreamParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullTestToneParams));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullRegistryEntries));
  ASSERT_TRUE(0u == theConfigObject.getNumEntries(nullPartitionParams));
}

TEST_F(IasTestAvbConfigurationBase, getNumEntries_PartitionParams)
{
  PartitionParams pp[] = {
                           { 0, "PortPrefix" },
                           cTerminator_PartitionParams
                         };
  ASSERT_EQ(1,theConfigObject.getNumEntries(pp));
}

TEST_F(IasTestAvbConfigurationBase, setup_null_api)
{
  IasAvbStreamHandlerInterfaceImpl * nullApi = NULL;
  ASSERT_FALSE(mConfig->setup(nullApi));
}


TEST_F(IasTestAvbConfigurationBase, setup_rx_audio_negative)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;
  mConfig->mTestProfiles[0].configAvbRx[0].sampleFreq = 16000u;
  ASSERT_EQ(eIasAvbProcInitializationFailed, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_tx_audio_negative)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;
  mConfig->mTestProfiles[0].configAvbTx[0].sampleFreq = 16000u;
  ASSERT_EQ(eIasAvbProcInitializationFailed, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_tx_video_negative)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;

  mConfig->mTestProfiles[0].configAvbVideoTx[0].format = IasAvbVideoFormat::eIasAvbVideoFormatIec61883;
  ASSERT_EQ(eIasAvbProcInitializationFailed, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_configs_null)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;
  mConfig->mTestProfiles[0].configAvbRx[0]          = cTerminator_StreamParamsAvbRx;
  mConfig->mTestProfiles[0].configAvbTx[0]          = cTerminator_StreamParamsAvbTx;
  mConfig->mTestProfiles[0].configAvbVideoRx        = NULL;
  mConfig->mTestProfiles[0].configAvbVideoTx        = NULL;
  mConfig->mTestProfiles[0].configAlsa              = NULL;
  mConfig->mTestProfiles[0].configVideo             = NULL;
  mConfig->mTestProfiles[0].configAvbClkRefStreamRx = NULL;
  mConfig->mTestProfiles[0].configAvbClkRefStreamTx = NULL;
  // NULL != mAvbClkRefStreamRx...
  ASSERT_EQ(eIasAvbProcOK, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_clk_ref_rx_negative1)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;
  mConfig->mTestProfiles[0].configAvbClkRefStreamRx[0].srClass = 'L';
  // fail on creation
  ASSERT_EQ(eIasAvbProcInitializationFailed, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_srClasses_positive)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
#if IAS_PREPRODUCTION_SW
    ,
    "--nohwcapture",
    "-q"
#endif
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;
  mConfig->mTestProfiles[0].configAvbClkRefStreamTx[0].srClass = 'L';
  mConfig->mTestProfiles[0].configAvbRx[0].srClass = 'L';
  mConfig->mTestProfiles[0].configAvbTx[0].srClass = 'L';
  mConfig->mTestProfiles[0].configAvbVideoRx[0].srClass = 'H';
  mConfig->mTestProfiles[0].configAvbVideoTx[0].srClass = 'H';
  // srClass ternary ops
  ASSERT_EQ(eIasAvbProcOK, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_clk_ref_rx_negative2)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  mConfig->mVerbosity = 3;

  StreamParamsAvbClockReferenceRx * twoClkRefStreamArr = new StreamParamsAvbClockReferenceRx[3];
  twoClkRefStreamArr[0] = { 'H', IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio, (1500u - 20u) / 8u, 0x91e0f000fef91111u, 0x91e0f000fef9u, cRefClockId, 0u, 0u };
  twoClkRefStreamArr[1] = { 'L', IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio, 185u, 0x91e0f000fef91110u, 0x91e0f000fef8u, cRefClockId, 0u, 0u };
  twoClkRefStreamArr[2] = cTerminator_StreamParamsAvbClockReferenceRx;
  mConfig->mTestProfiles[0].configAvbClkRefStreamRx = twoClkRefStreamArr;
  // 1u < mNumAvbClkRefStreamsRx
  ASSERT_EQ(eIasAvbProcInitializationFailed, api->init(theConfigPlugin, true, argc, (char**)args));

  delete api;
  api = NULL;
  delete[] twoClkRefStreamArr;
  mConfig->mTestProfiles[0].configAvbClkRefStreamRx = NULL;
}


TEST_F(IasTestAvbConfigurationBase, setup_positive)
{
  IasAvbStreamHandlerInterfaceImpl * api = new IasAvbStreamHandlerInterfaceImpl();
  optind = 0;

  IasMediaTransportAvb::IasSpringVilleInfo::fetchData();

  const char * customargs[] = {
    "setup",
    "-t", "NGIO",
    "-p", "mytest",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName()
#if IAS_PREPRODUCTION_SW
    ,
    "--nohwcapture"
#endif
  };
  int argcount = static_cast<int>(sizeof customargs / sizeof customargs[0]);

  mConfig->mVerbosity = 1;
  ASSERT_EQ(eIasAvbProcOK, api->init(theConfigPlugin, true, argcount, (char**)customargs));

  delete api;
  api = NULL;
}

TEST_F(IasTestAvbConfigurationBase, setupTestStreams)
{
  IasAvbStreamHandlerInterface * nullApi = NULL;
  mConfig->mVerbosity = 1;

  ASSERT_FALSE(mConfig->setupTestStreams(nullApi));
}

TEST_F(IasTestAvbConfigurationBase, handleProfileOption)
{
  ProfileParams * oldProfile = mConfig->mTestProfiles;
  mConfig->mTestProfiles[0] = *mConfig->mNullCfgRegProfile;
  std::string nullCfgRegProfileName = "Null_cfg_reg";
  ASSERT_EQ(IasAvbConfigurationBase::ContinueStatus::eContinue,
      mConfig->handleProfileOption(nullCfgRegProfileName));

  mConfig->mTestProfiles = oldProfile;
}

TEST_F(IasTestAvbConfigurationBase, handleTargetOption)
{
  std::string nullCfgRegTargetName = "NGIO";

  ASSERT_EQ(IasAvbConfigurationBase::ContinueStatus::eError,
      mConfig->handleTargetOption(nullCfgRegTargetName));

  ASSERT_FALSE(mConfig->passArguments(0, NULL, 0, mRegistry));

  ASSERT_EQ(IasAvbConfigurationBase::ContinueStatus::eContinue,
      mConfig->handleTargetOption(nullCfgRegTargetName));
}


TEST_F(IasTestAvbConfigurationBase, passArguments)
{
  RegistryEntries emptyKeyRegEntries[] =
  {
      { "", false, 1u, "testValue" },
      { "", true, 1u, "testValue" },
      cTerminator_RegistryEntries
  };
  RegistryEntries * oldReg = mConfig->mTestProfiles[0].configReg;
  mConfig->mTestProfiles[0].configReg = emptyKeyRegEntries;
  const char* args[] = {};
  optind = 0;

  ASSERT_FALSE(mConfig->passArguments(0, (char**)args, 0, mRegistry));

  mConfig->mTestProfiles[0].configReg = oldReg;
}

TEST_F(IasTestAvbConfigurationBase, passArguments_additional)
{
  optind = 0;

  const char* args[] = {
      "setup",
      "-p", "mytest",
      "-x", "1",
      "-a", "deprecated",
      "-c", "0",
      "-o", "0"
      "-i", "0",
      "-m", "0xFFFFFFFFFFFFu",
      "-n", "testName",
      "-e", "0",
      "-k", "opt=2",
      "-q"
  };
  optind = 0;

  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  ASSERT_FALSE(mConfig->passArguments(argc, (char**)args, 1, mRegistry));
  optind = 0;
}

TEST_F(IasTestAvbConfigurationBase, passArguments_noVerbose)
{
  optind = 0;

  const char* args[] = {
      "setup",
      "-p", "mytest",
      "-x", "1",
      "-X", "1",
      "-a", "deprecated",
      "-c", "channels",
      "-o", "isTx",
      "-i", "0",
      "-m", "0xFFFFFFFFFFFFu",
      "-n", "testName",
      "-e", "0",
      "-k", "opt=2",
      "-q"
  };
  optind = 0;

  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  ASSERT_FALSE(mConfig->passArguments(argc, (char**)args, 0, mRegistry));
  optind = 0;
}

TEST_F(IasTestAvbConfigurationBase, passArguments_getHexValue_verbose)
{
  optind = 0;

  const char* args[] = {
      "setup",
      "-p", "mytest",
      "-x", "1",
      "-a", "deprecated",
      "-c", "0",
      "-o", "0"
      "-i", "0",
      "-m", "281474976710657",
      "-n", "testName",
      "-e", "0",
      "-k", "opt=2",
      "-q"
  };

  optind = 0;

  int argc = static_cast<int>(sizeof args / sizeof args[0]);
  ASSERT_FALSE(mConfig->passArguments(argc, (char**)args, 1, mRegistry));
  optind = 0;
}

TEST_F(IasTestAvbConfigurationBase, passArguments_getHexValue_noVerbose)
{
  optind = 0;

  const char* args[] = {
      "setup",
      "-p", "mytest",
      "-t", "NGIO",
      "-m", "281474976710657",
  };

  optind = 0;

  int argc = static_cast<int>(sizeof args / sizeof args[0]);
  mConfig->passArguments(argc, (char**)args, 0, mRegistry);
  optind = 0;
}

TEST_F(IasTestAvbConfigurationBase, passArguments_missing_opts)
{
  optind = 0;
  const char *args[] = {
    "setup",
    "-t", "NGIO",
    "-t", "NGIO",
    "-p", "mytest",
    "-p", "mytest",
    "-s", "somesystem",
    "-X", "1",
    "-l", "2",
    "-n", IasMediaTransportAvb::IasSpringVilleInfo::getInterfaceName(),
    "-k", "someopt="
  };
  int argc = static_cast<int>(sizeof args / sizeof args[0]);

  ASSERT_FALSE(mConfig->passArguments(argc, (char**)args, 1, mRegistry));

  optind = 0;
  const char* helpargs[] = {
      "setup",
      "-h"
  };

  ASSERT_FALSE(mConfig->passArguments(2, (char**)helpargs, 1, mRegistry));

  optind = 0;
  const char* otherargs[] = {
      "setup",
      "-?"
  };

  ASSERT_FALSE(mConfig->passArguments(2, (char**)otherargs, 1, mRegistry));

}

TEST_F(IasTestAvbConfigurationBase, getProfileInfo)
{
  mConfig->mNullCfgRegProfile->configAvbRx = NULL;
  mConfig->mNullCfgRegProfile->configAvbTx = NULL;
  mConfig->getProfileInfo(*mConfig->mNullCfgRegProfile);
  ASSERT_TRUE(1);

  mConfig->mTestProfiles[0].configAvbRx->sampleFreq         = 0u;
  mConfig->mTestProfiles[0].configAvbTx->sampleFreq         = 0u;
  mConfig->mTestProfiles[0].configAvbVideoRx->maxPacketRate = 0u;
  mConfig->mTestProfiles[0].configAvbVideoTx->maxPacketRate = 0u;
  mConfig->mTestProfiles[0].configVideo->maxPacketRate      = 0u;
  mConfig->mTestProfiles[0].configAlsa->numChannels         = 0u;
  StreamParamsTestTone configTestTone[] = { {0u, 0u, (IasAvbAudioFormat)0, 0u, 0u, NULL} };
  mConfig->mTestProfiles[0].configTestTone = configTestTone;
  mConfig->getProfileInfo(mConfig->mTestProfiles[0]);
  ASSERT_TRUE(1);
}

TEST_F(IasTestAvbConfigurationBase, setRegistryValues)
{
  ASSERT_TRUE(NULL == mConfig->mRegistry);
  ASSERT_FALSE(mConfig->setRegistryValues(mConfig->mRegTest));

  RegistryEntries * nullEntries = NULL;

  // NULL == regValues
  ASSERT_FALSE(mConfig->setRegistryValues(nullEntries));
}
} /* IasMediaTransportAvb */
