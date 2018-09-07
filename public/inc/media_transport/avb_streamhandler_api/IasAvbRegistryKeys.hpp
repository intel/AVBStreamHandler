/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbRegistryKeys.hpp
 * @brief   key names to be used with the AvbStreamHandler registry
 * @details Use the constants defined here to access the registry.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_REGISTRYKEYS_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_REGISTRYKEYS_HPP

namespace IasMediaTransportAvb {


//@{
/**
 * @brief registry keys
 *
 */
namespace IasRegKeys
{
static const char cNwIfName[] =              "network.interface.name";          ///< (std::string) network interface name to be used (default: eth0)
static const char cNwIfPtpDev[] =            "network.interface.ptp.dev";       ///< (std::string) path of the ptp device to be used (default: auto detect)
static const char cAudioFloatGain[] =        "audio.tx.floatconversiongain";    ///< (uint64_t/1000000) multiplier for conversion float->int (default 500000 = 0.5) (deprecated)
static const char cAlsaDevicePeriods[] =     "alsa.device.periods.";            ///< (uint32_t) Number of Alsa periods the buffer contains within the Alsa device. Has to be appended by device name. (deprecated)
static const char cClockDriverFileName[] =   "clockdriver.filename";            ///< (std::string) file name of clock driver lib (default: none)
static const char cClockHwCapFrequency[] =   "clock.hwcapture.nominal";         ///< (uint64_t/Hz*1000) nominal clock frequency of the clock signal measured by HW time capturing (default: 93750)
static const char cAlsaNumFrames[] =         "local.alsa.frames";               ///< (uint32_t) how many ALSA frames are handled within one buffer (default: 256)
static const char cAlsaRingBuffer[] =        "local.alsa.periods";              ///< (uint32_t) how many Alsa periods fit into the ring buffer (default: 12)
static const char cAlsaRingBufferSz[] =      "local.alsa.ringbuffer";           ///< (uint32_t) how many samples fit into the ring buffer, must be a multiple of alsa (or base) period size
static const char cAlsaBaseFreq[] =          "local.alsa.basefreq";             ///< (uint32_t/Hz) base frequency for ALSA engine (default: 48000)
static const char cAlsaBasePeriod[] =        "local.alsa.baseperiod";           ///< (uint32_t) base ALSA period size for ALSA engine (default:128)
static const char cAlsaGroupName[] =         "alsa.groupname";                  ///< (std::string) GroupName used when creating alsa shared memory
static const char cVideoGroupName[] =        "video.groupname";                 ///< (std::string) GroupName used when creating v-streaming shared memory
static const char cSchedPolicy[] =           "sched.policy";                    ///< (int32_t) scheduler policy (default = SCHED_FIFO)
static const char cSchedPriority[] =         "sched.priority";                  ///< (int32_t) scheduler priority (default = 1)
static const char cAudioSparseTS[] =         "audio.tx.sparsetimestamp";        ///< (bool) 0=off, 1=on (default: off)
static const char cTestingProfileEnable[] =  "testing.profile.enable";          ///< (bool) 0=off, 1=on (default: off)

// the following strings need to be appended by the class ('high' or 'low' in lower case)
static const char cTSpecInterval[] =   "tspec.interval.";                ///< (uint64_t) SR Class measurement interval (ns)
static const char cTSpecVlanId[] =     "tspec.vlanid.";                  ///< (uint16_t) SR Class VLAN id
static const char cTSpecVlanPrio[] =   "tspec.vlanprio.";                ///< (uint8_t)  SR Class VLAN priority
static const char cTSpecPresTimeOff[] ="tspec.presentation.time.offset.";///< (uint32_t) SR Class presentation time offset in ns
static const char cTxMaxStreams[] =    "tx.maxstreams.";                 ///< (uint32_t) Transmit Engine / SR Class maxmimum number of active streams (default: 10)
static const char cTxMaxBw[] =         "tx.maxbandwidth.";               ///< (uint32_t/kbps) Transmit Engine / SR Class maxmimum bandwidth to allocate
static const char cTxMaxFrameLength[] ="tx.maxframelength.";             ///< (uint16_t/bytes) Transmit Engine / SR Class maxmimum Ethernet packet length (default: 1500)

}
//@}

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_REGISTRYKEYS_HPP */
