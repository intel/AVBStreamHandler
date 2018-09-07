/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/


#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"

/**
 * namespace IasMediaTransportAvb_v2_1
 */
namespace IasMediaTransportAvb {

static const IasAvbStreamDiagnostics IasAvbStreamDiagnosisDef = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

IasAvbStreamDiagnostics::IasAvbStreamDiagnostics()
  : mediaLocked(0u)
  , mediaUnlocked(0u)
  , streamInterrupted(0u)
  , seqNumMismatch(0u)
  , mediaReset(0u)
  , timestampUncertain(0u)
  , timestampValid(0u)
  , timestampNotValid(0u)
  , unsupportedFormat(0u)
  , lateTimestamp(0u)
  , earlyTimestamp(0u)
  , framesRx(0u)
  , framesTx(0u)
  , resetCount(0u)
{
}

IasAvbStreamDiagnostics::IasAvbStreamDiagnostics(IasAvbStreamDiagnostics const & iOther)
  : mediaLocked(iOther.mediaLocked)
  , mediaUnlocked(iOther.mediaUnlocked)
  , streamInterrupted(iOther.streamInterrupted)
  , seqNumMismatch(iOther.seqNumMismatch)
  , mediaReset(iOther.mediaReset)
  , timestampUncertain(iOther.timestampUncertain)
  , timestampValid(iOther.timestampValid)
  , timestampNotValid(iOther.timestampNotValid)
  , unsupportedFormat(iOther.unsupportedFormat)
  , lateTimestamp(iOther.lateTimestamp)
  , earlyTimestamp(iOther.earlyTimestamp)
  , framesRx(iOther.framesRx)
  , framesTx(iOther.framesTx)
  , resetCount(iOther.resetCount)
{
}

IasAvbStreamDiagnostics::IasAvbStreamDiagnostics(
    uint32_t const & iMediaLocked,
    uint32_t const & iMediaUnlocked,
    uint32_t const & iStreamInterrupted,
    uint32_t const & iSeqNumMismatch,
    uint32_t const & iMediaReset,
    uint32_t const & iTimestampUncertain,
    uint32_t const & iTimestampValid,
    uint32_t const & iTimestampNotValid,
    uint32_t const & iUnsupportedFormat,
    uint32_t const & iLateTimestamp,
    uint32_t const & iEarlyTimestamp,
    uint32_t const & iFramesRx,
    uint32_t const & iFramesTx,
    uint32_t const & iResetCount)
  : mediaLocked(iMediaLocked)
  , mediaUnlocked(iMediaUnlocked)
  , streamInterrupted(iStreamInterrupted)
  , seqNumMismatch(iSeqNumMismatch)
  , mediaReset(iMediaReset)
  , timestampUncertain(iTimestampUncertain)
  , timestampValid(iTimestampValid)
  , timestampNotValid(iTimestampNotValid)
  , unsupportedFormat(iUnsupportedFormat)
  , lateTimestamp(iLateTimestamp)
  , earlyTimestamp(iEarlyTimestamp)
  , framesRx(iFramesRx)
  , framesTx(iFramesTx)
  , resetCount(iResetCount)
{
}

IasAvbStreamDiagnostics & IasAvbStreamDiagnostics::operator = (IasAvbStreamDiagnostics const & iOther)
{
  if (this != &iOther)
  {
    mediaLocked = iOther.mediaLocked;
    mediaUnlocked = iOther.mediaUnlocked;
    streamInterrupted = iOther.streamInterrupted;
    seqNumMismatch = iOther.seqNumMismatch;
    mediaReset = iOther.mediaReset;
    timestampUncertain = iOther.timestampUncertain;
    timestampValid = iOther.timestampValid;
    timestampNotValid = iOther.timestampNotValid;
    unsupportedFormat = iOther.unsupportedFormat;
    lateTimestamp = iOther.lateTimestamp;
    earlyTimestamp = iOther.earlyTimestamp;
    framesRx = iOther.framesRx;
    framesTx = iOther.framesTx;
    resetCount = iOther.resetCount;
  }

  return *this;
}

bool IasAvbStreamDiagnostics::operator == (IasAvbStreamDiagnostics const & iOther) const
{
  return (this == &iOther)
      || (   (mediaLocked == iOther.mediaLocked)
          && (mediaUnlocked == iOther.mediaUnlocked)
          && (streamInterrupted == iOther.streamInterrupted)
          && (seqNumMismatch == iOther.seqNumMismatch)
          && (mediaReset == iOther.mediaReset)
          && (timestampUncertain == iOther.timestampUncertain)
          && (timestampValid == iOther.timestampValid)
          && (timestampNotValid == iOther.timestampNotValid)
          && (unsupportedFormat == iOther.unsupportedFormat)
          && (lateTimestamp == iOther.lateTimestamp)
          && (earlyTimestamp == iOther.earlyTimestamp)
          && (framesRx == iOther.framesRx)
          && (framesTx == iOther.framesTx)
          && (resetCount == iOther.resetCount));
}

bool IasAvbStreamDiagnostics::operator != (IasAvbStreamDiagnostics const & iOther) const
{
  return !(*this == iOther);
}

IasAvbAudioStreamAttributes::IasAvbAudioStreamAttributes()
  : direction(eIasAvbTransmitToNetwork)
  , maxNumChannels(0u)
  , numChannels(0u)
  , sampleFreq(0u)
  , format(eIasAvbAudioFormatIec61883)
  , clockId(0u)
  , assignMode(eIasAvbIdAssignModeStatic)
  , streamId(0u)
  , dmac(0u)
  , sourceMac(0u)
  , txActive(0)
  , rxStatus(eIasAvbStreamInactive)
  , localStreamId(0u)
  , preconfigured(0)
  , diagnostics(IasAvbStreamDiagnosisDef)
{
}

IasAvbAudioStreamAttributes::IasAvbAudioStreamAttributes(IasAvbAudioStreamAttributes const & iOther)
  : direction(iOther.direction)
  , maxNumChannels(iOther.maxNumChannels)
  , numChannels(iOther.numChannels)
  , sampleFreq(iOther.sampleFreq)
  , format(iOther.format)
  , clockId(iOther.clockId)
  , assignMode(iOther.assignMode)
  , streamId(iOther.streamId)
  , dmac(iOther.dmac)
  , sourceMac(iOther.sourceMac)
  , txActive(iOther.txActive)
  , rxStatus(iOther.rxStatus)
  , localStreamId(iOther.localStreamId)
  , preconfigured(iOther.preconfigured)
  , diagnostics(iOther.diagnostics)
{
}

IasAvbAudioStreamAttributes::IasAvbAudioStreamAttributes(
    IasAvbStreamDirection const & iDirection,
    uint16_t const & iMaxNumChannels,
    uint16_t const & iNumChannels,
    uint32_t const & iSampleFreq,
    IasAvbAudioFormat const & iFormat,
    uint32_t const & iClockId,
    IasAvbIdAssignMode const & iAssignMode,
    uint64_t const & iStreamId,
    uint64_t const & iDmac,
    uint64_t const & iSourceMac,
    bool const & iTxActive,
    IasAvbStreamState const & iRxStatus,
    uint16_t const & iLocalStreamId,
    bool const & iPreconfigured,
    IasAvbStreamDiagnostics const & iDiagnostics)
  : direction(iDirection)
  , maxNumChannels(iMaxNumChannels)
  , numChannels(iNumChannels)
  , sampleFreq(iSampleFreq)
  , format(iFormat)
  , clockId(iClockId)
  , assignMode(iAssignMode)
  , streamId(iStreamId)
  , dmac(iDmac)
  , sourceMac(iSourceMac)
  , txActive(iTxActive)
  , rxStatus(iRxStatus)
  , localStreamId(iLocalStreamId)
  , preconfigured(iPreconfigured)
  , diagnostics(iDiagnostics)
{
}

IasAvbAudioStreamAttributes & IasAvbAudioStreamAttributes::operator = (IasAvbAudioStreamAttributes const & iOther)
{
  if (this != &iOther)
  {
    direction = iOther.direction;
    maxNumChannels = iOther.maxNumChannels;
    numChannels = iOther.numChannels;
    sampleFreq = iOther.sampleFreq;
    format = iOther.format;
    clockId = iOther.clockId;
    assignMode = iOther.assignMode;
    streamId = iOther.streamId;
    dmac = iOther.dmac;
    sourceMac = iOther.sourceMac;
    txActive = iOther.txActive;
    rxStatus = iOther.rxStatus;
    localStreamId = iOther.localStreamId;
    preconfigured = iOther.preconfigured;
    diagnostics = iOther.diagnostics;
  }

  return *this;
}

bool IasAvbAudioStreamAttributes::operator == (IasAvbAudioStreamAttributes const & iOther) const
{
  return (this == &iOther)
      || (   (direction == iOther.direction)
          && (maxNumChannels == iOther.maxNumChannels)
          && (numChannels == iOther.numChannels)
          && (sampleFreq == iOther.sampleFreq)
          && (format == iOther.format)
          && (clockId == iOther.clockId)
          && (assignMode == iOther.assignMode)
          && (streamId == iOther.streamId)
          && (dmac == iOther.dmac)
          && (sourceMac == iOther.sourceMac)
          && (txActive == iOther.txActive)
          && (rxStatus == iOther.rxStatus)
          && (localStreamId == iOther.localStreamId)
          && (preconfigured == iOther.preconfigured)
          && (diagnostics == iOther.diagnostics));
}

bool IasAvbAudioStreamAttributes::operator != (IasAvbAudioStreamAttributes const & iOther) const
{
  return !(*this == iOther);
}

IasAvbVideoStreamAttributes::IasAvbVideoStreamAttributes()
  : direction(eIasAvbTransmitToNetwork)
  , maxPacketRate(0u)
  , maxPacketSize(0u)
  , format(IasAvbVideoFormat::eIasAvbVideoFormatIec61883)
  , clockId(0u)
  , assignMode(eIasAvbIdAssignModeStatic)
  , streamId(0u)
  , dmac(0u)
  , sourceMac(0u)
  , txActive(0)
  , rxStatus(eIasAvbStreamInactive)
  , localStreamId(0u)
  , preconfigured(0)
  , diagnostics(IasAvbStreamDiagnosisDef)
{
}

IasAvbVideoStreamAttributes::IasAvbVideoStreamAttributes(IasAvbVideoStreamAttributes const & iOther)
  : direction(iOther.direction)
  , maxPacketRate(iOther.maxPacketRate)
  , maxPacketSize(iOther.maxPacketSize)
  , format(iOther.format)
  , clockId(iOther.clockId)
  , assignMode(iOther.assignMode)
  , streamId(iOther.streamId)
  , dmac(iOther.dmac)
  , sourceMac(iOther.sourceMac)
  , txActive(iOther.txActive)
  , rxStatus(iOther.rxStatus)
  , localStreamId(iOther.localStreamId)
  , preconfigured(iOther.preconfigured)
  , diagnostics(iOther.diagnostics)
{
}

IasAvbVideoStreamAttributes::IasAvbVideoStreamAttributes(
    IasAvbStreamDirection const & iDirection,
    uint16_t const & iMaxPacketRate,
    uint16_t const & iMaxPacketSize,
    IasAvbVideoFormat const & iFormat,
    uint32_t const & iClockId,
    IasAvbIdAssignMode const & iAssignMode,
    uint64_t const & iStreamId,
    uint64_t const & iDmac,
    uint64_t const & iSourceMac,
    bool const & iTxActive,
    IasAvbStreamState const & iRxStatus,
    uint16_t const & iLocalStreamId,
    bool const & iPreconfigured,
    IasAvbStreamDiagnostics const & iDiagnostics)
  : direction(iDirection)
  , maxPacketRate(iMaxPacketRate)
  , maxPacketSize(iMaxPacketSize)
  , format(iFormat)
  , clockId(iClockId)
  , assignMode(iAssignMode)
  , streamId(iStreamId)
  , dmac(iDmac)
  , sourceMac(iSourceMac)
  , txActive(iTxActive)
  , rxStatus(iRxStatus)
  , localStreamId(iLocalStreamId)
  , preconfigured(iPreconfigured)
  , diagnostics(iDiagnostics)
{
}

IasAvbVideoStreamAttributes & IasAvbVideoStreamAttributes::operator = (IasAvbVideoStreamAttributes const & iOther)
{
  if (this != &iOther)
  {
    direction = iOther.direction;
    maxPacketRate = iOther.maxPacketRate;
    maxPacketSize = iOther.maxPacketSize;
    format = iOther.format;
    clockId = iOther.clockId;
    assignMode = iOther.assignMode;
    streamId = iOther.streamId;
    dmac = iOther.dmac;
    sourceMac = iOther.sourceMac;
    txActive = iOther.txActive;
    rxStatus = iOther.rxStatus;
    localStreamId = iOther.localStreamId;
    preconfigured = iOther.preconfigured;
    diagnostics = iOther.diagnostics;
  }

  return *this;
}

bool IasAvbVideoStreamAttributes::operator == (IasAvbVideoStreamAttributes const & iOther) const
{
  return (this == &iOther)
      || (   (direction == iOther.direction)
          && (maxPacketRate == iOther.maxPacketRate)
          && (maxPacketSize == iOther.maxPacketSize)
          && (format == iOther.format)
          && (clockId == iOther.clockId)
          && (assignMode == iOther.assignMode)
          && (streamId == iOther.streamId)
          && (dmac == iOther.dmac)
          && (sourceMac == iOther.sourceMac)
          && (txActive == iOther.txActive)
          && (rxStatus == iOther.rxStatus)
          && (localStreamId == iOther.localStreamId)
          && (preconfigured == iOther.preconfigured)
          && (diagnostics == iOther.diagnostics));
}

bool IasAvbVideoStreamAttributes::operator != (IasAvbVideoStreamAttributes const & iOther) const
{
  return !(*this == iOther);
}

IasAvbClockReferenceStreamAttributes::IasAvbClockReferenceStreamAttributes()
  : direction(eIasAvbTransmitToNetwork)
  , type(eIasAvbCrsTypeAudio)
  , crfStampsPerPdu(0u)
  , crfStampInterval(0u)
  , baseFreq(0u)
  , pull(eIasAvbCrsMultFlat)
  , clockId(0u)
  , assignMode(eIasAvbIdAssignModeStatic)
  , streamId(0u)
  , dmac(0u)
  , sourceMac(0u)
  , txActive(0)
  , rxStatus(eIasAvbStreamInactive)
  , preconfigured(0)
  , diagnostics(IasAvbStreamDiagnosisDef)
{
}

IasAvbClockReferenceStreamAttributes::IasAvbClockReferenceStreamAttributes(IasAvbClockReferenceStreamAttributes const & iOther)
  : direction(iOther.direction)
  , type(iOther.type)
  , crfStampsPerPdu(iOther.crfStampsPerPdu)
  , crfStampInterval(iOther.crfStampInterval)
  , baseFreq(iOther.baseFreq)
  , pull(iOther.pull)
  , clockId(iOther.clockId)
  , assignMode(iOther.assignMode)
  , streamId(iOther.streamId)
  , dmac(iOther.dmac)
  , sourceMac(iOther.sourceMac)
  , txActive(iOther.txActive)
  , rxStatus(iOther.rxStatus)
  , preconfigured(iOther.preconfigured)
  , diagnostics(iOther.diagnostics)
{
}

IasAvbClockReferenceStreamAttributes::IasAvbClockReferenceStreamAttributes(
    IasAvbStreamDirection const & iDirection,
    IasAvbClockReferenceStreamType const & iType,
    uint16_t const & iCrfStampsPerPdu,
    uint16_t const & iCrfStampInterval,
    uint32_t const & iBaseFreq,
    IasAvbClockMultiplier const & iPull,
    uint32_t const & iClockId,
    IasAvbIdAssignMode const & iAssignMode,
    uint64_t const & iStreamId,
    uint64_t const & iDmac,
    uint64_t const & iSourceMac,
    bool const & iTxActive,
    IasAvbStreamState const & iRxStatus,
    bool const & iPreconfigured,
    IasAvbStreamDiagnostics const & iDiagnostics)
  : direction(iDirection)
  , type(iType)
  , crfStampsPerPdu(iCrfStampsPerPdu)
  , crfStampInterval(iCrfStampInterval)
  , baseFreq(iBaseFreq)
  , pull(iPull)
  , clockId(iClockId)
  , assignMode(iAssignMode)
  , streamId(iStreamId)
  , dmac(iDmac)
  , sourceMac(iSourceMac)
  , txActive(iTxActive)
  , rxStatus(iRxStatus)
  , preconfigured(iPreconfigured)
  , diagnostics(iDiagnostics)
{
}

IasAvbClockReferenceStreamAttributes & IasAvbClockReferenceStreamAttributes::operator = (IasAvbClockReferenceStreamAttributes const & iOther)
{
  if (this != &iOther)
  {
    direction = iOther.direction;
    type = iOther.type;
    crfStampsPerPdu = iOther.crfStampsPerPdu;
    crfStampInterval = iOther.crfStampInterval;
    baseFreq = iOther.baseFreq;
    pull = iOther.pull;
    clockId = iOther.clockId;
    assignMode = iOther.assignMode;
    streamId = iOther.streamId;
    dmac = iOther.dmac;
    sourceMac = iOther.sourceMac;
    txActive = iOther.txActive;
    rxStatus = iOther.rxStatus;
    preconfigured = iOther.preconfigured;
    diagnostics = iOther.diagnostics;
  }

  return *this;
}

bool IasAvbClockReferenceStreamAttributes::operator == (IasAvbClockReferenceStreamAttributes const & iOther) const
{
  return (this == &iOther)
      || (   (direction == iOther.direction)
          && (type == iOther.type)
          && (crfStampsPerPdu == iOther.crfStampsPerPdu)
          && (crfStampInterval == iOther.crfStampInterval)
          && (baseFreq == iOther.baseFreq)
          && (pull == iOther.pull)
          && (clockId == iOther.clockId)
          && (assignMode == iOther.assignMode)
          && (streamId == iOther.streamId)
          && (dmac == iOther.dmac)
          && (sourceMac == iOther.sourceMac)
          && (txActive == iOther.txActive)
          && (rxStatus == iOther.rxStatus)
          && (preconfigured == iOther.preconfigured)
          && (diagnostics == iOther.diagnostics));
}

bool IasAvbClockReferenceStreamAttributes::operator != (IasAvbClockReferenceStreamAttributes const & iOther) const
{
  return !(*this == iOther);
}

} // namespace IasMediaTransportAvb_v2_1
