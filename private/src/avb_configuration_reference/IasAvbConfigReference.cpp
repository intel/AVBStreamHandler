/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 * @brief   Reference configuration for AVB Streamhandler.
 * @date    2016
 */

#include "media_transport/avb_configuration/IasAvbConfigurationBase.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerInterface.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbConfigRegistryInterface.hpp"

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) ((sizeof a)/(sizeof a[0]))
#endif

using namespace IasMediaTransportAvb;

// Clock Reference Streams
static const uint32_t cRefClockId = 0x80864711u;



TestToneParams StandardBrd2MasterToneParam0[] =
{
    // stereo test tone with two sine waves
    { 0x8000u, 0u, 1000u, -20, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // left
    { 0x8000u, 1u, 1500u, -20, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // right
    cTerminator_TestToneParams
};

TestToneParams StandardBrd2MasterToneParam1[] =
{
    // stereo test tone with one sine wave and one rising sawtooth
    { 0x8001u, 0u, 1000u, -20, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // left
    { 0x8001u, 1u, 500u,  -30, IasAvbTestToneMode::eIasAvbTestToneSawtooth, 1 }, // right
    cTerminator_TestToneParams
};

TestToneParams StandardBrd2MasterToneParam2[] =
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

TestToneParams StandardBrd2MasterToneParam3[] =
{
    // 5.1 test tone
    { 0x8003u, 0u, 330u,  -20, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // front left
    { 0x8003u, 1u, 550u,  -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // front right
    { 0x8003u, 2u, 55u,   -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // LFE
    { 0x8003u, 3u, 440u,  -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // center
    { 0x8003u, 4u, 660u,  -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // surround left
    { 0x8003u, 5u, 275u,  -30, IasAvbTestToneMode::eIasAvbTestToneSine,     0 }, // surround right
    cTerminator_TestToneParams
};

StreamParamsTestTone StandardBrd2MasterTestTones[] =
{
    { 2u,  48000u, IasAvbAudioFormat::eIasAvbAudioFormatSafFloat, 0x00u, 0x8000u, StandardBrd2MasterToneParam0 },
    { 2u,  48000u, IasAvbAudioFormat::eIasAvbAudioFormatSafFloat, 0x00u, 0x8001u, StandardBrd2MasterToneParam1 },
    { 8u,  48000u, IasAvbAudioFormat::eIasAvbAudioFormatSafFloat, 0x13u, 0x8002u, StandardBrd2MasterToneParam2 },
    { 6u,  48000u, IasAvbAudioFormat::eIasAvbAudioFormatSafFloat, 0x0Bu, 0x8003u, StandardBrd2MasterToneParam3 },
    cTerminator_StreamParamsTestTone
};



// for Unittest only.
StreamParamsAvbTx Unittest2chSetupAvbTxNC[] =
{
    { 'H', 2u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000001u, 0x91E0F000FE01u, 0u, true },
    { 'H', 2u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000002u, 0x91E0F000FE02u, 0u, true },
    { 'H', 2u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000003u, 0x91E0F000FE03u, 0u, true },
    cTerminator_StreamParamsAvbTx
};

StreamParamsAvbRx Unittest2chSetupAvbRxNC[] =
{
    { 'H', 8u, 48000u, 0x0u, 0x0u, 0u, 0u, 0u },
    cTerminator_StreamParamsAvbRx
};

//Video
StreamParamsAvbVideoTx VideoPocSetupAvbVideoMpegTsMasterTx[] =
{
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cIasAvbPtpClockDomainId, 0x91E0F00007812642u, 0x91E0F0000781u, 501u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cIasAvbPtpClockDomainId, 0x91E0F00007812643u, 0x91E0F0000782u, 502u, true },
    cTerminator_StreamParamsAvbVideoTx
};

StreamParamsAvbVideoTx VideoPocSetupAvbVideoMasterTx[] =
{
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007812642u, 0x91E0F0000781u, 501u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007822643u, 0x91E0F0000782u, 502u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007832644u, 0x91E0F0000783u, 503u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007842645u, 0x91E0F0000784u, 504u, true },
    cTerminator_StreamParamsAvbVideoTx
};

StreamParamsAvbVideoRx VideoPocSetupAvbVideoMpegTsMasterRx[] =
{
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F00007852646u, 0x91E0F0000785u, 507u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F00007852647u, 0x91E0F0000786u, 508u },
   cTerminator_StreamParamsAvbVideoRx
};

StreamParamsAvbVideoRx VideoPocSetupAvbVideoMasterRx[] =
{
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007892640u, 0x91E0F0000789u, 505u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F000078A2641u, 0x91E0F000078Au, 506u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007852646u, 0x91E0F0000785u, 507u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007862647u, 0x91E0F0000786u, 508u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007872648u, 0x91E0F0000787u, 509u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007882649u, 0x91E0F0000788u, 510u },
   cTerminator_StreamParamsAvbVideoRx
};

StreamParamsVideo VideoPocSetupLocalVideoMpegTsMaster[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.1", 501u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.2", 502u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.7", 507u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.8", 508u },
    cTerminator_StreamParamsVideo
};

StreamParamsVideo VideoPocSetupLocalVideoMaster[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.1", 501u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.2", 502u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.3", 503u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.4", 504u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.5", 505u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.6", 506u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.7", 507u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.8", 508u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.9", 509u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.10", 510u },
    cTerminator_StreamParamsVideo
};

StreamParamsAvbVideoTx VideoPocSetupAvbVideoMpegTsSlaveTx[] =
{
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cIasAvbPtpClockDomainId, 0x91E0F00007852646u, 0x91E0F0000785u, 501u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cIasAvbPtpClockDomainId, 0x91E0F00007852647u, 0x91E0F0000786u, 502u, true },
    cTerminator_StreamParamsAvbVideoTx
};

StreamParamsAvbVideoTx VideoPocSetupAvbVideoSlaveTx[] =
{
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007852646u, 0x91E0F0000785u, 501u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007862647u, 0x91E0F0000786u, 502u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007872648u, 0x91E0F0000787u, 503u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F00007882649u, 0x91E0F0000788u, 504u, true },
    cTerminator_StreamParamsAvbVideoTx
};

StreamParamsAvbVideoRx VideoPocSetupAvbVideoMpegTsSlaveRx[] =
{
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F00007812642u, 0x91E0F0000781u, 507u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F00007812643u, 0x91E0F0000782u, 508u },
   cTerminator_StreamParamsAvbVideoRx
};

StreamParamsAvbVideoRx VideoPocSetupAvbVideoSlaveRx[] =
{
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007892640u, 0x91E0F0000789u, 505u }, // LGE CAM
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F000078A2641u, 0x91E0F000078Au, 506u }, // LGE CAM
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007812642u, 0x91E0F0000781u, 507u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007822643u, 0x91E0F0000782u, 508u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007832644u, 0x91E0F0000783u, 509u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F00007842645u, 0x91E0F0000784u, 510u },
   cTerminator_StreamParamsAvbVideoRx
};

StreamParamsVideo VideoPocSetupLocalVideoMpegTsSlave[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.1", 501u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.2", 502u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.7", 507u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb.mpegts_streaming.8", 508u },
    cTerminator_StreamParamsVideo
};

StreamParamsVideo VideoPocSetupLocalVideoSlave[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.1", 501u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.2", 502u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.3", 503u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.4", 504u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.5", 505u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.6", 506u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.7", 507u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.8", 508u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.9", 509u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.10", 510u },
    cTerminator_StreamParamsVideo
};


//------------------------------------------
//
// MRB_Master_Audio
//
//------------------------------------------

StreamParamsAvbRx MrbMasterAvbRx[] =
{
    { 'L', 2u, 48000u, 0x91E0F000FE000005u, 0x91E0F000FE05u, 5u, 0u, 0u },
    { 'L', 2u, 48000u, 0x91E0F000FE000006u, 0x91E0F000FE06u, 6u, 0u, 0u },
    { 'L', 6u, 48000u, 0x91E0F000FE000007u, 0x91E0F000FE07u, 7u, 0u, 0u },
    { 'L', 6u, 48000u, 0x91E0F000FE000008u, 0x91E0F000FE08u, 8u, 0u, 0u },
    cTerminator_StreamParamsAvbRx
};

StreamParamsAvbTx MrbMasterAvbTx[] =
{
    { 'L', 2u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000001u, 0x91E0F000FE01u, 1u, true },
    { 'L', 2u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000002u, 0x91E0F000FE02u, 2u, true },
    { 'L', 6u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000003u, 0x91E0F000FE03u, 3u, true },
    { 'L', 6u, 48000u, cIasAvbPtpClockDomainId, 0x91E0F000FE000004u, 0x91E0F000FE04u, 4u, true },
    cTerminator_StreamParamsAvbTx
};

StreamParamsAvbTx MrbMasterAvbTxRaw[] =
{
    { 'L', 2u, 48000u, cIasAvbRawClockDomainId, 0x91E0F000FE000001u, 0x91E0F000FE01u, 1u, true },
    { 'L', 2u, 48000u, cIasAvbRawClockDomainId, 0x91E0F000FE000002u, 0x91E0F000FE02u, 2u, true },
    { 'L', 6u, 48000u, cIasAvbRawClockDomainId, 0x91E0F000FE000003u, 0x91E0F000FE03u, 3u, true },
    { 'L', 6u, 48000u, cIasAvbRawClockDomainId, 0x91E0F000FE000004u, 0x91E0F000FE04u, 4u, true },
    cTerminator_StreamParamsAvbTx
};

StreamParamsAvbClockReferenceTx MrbMasterClkRefTx[] =
{
   // we want to send 50 PDUs per second, and six stamps per PDU, see IEEE1722rev1/D14 Table 28 (p122)
   { 'L', 6u, 48000u / (50u * 6u), 48000u, IasAvbClockMultiplier::eIasAvbCrsMultFlat, cIasAvbPtpClockDomainId, IasAvbIdAssignMode::eIasAvbIdAssignModeStatic, 0x91E0F000FE000091u, 0x91E0F000FE91u, true },
   cTerminator_StreamParamsAvbClockReferenceTx
};

StreamParamsAvbClockReferenceTx MrbMasterClkRefTxRaw[] =
{
   // we want to send 50 PDUs per second, and six stamps per PDU, see IEEE1722rev1/D14 Table 28 (p122)
   { 'L', 6u, 48000u / (50u * 6u), 48000u, IasAvbClockMultiplier::eIasAvbCrsMultFlat, cIasAvbRawClockDomainId, IasAvbIdAssignMode::eIasAvbIdAssignModeStatic, 0x91E0F000FE000091u, 0x91E0F000FE91u, true },
   cTerminator_StreamParamsAvbClockReferenceTx
};

StreamParamsAlsa MrbMasterAlsa[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_0", 1u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};

StreamParamsAlsa MrbMasterAlsa1[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 8u, 0x00u, false, "hw:0,0",   1u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};

StreamParamsAlsa MrbMasterAlsa2[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_0", 1u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 8u, 0x00u, false, "front",    5u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};


StreamParamsAlsa MrbMasterAlsa3[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "hw:0,0",   1u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 8u, 0x00u, false, "front",    5u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbPtpClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};

StreamParamsAlsa MrbMasterAlsaRaw[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "stereo_0", 1u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cIasAvbRawClockDomainId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};


//------------------------------------------
//
// MRB_Slave_Audio
//
//------------------------------------------

StreamParamsAvbRx MrbSlaveAvbRx[] =
{
   { 'L', 2u, 48000u, 0x91E0F000FE000001u, 0x91E0F000FE01u, 1u, 0u, 0u },
   { 'L', 2u, 48000u, 0x91E0F000FE000002u, 0x91E0F000FE02u, 2u, 0u, 0u },
   { 'L', 6u, 48000u, 0x91E0F000FE000003u, 0x91E0F000FE03u, 3u, 0u, 0u },
   { 'L', 6u, 48000u, 0x91E0F000FE000004u, 0x91E0F000FE04u, 4u, 0u, 0u },
    cTerminator_StreamParamsAvbRx
};

StreamParamsAvbTx MrbSlaveAvbTx[] =
{
    // streams exchanged between master and slave silverbox
    { 'L', 2u, 48000u, cRefClockId, 0x91E0F000FE000005u, 0x91E0F000FE05u, 5u, true },
    { 'L', 2u, 48000u, cRefClockId, 0x91E0F000FE000006u, 0x91E0F000FE06u, 6u, true },
    { 'L', 6u, 48000u, cRefClockId, 0x91E0F000FE000007u, 0x91E0F000FE07u, 7u, true },
    { 'L', 6u, 48000u, cRefClockId, 0x91E0F000FE000008u, 0x91E0F000FE08u, 8u, true },
    cTerminator_StreamParamsAvbTx
};

StreamParamsAvbClockReferenceRx MrbSlaveClkRefRx[] =
{
    { 'L', IasAvbClockReferenceStreamType::eIasAvbCrsTypeAudio, (1500u - 20u) / 8u, 0x91E0F000FE000091u, 0x91E0F000FE91u, cRefClockId, 0u, 0u },
    cTerminator_StreamParamsAvbClockReferenceRx
};

StreamParamsAlsa MrbSlaveAlsa[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_0", 1u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};

StreamParamsAlsa MrbSlaveAlsa1[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_0", 5u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 8u, 0x00u, false, "front",    1u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};

StreamParamsAlsa MrbSlaveAlsa2[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "hw:0,0",   5u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 6u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     7u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,  6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     8u, eIasAlsaVirtualDevice, 48000 },

    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 8u, 0x00u, false, "front",    1u, eIasAlsaHwDevice     , 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 2u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "stereo_1", 2u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_0",     3u, eIasAlsaVirtualDevice, 48000 },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork, 6u, 48000u, cRefClockId, 192u, 3u, 0x00u, false, "mc_1",     4u, eIasAlsaVirtualDevice, 48000 },
    cTerminator_StreamParamsAlsa
};


//------------------------------------------
//
// MRB_Master_AV (Audio and Video)
//
//------------------------------------------

//StreamParamsAvbRx MrbMasterAvbRx[] = already defined in profile 'MRB_Master_Audio'

//StreamParamsAvbTx MrbMasterAvbTx[] = already defined in profile 'MRB_Master_Audio'

StreamParamsAvbVideoRx MrbMasterAvbVideoRx[] =
{
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp,      0x91E0F000FE000023u, 0x91E0F000FE23u, 23u },
   { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F000FE000024u, 0x91E0F000FE24u, 24u },
   cTerminator_StreamParamsAvbVideoRx
};

StreamParamsAvbVideoTx MrbMasterAvbVideoTx[] =
{
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp,      cIasAvbPtpClockDomainId, 0x91E0F000FE000021u, 0x91E0F000FE21u, 21u, true },
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cIasAvbPtpClockDomainId, 0x91E0F000FE000022u, 0x91E0F000FE22u, 22u, true },
    cTerminator_StreamParamsAvbVideoTx
};

//StreamParamsAvbClockReferenceTx MrbMasterClkRefTx[] = already defined in profile 'MRB_Master_Audio'

//StreamParamsAlsa MrbMasterAlsa[] = already defined in profile 'MRB_Master_Audio'

StreamParamsVideo MrbMasterLocalVideo[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.1", 501u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb_streaming.2", 502u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.3", 503u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb_streaming.4", 504u },
    cTerminator_StreamParamsVideo
};


//------------------------------------------
//
// MRB_Slave_AV (Audio and Video)
//
//------------------------------------------

//StreamParamsAvbRx MrbSlaveAvbRx[] = already defined in profile 'MRB_Slave_Audio'

//StreamParamsAvbTx MrbSlaveAvbTx[] = already defined in profile 'MRB_Slave_Audio'

StreamParamsAvbVideoRx MrbSlaveAvbVideoRx[] =
{
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, 0x91E0F000FE000021u, 0x91E0F000FE21u, 21u },
    { 'L', 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, 0x91E0F000FE000022u, 0x91E0F000FE22u, 22u },
    cTerminator_StreamParamsAvbVideoRx
};

StreamParamsAvbVideoTx MrbSlaveAvbVideoTx[] =
{
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatRtp, cIasAvbPtpClockDomainId, 0x91E0F000FE000023u, 0x91E0F000FE23u, 23u, true },
    { 'L', 4000u, 1460, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, cIasAvbPtpClockDomainId, 0x91E0F000FE000024u, 0x91E0F000FE24u, 24u, true },
    cTerminator_StreamParamsAvbVideoTx
};

//StreamParamsAvbClockReferenceRx MrbSlaveClkRefRx[] = already defined in profile 'MRB_Slave_Audio'

//StreamParamsAlsa MrbSlaveAlsa[] = already defined in profile 'MRB_Slave_Audio'

StreamParamsVideo MrbSlaveLocalVideo[] =
{
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.3", 23u },
    { IasAvbStreamDirection::eIasAvbTransmitToNetwork, 4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb_streaming.4", 24u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatRtp, "media_transport.avb_streaming.1", 21u },
    { IasAvbStreamDirection::eIasAvbReceiveFromNetwork,4000u, 1460u, IasAvbVideoFormat::eIasAvbVideoFormatIec61883, "media_transport.avb_streaming.2", 22u },
    cTerminator_StreamParamsVideo
};


//------------------------------------------
//
// Registry Entries
//
//------------------------------------------
RegistryEntries RegStandard[] =
{
    { "tspec.presentation.time.offset.low", true, 1000000u, NULL }, // set presentation time offset to value suitable for lab tools
    { "tspec.vlanid.low", true, 2u, NULL },
    { "tspec.vlanprio.low", true, 3u, NULL },
    { "tspec.interval.high", true, 125000u, NULL },
    { "compatibility.audio", false, 0u, "SAF" },
    { "tspec.interval.low", true, 1333000u, NULL }, // enable class C suport
    { "local.alsa.ringbuffer", true, 512u, NULL },  // base period size * 4
    cTerminator_RegistryEntries
};

RegistryEntries RegTgtMrb[] =
{
    { "clock.hwcapture.nominal", true, 0u, NULL }, // disable H/W capture
    cTerminator_RegistryEntries
};

RegistryEntries RegClassCVideoBandwidth[] =
{
    { "audio.rx.srclass", false, 0u, "low" },
    { "video.rx.srclass", false, 0u, "low" },
    { "tspec.interval.low", true, 1333000u, NULL },
    { "tspec.interval.high", true, 1333000u, NULL },
    { "tx.maxbandwidth.low", true, 500000u, NULL },
    { "receive.idlewait", true, 50000000u, NULL }, // idlewait to 50 ms due to video data flow
    { "local.alsa.ringbuffer", true, 768u, NULL }, // alsa period size * 4
    { "tspec.presentation.time.offset.low", true, 2000000u, NULL }, // two MRBs and one switch in-between (2.0ms)
    cTerminator_RegistryEntries
};


//------------------------------------------
//
// Target Entries
//
//------------------------------------------

TargetParams Targets[] =
{
    { "NGIO",    0x1531u, 3u, "eth1", NULL },
    { "GrMrb",   0x157Cu, 2u, "eth0", RegTgtMrb },
    { "CvH",     0x1531u, 1u, "eth0", NULL },
    { "Fedora",  0x1533u, 7u, "p1p1", NULL },
};

ProfileParams Profiles[] =
{

    { "UnitTests", Unittest2chSetupAvbRxNC, Unittest2chSetupAvbTxNC, NULL, NULL, NULL, NULL, NULL, NULL, RegStandard, StandardBrd2MasterTestTones },
    { "Video_POC_Master",       NULL,   NULL,   VideoPocSetupAvbVideoMasterRx, VideoPocSetupAvbVideoMasterTx, NULL, NULL, NULL, VideoPocSetupLocalVideoMaster, RegClassCVideoBandwidth, NULL },
    { "Video_POC_MpegTs_Master",       NULL,   NULL,   VideoPocSetupAvbVideoMpegTsMasterRx, VideoPocSetupAvbVideoMpegTsMasterTx, NULL, NULL, NULL, VideoPocSetupLocalVideoMpegTsMaster, RegClassCVideoBandwidth, NULL },
    { "Video_POC_Slave",        NULL,   NULL,   VideoPocSetupAvbVideoSlaveRx, VideoPocSetupAvbVideoSlaveTx, NULL, NULL, NULL, VideoPocSetupLocalVideoSlave, RegClassCVideoBandwidth, NULL },
    { "Video_POC_MpegTs_Slave",        NULL,   NULL,   VideoPocSetupAvbVideoMpegTsSlaveRx, VideoPocSetupAvbVideoMpegTsSlaveTx, NULL, NULL, NULL, VideoPocSetupLocalVideoMpegTsSlave, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_Audio",  MrbMasterAvbRx, MrbMasterAvbTx, NULL, NULL, NULL, MrbMasterClkRefTx, MrbMasterAlsa, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_Audio1",  MrbMasterAvbRx, MrbMasterAvbTx, NULL, NULL, NULL, MrbMasterClkRefTx, MrbMasterAlsa1, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_Audio2",  MrbMasterAvbRx, MrbMasterAvbTx, NULL, NULL, NULL, MrbMasterClkRefTx, MrbMasterAlsa2, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_Audio3",  MrbMasterAvbRx, MrbMasterAvbTx, NULL, NULL, NULL, MrbMasterClkRefTx, MrbMasterAlsa3, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_Audio_Raw",  MrbMasterAvbRx, MrbMasterAvbTxRaw, NULL, NULL, NULL, MrbMasterClkRefTxRaw, MrbMasterAlsaRaw, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Slave_Audio", MrbSlaveAvbRx, MrbSlaveAvbTx, NULL, NULL, MrbSlaveClkRefRx, NULL, MrbSlaveAlsa, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Slave_Audio1", MrbSlaveAvbRx, MrbSlaveAvbTx, NULL, NULL, MrbSlaveClkRefRx, NULL, MrbSlaveAlsa1, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Slave_Audio2", MrbSlaveAvbRx, MrbSlaveAvbTx, NULL, NULL, MrbSlaveClkRefRx, NULL, MrbSlaveAlsa2, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_AV",  MrbMasterAvbRx, MrbMasterAvbTx, MrbMasterAvbVideoRx, MrbMasterAvbVideoTx, NULL, MrbMasterClkRefTx, MrbMasterAlsa, MrbMasterLocalVideo, RegClassCVideoBandwidth, NULL },
    { "MRB_Slave_AV", MrbSlaveAvbRx, MrbSlaveAvbTx, MrbSlaveAvbVideoRx, MrbSlaveAvbVideoTx, MrbSlaveClkRefRx, NULL, MrbSlaveAlsa, MrbSlaveLocalVideo, RegClassCVideoBandwidth, NULL },
    { "MRB_Master_Crs",  NULL, NULL, NULL, NULL, NULL, MrbMasterClkRefTx, NULL, NULL, RegClassCVideoBandwidth, NULL },
    { "MRB_Slave_Crs", NULL, NULL, NULL, NULL, MrbSlaveClkRefRx, NULL, NULL, NULL, RegClassCVideoBandwidth, NULL },
};


/**
 * This is our configuration object. It registers automatically through the base class's constructor.
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
} theConfigObject;
