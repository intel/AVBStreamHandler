/*
 * Copyright (C) 2018 Intel Corporation.All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file
 * @brief   Simple example configuration plugin for AVB Streamhandler.
 * @date    2016
 */

#include "media_transport/avb_configuration/IasAvbConfigurationBase.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbConfigRegistryInterface.hpp"

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) ((sizeof a)/(sizeof a[0]))
#endif

using namespace IasMediaTransportAvb;

namespace
{

/*
 * PLEASE READ:
 *
 * This file is an example configuration that can be used as a starting point
 * for a project-specific configuration plugin. Please make sure you have also
 * read the Media Transport Interface Interface Component Documentation (ICD)
 * for AVB Strean Handler.
 *
 * It is recommended to browse through this file from the end backwards, as
 * the order of defining the necessary entities is reverse to the hierarchy of
 * the configuration tree - i.e. the root is the configuration object at the end
 * of the file.
 * When creating a configuration, the definition of the configuration object is
 * the first step, then the profiles and targets are defined, and then the specific
 * tables for items such as AVB streams, ALSA devices etc. are defined.
 * In order to avoid lots of forward references in the code, this needs to be
 * done "bottom-up".
 *
 * Please note that all items listed here are pre-created during the initialization
 * time of the Stream Handler, before it actually starts its real-time operation.
 * Any items created this way have the "pre-created" flag set when queried using the
 * GetAvbStreamInfo call on the run-time API.
 *
 * Some of the items (in particular, clock reference streams) can only be pre-created
 * through configuration, and not through the run-time API later.
 */







/*
 * If operating in clock slave mode, i.e. the audio clock being driven by a stream
 * received via AVB (a clock reference stream, or an audio stream), we need a clock
 * ID that can be referenced by the API at run time, e.g. when creating other AVB
 * streams and/or ALSA interfaces that shall run synchronized to this received clock.
 * The ID value is arbitrary, besides it should not be in the range below 0x80000000 to
 * avoid collision with predefined values.
 */
static const uint32_t cRefClockId = 0x80864711u;
static const uint32_t cMasterClockId = cIasAvbPtpClockDomainId;



//------------------------------------------
//
// Item tables shared by both example profiles
//
//------------------------------------------

// for an explanation of the table columns, please refer to the createLocalVideoStream API documentation
StreamParamsVideo ExampleSetupVideo[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.1", 501u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb_streaming.2", 502u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.3", 503u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb_streaming.4", 504u },
    cTerminator_StreamParamsVideo
};


// for an explanation of the table columns, please refer to the setTestToneParams API documentation
TestToneParams ExampleTestToneParam1[] =
{
    // stereo test tone with one sine wave at 1kHz/-20dB and one rising sawtooth at 500Hz/-30dB
    { 0x8001u, 0u, 1000u, -20, IasAvbTestToneMode::eIasAvbTestToneSine, 0 }, // left
    { 0x8001u, 1u, 500u,  -30, IasAvbTestToneMode::eIasAvbTestToneSawtooth,                 1 }, // right
    cTerminator_TestToneParams
};

// for an explanation of the table columns, please refer to the setTestToneParams API documentation
TestToneParams ExampleTestToneParam2[] =
  {
    // 7.1 test tone
    { 0x8002u, 0u, 750u,  -20, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // front left
    { 0x8002u, 1u, 1250u, -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // front right
    { 0x8002u, 2u, 62u,   -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // LFE
    { 0x8002u, 3u, 1000u, -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // center
    { 0x8002u, 4u, 625u,  -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // surround left
    { 0x8002u, 5u, 1500u, -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // surround right
    { 0x8002u, 6u, 500u,  -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // rear surround left
    { 0x8002u, 7u, 2000u, -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // rear surround right
    cTerminator_TestToneParams
};

// for an explanation of the table columns, please refer to the createTestToneStream API documentation
StreamParamsTestTone ExampleTestTones[] =
{
    { 2u,  48000u, IasAvbAudioFormat::eIasAvbAudioFormatSaf16, 0x00u, 0x8000u, ExampleTestToneParam1 },
    { 8u,  48000u, IasAvbAudioFormat::eIasAvbAudioFormatSaf16, 0x13u, 0x8001u, ExampleTestToneParam2 },
    cTerminator_StreamParamsTestTone
};



//------------------------------------------
//
// Item tables specific to "Master" example profile
//
//------------------------------------------

// for an explanation of the table columns, please refer to the createReceiveAudioStream API documentation
StreamParamsAvbRx MasterSetupAvbAudioRx[] =
{
    { 'H', 2u, 48000u, 0x91E0F000FE050000u, 0x91E0F000FE05u, 5u, 0u, 0u },
    { 'L', 2u, 48000u, 0x91E0F000FE060000u, 0x91E0F000FE06u, 6u, 0u, 0u },
    { 'L', 6u, 48000u, 0x91E0F000FE070000u, 0x91E0F000FE07u, 7u, 0u, 0u },
    { 'L', 6u, 48000u, 0x91E0F000FE080000u, 0x91E0F000FE08u, 8u, 0u, 0u },
    cTerminator_StreamParamsAvbRx
};

// for an explanation of the table columns, please refer to the createTransmitAudioStream API documentation
StreamParamsAvbTx MasterSetupAvbAudioTx[] =
{
    { 'H', 2u, 48000u, cMasterClockId, 0x91E0F000FE010000u, 0x91E0F000FE01u, 1u, false },
    { 'L', 2u, 48000u, cMasterClockId, 0x91E0F000FE020000u, 0x91E0F000FE02u, 2u, true },
    { 'L', 6u, 48000u, cMasterClockId, 0x91E0F000FE030000u, 0x91E0F000FE03u, 3u, true },
    { 'L', 6u, 48000u, cMasterClockId, 0x91E0F000FE040000u, 0x91E0F000FE04u, 4u, true },
    cTerminator_StreamParamsAvbTx
};

// for an explanation of the table columns, please refer to the createReceiveVideoStream API documentation
StreamParamsAvbVideoRx MasterSetupAvbVideoRx[] =
{
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp,      0x91E0F000FE000023u, 0x91E0F000FE23u, 503u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F000FE000024u, 0x91E0F000FE24u, 504u },
   cTerminator_StreamParamsAvbVideoRx
};

// for an explanation of the table columns, please refer to the createTransmitVideoStream API documentation
StreamParamsAvbVideoTx MasterSetupAvbVideoTx[] =
{
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp,      cMasterClockId, 0x91E0F000FE000021u, 0x91E0F000FE21u, 501u, true },
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cMasterClockId, 0x91E0F000FE000022u, 0x91E0F000FE22u, 502u, true },
    cTerminator_StreamParamsAvbVideoTx
};

// for an explanation of the table columns, please refer to the createTransmitClockReferenceStream API documentation
StreamParamsAvbClockReferenceTx MasterSetupAvbCrfTx[] =
{
   // we want to send 50 PDUs per second, and six stamps per PDU, see IEEE1722rev1/D14 Table 28 (p122)
   { 'L', 6u, 48000u / (50u * 6u), 48000u, IasAvbClockMultiplier::eIasAvbCrsMultFlat, cMasterClockId, IasAvbIdAssignMode::eIasAvbIdAssignModeStatic, 0x91E0F000FE910000u, 0x91E0F000FE91u, true },
   cTerminator_StreamParamsAvbClockReferenceTx
};

// for an explanation of the table columns, please refer to the createAlsaStream API documentation
StreamParamsAlsa MasterSetupAlsa[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "stereo_0", 1u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cMasterClockId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};




//------------------------------------------
//
// Item tables specific to "Slave" example profile
//
//------------------------------------------

// for an explanation of the table columns, please refer to the createReceiveAudioStream API documentation
StreamParamsAvbRx SlaveSetupAvbAudioRx[] =
{
   { 'H', 2u, 48000u, 0x91E0F000FE010000u, 0x91E0F000FE01u, 5u, 0u, 0u },
   { 'L', 2u, 48000u, 0x91E0F000FE020000u, 0x91E0F000FE02u, 6u, 0u, 0u },
   { 'L', 6u, 48000u, 0x91E0F000FE030000u, 0x91E0F000FE03u, 7u, 0u, 0u },
   { 'L', 6u, 48000u, 0x91E0F000FE040000u, 0x91E0F000FE04u, 8u, 0u, 0u },
    cTerminator_StreamParamsAvbRx
};

// for an explanation of the table columns, please refer to the createTransmitAudioStream API documentation
StreamParamsAvbTx SlaveSetupAvbAudioTx[] =
{
    { 'H', 2u, 48000u, cRefClockId, 0x91E0F000FE050000u, 0x91E0F000FE05u, 1u, false },
    { 'L', 2u, 48000u, cRefClockId, 0x91E0F000FE060000u, 0x91E0F000FE06u, 2u, true },
    { 'L', 6u, 48000u, cRefClockId, 0x91E0F000FE070000u, 0x91E0F000FE07u, 3u, true },
    { 'L', 6u, 48000u, cRefClockId, 0x91E0F000FE080000u, 0x91E0F000FE08u, 4u, true },
    cTerminator_StreamParamsAvbTx
};

// for an explanation of the table columns, please refer to the createReceiveVideoStream API documentation
StreamParamsAvbVideoRx SlaveSetupAvbVideoRx[] =
{
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F000FE000021u, 0x91E0F000FE21u, 21u },
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F000FE000022u, 0x91E0F000FE22u, 22u },
    cTerminator_StreamParamsAvbVideoRx
};

// for an explanation of the table columns, please refer to the createTransmitVideoStream API documentation
StreamParamsAvbVideoTx SlaveSetupAvbVideoTx[] =
{
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cMasterClockId, 0x91E0F000FE000023u, 0x91E0F000FE23u, 23u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cMasterClockId, 0x91E0F000FE000024u, 0x91E0F000FE24u, 24u, true },
    cTerminator_StreamParamsAvbVideoTx
};


// for an explanation of the table columns, please refer to the createReceiveClockReferenceStream API documentation
StreamParamsAvbClockReferenceRx SlaveSetupAvbCrfRx[] =
{
    { 'L', IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio, (1500u - 20u) / 8u, 0x91E0F000FE000091u, 0x91E0F000FE91u, cRefClockId, 0u, 0u },
    cTerminator_StreamParamsAvbClockReferenceRx
};

// for an explanation of the table columns, please refer to the createAlsaStream API documentation
// the table is identical with the "master" table except for the clock ID
StreamParamsAlsa SlaveSetupAlsa[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_0", 1u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};


//------------------------------------------
//
// Registry Entries
//
//------------------------------------------
/*
 * The Streamhandler uses a "registry" data base of key/value pairs. Values can be either 64 bit
 * unsigned integer or string values. The entries are used to parameterize the Streamhandler's
 * behavior in multiple ways. A list of user-relevant parameters can be found in
 * IasAvbRegistryKeys.hpp. Registry entry lists can be associated with profiles and/or targets.
 *
 * Depending on the isNum field, the registry entry is either numerical or textual. Either the
 * numValue or the textValue field is ignored, depending on the isNum field.
 */


RegistryEntries ExampleSetupRegistry[] =
{
    // define the "low" class according to "class C"
    { "tspec.interval.low", true, 1333000u, NULL }, // class C measurement interval (1.333ms)
    { "tspec.vlanid.low", true, 2u, NULL },         // VLAN ID
    { "tspec.vlanprio.low", true, 3u, NULL },       // VLAN priority (PCP)
    { "tspec.presentation.time.offset.low", true, 5000000u, NULL }, // 5ms instead of the standard class C 15ms

    // set bandwidth limit for all active class C streams to 50Mbit/s
    { "tx.maxbandwidth.low", true, 50000u, NULL },

    // set RX engine to wait up to 50ms for new packets to avoid premature timeout for video-only scenarios
    { "receive.idlewait", true, 50000000u, NULL },
    cTerminator_RegistryEntries
};


//------------------------------------------
//
// Target Entries
//
//------------------------------------------
/*
 * List of supported "targets", i.e. hardware platforms. Specific parameters are PCI device ID,
 * PCI bus ID, network interface name and potential registry entries that shall be added for
 * the given platform.
 * The target to be used must be specified on the command line using the -t or --target option.
 */

TargetParams Targets[] =
{
    { "GrMrb",    0x1533u, 2u, "eth0", NULL }, // example entry for Gordon Ridge MRB
    { "MyDevice", 0x157Cu, 1u, "eth0", NULL }  // example entry for user hardware
};

//------------------------------------------
//
// Profiles
//
//------------------------------------------

/*
 * List of profiles.
 * The config plugin can contain an arbitrary number of different profiles, so the same plugin
 * can be used in multiple configurations of the project (e.g. no external amplifier, external
 * amplifier with stereo sound, ext. amp with multichannel sound, etc.) or on different ECUs
 * in the same system (e.g. head unit vs. rear-seat unit)
 * The actual profile is selected through the -p or --profile parameter on the stream handler
 * command line.
 *
 * Pointers to the individual item tables can be NULL to indicate that this kind of item is
 * not created through the configuration.
 *
 * The following table contains two example configurations, one for a "master" device (i.e.
 * a device running independently), and one for a "slave" device (i.e. a device being clocked
 * from an incoming AVB clock reference stream). Note that some of the item tables are shared by
 * both profiles.
 */
ProfileParams Profiles[] =
{
    // example 1: A Master configuration
    {
        "MasterExample",              // profile name as specified with -p on the command line
        MasterSetupAvbAudioRx,        // table with AVB audio receive streams to be created
        MasterSetupAvbAudioTx,        // table with AVB audio transmit streams to be created
        MasterSetupAvbVideoRx,        // table with AVB video receive streams to be created
        MasterSetupAvbVideoTx,        // table with AVB video transmit streams to be created
        NULL,                         // table with AVB CRF receive streams to be created (none for master)
        MasterSetupAvbCrfTx,          // table with AVB CRF receive streams to be created
        MasterSetupAlsa,              // table with ALSA devices to be created
        ExampleSetupVideo,            // table with local video streaming interfaces to be created
        ExampleSetupRegistry,         // list of entries to be added to the configuration registry
        ExampleTestTones              // list of test tone generators that can be connected to AVB streams instead of ALSA devices
    },
    // example 2: A Slave configuration
    {
        "SlaveExample",              // profile name as specified with -p on the command line
        SlaveSetupAvbAudioRx,         // table with AVB audio receive streams to be created
        SlaveSetupAvbAudioTx,         // table with AVB audio transmit streams to be created
        SlaveSetupAvbVideoRx,         // table with AVB video receive streams to be created
        SlaveSetupAvbVideoTx,         // table with AVB video transmit streams to be created
        SlaveSetupAvbCrfRx,           // table with AVB CRF receive streams to be created
        NULL,                         // table with AVB CRF receive streams to be created (none for slave)
        SlaveSetupAlsa,               // table with ALSA devices to be created
        ExampleSetupVideo,            // table with local video streaming interfaces to be created
        ExampleSetupRegistry,         // list of entries to be added to the configuration registry
        ExampleTestTones              // list of test tone generators that can be connected to AVB streams instead of ALSA devices
    }
};




//------------------------------------------
//
// The configuration object
//
//------------------------------------------
/*
 * This is our configuration object. It registers with the streamhandler automatically through
 * the base class's constructor. This is boilerplate code that typically does not change for
 * a specific configuration.
 *
 * Note that, in theory, you could override the methods of the IasAvbConfigurationBase class
 * and implement additional ways to modify the configuration, such as additional command line
 * arguments, parsing of XML configuration files, etc. For more information, see the definition
 * of the IasAvbConfigurationBase class in IasAvbConfigurationBase.hpp.
 */
class IasAvbConfigurationReference : public IasAvbConfigurationBase
{
  public:
    IasAvbConfigurationReference() {}

    virtual ~IasAvbConfigurationReference() {}

  private:

    virtual uint32_t getTargets(TargetParams *& targets)
    {
      targets = Targets;
      return static_cast<uint32_t>(ARRAY_LEN(Targets));
    }

    virtual uint32_t getProfiles(ProfileParams *& profiles)
    {
      profiles = Profiles;
      return static_cast<uint32_t>(ARRAY_LEN(Profiles));
    }
} configObject;

} // end anonymous namespace
