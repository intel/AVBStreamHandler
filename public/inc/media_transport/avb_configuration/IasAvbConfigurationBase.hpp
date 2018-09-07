/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbDefaultConfiguration.hpp
 * @brief   Example implementation of callback interface to configure StreamHandler.
 * @date    2013
 */


#ifndef IAS_MEDIATRANSPORT_AVBCONFIGURATION_IASAVBCONFIGURATIONBASE_HPP
#define IAS_MEDIATRANSPORT_AVBCONFIGURATION_IASAVBCONFIGURATIONBASE_HPP

#include "media_transport/avb_streamhandler_api/IasAvbConfiguratorInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerInterface.hpp"
#include <sstream>
#include <iostream>
#include <getopt.h>


namespace IasMediaTransportAvb
{
  class IasAvbConfigRegistryInterface;
}


using IasMediaTransportAvb::IasAvbConfigRegistryInterface;
using IasMediaTransportAvb::IasAvbStreamHandlerInterface;
using IasMediaTransportAvb::IasAvbStreamDirection;
using IasMediaTransportAvb::IasAvbConfiguratorInterface;
using IasMediaTransportAvb::IasAvbAudioFormat;
using IasMediaTransportAvb::IasAvbTestToneMode;

namespace IasMediaTransportAvb
{

struct RegistryEntries
{
    const char* keyName;
    bool isNum;
    uint64_t numValue;
    const char* textValue;

    bool isValid() const {return keyName != NULL;}
};

static const RegistryEntries cTerminator_RegistryEntries __attribute__((used)) = { NULL, false, 0u, NULL };

struct PartitionParams
{
    uint16_t channelIdx;
    const char *portPrefix;

    bool isValid() const {return portPrefix != NULL;}
};

static const PartitionParams cTerminator_PartitionParams __attribute__((used)) = { 0u, NULL };

struct StreamParamsAvbRx
{
    char srClass;
    uint16_t maxNumChannels;
    uint32_t sampleFreq;
    uint64_t streamId;
    uint64_t dMac;
    uint16_t localStreamdIDToConnect;
    uint32_t slaveClockId; // <<optional>>;
    uint32_t clockDriverId; // <<optional>>;

    bool isValid() const {return sampleFreq != 0;}
};

static const StreamParamsAvbRx cTerminator_StreamParamsAvbRx __attribute__((used)) = { 'H', 0u, 0u, 0u, 0u, 0u, 0u, 0u };

struct StreamParamsAvbTx
{
    char srClass;
    uint16_t maxNumChannels;
    uint32_t sampleFreq;
    uint32_t clockId;
    uint64_t streamId;
    uint64_t dMac;
    uint16_t localStreamdIDToConnect;
    bool activate;

    bool isValid() const {return sampleFreq != 0;}
};

static const StreamParamsAvbTx cTerminator_StreamParamsAvbTx __attribute__((used)) = { 'H', 0u, 0u, 0u, 0u, 0u, 0u, false };

struct StreamParamsAvbVideoRx
{
    char srClass;
    uint16_t maxPacketRate;
    uint32_t maxPacketSize;
    IasAvbVideoFormat format;
    uint64_t streamId;
    uint64_t dMac;
    uint16_t localStreamdIDToConnect;

    bool isValid() const {return maxPacketRate != 0;}
};

static const StreamParamsAvbVideoRx cTerminator_StreamParamsAvbVideoRx __attribute__((used)) = { 'L', 0u, 0u, IasAvbVideoFormat(), 0u, 0u, 0u };

struct StreamParamsAvbVideoTx
{
    char srClass;
    uint16_t maxPacketRate;
    uint32_t maxPacketSize;
    IasAvbVideoFormat format;
    uint32_t clockId;
    uint64_t streamId;
    uint64_t dMac;
    uint16_t localStreamdIDToConnect;
    bool activate;

    bool isValid() const {return maxPacketRate != 0;}
};

static const StreamParamsAvbVideoTx cTerminator_StreamParamsAvbVideoTx __attribute__((used)) = { 'L', 0u, 0u, IasAvbVideoFormat(), 0u, 0u, 0u, 0u, 0u };

struct StreamParamsAvbClockReferenceRx
{
    char srClass;
    IasAvbClockReferenceStreamType type;
    uint16_t maxCrfStampsPerPdu;
    uint64_t streamId;
    uint64_t dMac;
    uint32_t clockId;
    uint32_t slaveClockId; // <<optional>>;
    uint32_t clockDriverId; // <<optional>>;

    bool isValid() const {return maxCrfStampsPerPdu != 0;}
};

static const StreamParamsAvbClockReferenceRx cTerminator_StreamParamsAvbClockReferenceRx __attribute__((used)) = { 'H', IasAvbClockReferenceStreamType(), 0u, 0u, 0u, 0u, 0u, 0u };

struct StreamParamsAvbClockReferenceTx
{
    char srClass;
    uint16_t crfStampsPerPdu;
    uint16_t crfStampInterval;
    uint32_t baseFreq;
    IasAvbClockMultiplier pull;
    uint32_t clockId;
    IasAvbIdAssignMode assignMode;
    uint64_t streamId;
    uint64_t dMac;
    bool activate;

    bool isValid() const {return crfStampsPerPdu != 0;}
};

static const StreamParamsAvbClockReferenceTx cTerminator_StreamParamsAvbClockReferenceTx __attribute__((used)) = { 'H', 0u, 0u, 0u, IasAvbClockMultiplier(), 0u, IasAvbIdAssignMode(), 0u, 0u, false };

struct StreamParamsAlsa
{
    IasAvbStreamDirection streamDirection;
    uint16_t numChannels;
    uint32_t sampleFreq;
    uint32_t clockId;
    uint32_t periodSize;
    uint32_t numPeriods;
    uint8_t layout;
    bool hasSideChannel;
    std::string deviceName;
    uint16_t streamId;
    IasAlsaDeviceTypes alsaDeviceType;
    uint32_t sampleFreqASRC;

    bool isValid() const {return numChannels != 0;}
};

static const StreamParamsAlsa cTerminator_StreamParamsAlsa __attribute__((used)) = { IasAvbStreamDirection(), 0u, 0u, 0u, 0u, 0u, 0u, false, "", 0u, IasAlsaDeviceTypes(), 0u };

struct StreamParamsVideo
{
    IasAvbStreamDirection streamDirection;
    uint16_t maxPacketRate;
    uint16_t maxPacketSize;
    IasAvbVideoFormat format;
    const char *ipcName;
    uint16_t streamId;

    bool isValid() const {return maxPacketRate != 0;}
};

static const StreamParamsVideo cTerminator_StreamParamsVideo __attribute__((used)) = { IasAvbStreamDirection(), 0u, 0u, IasAvbVideoFormat(), NULL, 0u };

struct TestToneParams
{
    uint16_t localStreamId;
    uint16_t channel;
    uint32_t signalFrequency;
    int32_t level;
    IasAvbTestToneMode mode;
    int32_t userParam;

    bool isValid() const {return signalFrequency != 0;}
};

static const TestToneParams cTerminator_TestToneParams __attribute__((used)) = { 0u, 0u, 0u, 0u, IasAvbTestToneMode(), 0u };

struct StreamParamsTestTone
{
    uint16_t numberOfChannels;
    uint32_t sampleFreq;
    IasAvbAudioFormat format;
    uint8_t channelLayout;
    uint16_t streamId;
    TestToneParams *toneParams;

    bool isValid() const {return toneParams != NULL;}
};

static const StreamParamsTestTone cTerminator_StreamParamsTestTone __attribute__((used)) = { 0u, 0u, IasAvbAudioFormat(), 0u, 0u, NULL };

struct ProfileParams
{
    const char *profileName;
    StreamParamsAvbRx *configAvbRx;
    StreamParamsAvbTx *configAvbTx;
    StreamParamsAvbVideoRx *configAvbVideoRx;
    StreamParamsAvbVideoTx *configAvbVideoTx;
    StreamParamsAvbClockReferenceRx *configAvbClkRefStreamRx;
    StreamParamsAvbClockReferenceTx *configAvbClkRefStreamTx;
    StreamParamsAlsa *configAlsa;
    StreamParamsVideo *configVideo;
    RegistryEntries *configReg;
    StreamParamsTestTone *configTestTone;
};

struct TargetParams
{
    const char *targetName;
    uint16_t deviceId;
    uint8_t bus;
    const char *ifName;
    RegistryEntries *configReg;
};

class IasAvbConfigurationBase : public IasAvbConfiguratorInterface
{
  public:
    IasAvbConfigurationBase();
    virtual ~IasAvbConfigurationBase() {}

    /**
     * @brief the one and only method that needs to be exported by the shared library object
     *
     * @return reference to the configurator object
     */
    static IasAvbConfiguratorInterface & getInstance();

    static StreamParamsAvbRx  DefaultSetupAvbRx[];
    static StreamParamsAvbTx  DefaultSetupAvbTx[];

  protected:

    enum ContinueStatus
    {
      eContinue,    ///< continue with the regular process as implemented in base class
      eError,       ///< abort stream handler initialization
      eHandled      ///< continue, but skip the remaining steps in the base class's method implementation
    };

    //{@ IasAvbConfiguratorInterface implementation
    virtual bool passArguments(int32_t argc, char** argv, int32_t verbosity, IasAvbConfigRegistryInterface & registry);
    virtual bool setup(IasAvbStreamHandlerInterface* api);
    //@}

    //
    // interactions with derived class (overridables)
    //

    /**
     * @brief hand over the target parameter to the base class
     *
     * This override is called by the base class to get all information about the selectable targets. This
     * information is needed to setup the streamhandler with special target parameter.
     *
     * @param targets reference pointer to the targets table.
     * @return number of available targets.
     */
    virtual uint32_t getTargets(TargetParams *& targets) = 0;

    /**
     * @brief hand over the profile parameter to the base class
     *
     * This override is called by the base class to get all information about the selectable profiles. This
     * information is needed to setup the streamhandler with special profile parameter.
     *
     * @param profiles reference pointer to the profiles table.
     * @return number of available profiles.
     */
    virtual uint32_t getProfiles(ProfileParams *& profiles) = 0;
    //@}

    //{@ optional overrides
    /**
     * @brief handle all arguments not handled by the base class
     *
     * The default implementation just prints out a message and returns eError,
     * since the base class obviously doesn't know the option. Override this if
     * you have extended the option table by custom options (see @ref preParseArguments)
     *
     * The index parameter is useful when an option is supposed to manipulate an entry
     * in a row of a table (e.g. change streamId).
     *
     * @param c option code as defined in the options table
     * @param index current index value
     * @return ContinueStatus as defined above. eHandled and eContinue are not handled differently.
     */
    virtual ContinueStatus handleDerivedOptions(int32_t c, uint32_t index);

    /**
     * @brief accomplish things to be done before parsing the command line arguments
     *
     * Use this override to set default values and/or registry entries. Also, the default
     * option table can be replaced, typically with a reduced or extended version. You must ensure
     * that *optionTable points to a valid table and the table must be terminated with an entry
     * having NULL in the name field!
     *
     * @param optionTable pointer to base address of optionTable
     * @return ContinueStatus as defined above.
     */
    virtual ContinueStatus preParseArguments(const struct option ** optionTable) { (void) optionTable; return eContinue; }

    /**
     * @brief accomplish things to be done after parsing the command line arguments is finished
     *
     * Use this override to set configurations and/or registry entries, depending on the arguments
     * encountered before.
     *
     * @return ContinueStatus as defined above. eHandled and eContinue are not handled differently.
     */
    virtual ContinueStatus postParseArguments() { return eContinue; }

    /**
     * @brief control execution of setup method
     *
     * Control whether the default setup () should be executed
     * @return eError do not continue and abort stream handler initialization
     * @return eContinue continue with the normal setup process
     * @return eHandled no need to run the normal procedure, handled by derived class
     */
    virtual ContinueStatus preSetup(IasAvbStreamHandlerInterface *api) { (void) api; return eContinue; }

    /**
     * @brief accomplish things to be done after executing the normal setup process
     *
     * Typically, this override will partition streams and/or define clock recovery on RX streams.
     * If eContinue is returned, the base class will conclude the setup().
     * If eHandled is returned, the base class will exit the setup() method directly.
     *
     * @return ContinueStatus as defined above.
     */
    virtual ContinueStatus postSetup(IasAvbStreamHandlerInterface *api) { (void) api; return eContinue; }
    //@}

    //
    // internal helpers
    //

  private:
    template <class T>
    bool getHexVal(T& target, const std::string & name, uint64_t limit = 0u);

    ContinueStatus setupTestStreams(IasAvbStreamHandlerInterface *api);

    template<typename  T>
    uint32_t getNumEntries(const T *params) const;

    void getProfileInfo(const ProfileParams &theProfile);
    bool setRegistryValues(const RegistryEntries* regValues);

    ContinueStatus handleTargetOption(const std::string & targetName);
    ContinueStatus handleProfileOption(const std::string & profileName);


    //
    // attributes
    //

    IasAvbConfigRegistryInterface *mRegistry;
    StreamParamsAvbRx *mAvbStreamsRx;
    StreamParamsAvbTx *mAvbStreamsTx;
    StreamParamsAvbVideoRx *mAvbVideoStreamsRx;
    StreamParamsAvbVideoTx *mAvbVideoStreamsTx;
    StreamParamsAvbClockReferenceRx *mAvbClkRefStreamRx;
    StreamParamsAvbClockReferenceTx *mAvbClkRefStreamTx;
    StreamParamsAlsa *mAlsaStreams;
    StreamParamsVideo *mVideoStreams;
    StreamParamsTestTone *mTestStreams;
    uint32_t mNumAvbStreamsRx;
    uint32_t mNumAvbStreamsTx;
    uint32_t mNumAvbVideoStreamsRx;
    uint32_t mNumAvbVideoStreamsTx;
    uint32_t mNumAvbClkRefStreamsRx;
    uint32_t mNumAvbClkRefStreamsTx;
    uint32_t mNumAlsaStreams;
    uint32_t mNumVideoStreams;
    uint32_t mNumTestStreams;

    bool mUseDefaultChannelLayout;
    bool mUseDefaultDmac;
    int32_t mUseFixedClock;
    int32_t mUseHwC;
    bool mUseClkRec;
    int32_t mVerbosity;
    bool mProfileSet;
    bool mTargetSet;

    static IasAvbConfiguratorInterface *instance;
};

template<typename  T>
uint32_t IasAvbConfigurationBase::getNumEntries(const T * params) const
{
  uint32_t entries = 0u;
  if (NULL != params)
  {
    while (params[entries].isValid())
    {
      entries++;
    }
  }

  return entries;
}

template <class T>
bool IasAvbConfigurationBase::getHexVal(T& target, const std::string & name, uint64_t limit)
{
  bool ret = false;
  uint64_t x;
  std::stringstream(std::string(optarg)) >> std::hex >> x;

  if (0u == limit)
  {
    // set limit to max value of our target data type
    limit = T(-1);
  }

  if (x > limit)
  {
    if (mVerbosity >= 0)
    {
      std::cerr << "AVB_ERR:Invalid " << name << " value: "<< std::showbase << std::hex << x << std::endl;
    }
  }
  else
  {
    if (mVerbosity > 0)
    {
      std::cout << "AVB_LOG:" <<name << " set to " << std::showbase << std::hex << x << std::endl;
    }
    target = T(x);
    ret = true;
  }

  return ret;
}

} // namespace IasMediaTransportConfiguration


#endif // defined IAS_MEDIATRANSPORT_AVBCONFIGURATION_IASAVBCONFIGBASE_HPP

