/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbStreamHandlerTypes.hpp
 * @brief   The definition of generic AVB types.
 * @date    2017
 */

#ifndef IASAVBSTREAMHANDLERTYPES_HPP_
#define IASAVBSTREAMHANDLERTYPES_HPP_

#include <stdint.h>
#include <vector>

namespace IasMediaTransportAvb {

//@{
/// import fidl type definitions into our namespace for convenience and readability

enum IasAvbAudioFormat
{
  eIasAvbAudioFormatIec61883 = 0,
  eIasAvbAudioFormatSaf16    = 1,
  eIasAvbAudioFormatSaf24    = 2,
  eIasAvbAudioFormatSaf32    = 3,
  eIasAvbAudioFormatSafFloat = 4,
};

enum IasAvbTestToneMode
{
  eIasAvbTestToneSine     = 0,
  eIasAvbTestTonePulse    = 1,
  eIasAvbTestToneSawtooth = 2,
  eIasAvbTestToneFile     = 3,
};

enum IasAvbClockReferenceStreamType
{
  eIasAvbCrsTypeUser          = 0,
  eIasAvbCrsTypeAudio         = 1,
  eIasAvbCrsTypeVideoFrame    = 2,
  eIasAvbCrsTypeVideoLine     = 3,
  eIasAvbCrsTypeMachineCycle  = 4,
};

enum IasAvbClockMultiplier
{
  eIasAvbCrsMultFlat          = 0,
  eIasAvbCrsMultMinus1000ppm  = 1,
  eIasAvbCrsMultPlus1000ppm   = 2,
  eIasAvbCrsMultTvToMovie     = 3,
  eIasAvbCrsMultMovieToTv     = 4,
  eIasAvbCrsMultOneEighth     = 5,
};

enum IasAvbIdAssignMode
{
  eIasAvbIdAssignModeStatic       = 0,
  eIasAvbIdAssignModeDynamicAll   = 1,
  eIasAvbIdAssignModeDynamicMaap  = 2,
  eIasAvbIdAssignModeDynamicSrp   = 3
};

enum IasAvbResult
{
  eIasAvbResultOk               = 0,
  eIasAvbResultErr              = 1,
  eIasAvbResultNotImplemented   = 2,
  eIasAvbResultNotSupported     = 3,
  eIasAvbResultInvalidParam     = 4
};

enum IasAvbStreamState
{
  eIasAvbStreamInactive     = 0,
  eIasAvbStreamNoData       = 1,
  eIasAvbStreamInvalidData  = 2,
  eIasAvbStreamValid        = 3,
};

enum IasAvbStreamDirection
{
  eIasAvbTransmitToNetwork  = 0,
  eIasAvbReceiveFromNetwork = 1,
};

enum IasAvbVideoFormat
{
  eIasAvbVideoFormatIec61883 = 0,
  eIasAvbVideoFormatRtp      = 1,
};

enum IasAvbSrClass
{
  eIasAvbSrClassHigh = 0,
  eIasAvbSrClassLow  = 1,
};

enum IasAlsaDeviceTypes
{
  eIasAlsaVirtualDevice = 0,
  eIasAlsaHwDevice      = 1,
  eIasAlsaHwDeviceAsync = 2,
};

struct IasAvbStreamDiagnostics
{
  /*!
   * @brief Default constructor.
   */
  IasAvbStreamDiagnostics();

  /*!
   * @brief Copy constructor.
   */
  IasAvbStreamDiagnostics(IasAvbStreamDiagnostics const & iOther);

  /*!
   * @brief Field constructor.
   *
   * @param[in] iiMediaLocked Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiMediaUnlocked Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiStreamInterrupted Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiSeqNumMismatch Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiMediaReset Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiTimestampUncertain Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiTimestampValid Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiTimestampNotValid Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiUnsupportedFormat Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiLateTimestamp Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiEarlyTimestamp Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiFramesRx Initial value for field IasAvbStreamDiagnostics.
   * @param[in] iiFramesTx Initial value for field IasAvbStreamDiagnostics.
   */
  IasAvbStreamDiagnostics(uint32_t const & iMediaLocked, uint32_t const & iMediaUnlocked, uint32_t const & iStreamInterrupted, uint32_t const & iSeqNumMismatch, uint32_t const & iMediaReset, uint32_t const & iTimestampUncertain, uint32_t const & iTimestampValid, uint32_t const & iTimestampNotValid, uint32_t const & iUnsupportedFormat, uint32_t const & iLateTimestamp, uint32_t const & iEarlyTimestamp, uint32_t const & iFramesRx, uint32_t const & iFramesTx, uint32_t const & resetCount);

  /*!
   * @brief Assignment operator.
   */
  IasAvbStreamDiagnostics & operator = (IasAvbStreamDiagnostics const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (IasAvbStreamDiagnostics const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (IasAvbStreamDiagnostics const & iOther) const;

  /**
   * description: Increments on a stream media clock locking.
   */
  inline const uint32_t &getMediaLocked() const { return mediaLocked; }
  inline void setMediaLocked(const uint32_t &_value) { mediaLocked = _value; }
  /**
   * description: Increments on a stream media clock unlocking.
   */
  inline const uint32_t &getMediaUnlocked() const { return mediaUnlocked; }
  inline void setMediaUnlocked(const uint32_t &_value) { mediaUnlocked = _value; }
  /**
   * description: Increments when stream playback is interrupted.
   */
  inline const uint32_t &getStreamInterrupted() const { return streamInterrupted; }
  inline void setStreamInterrupted(const uint32_t &_value) { streamInterrupted = _value; }
  /**
   * description: Increments when an AVTPDU is received with a nonsequential sequence_num field.
   */
  inline const uint32_t &getSeqNumMismatch() const { return seqNumMismatch; }
  inline void setSeqNumMismatch(const uint32_t &_value) { seqNumMismatch = _value; }
  /**
   * description: Increments on a toggle of the mr bit in the AVTPDU.
   */
  inline const uint32_t &getMediaReset() const { return mediaReset; }
  inline void setMediaReset(const uint32_t &_value) { mediaReset = _value; }
  /**
   * description: Increments on a toggle of the tu bit in the AVTPDU.
   */
  inline const uint32_t &getTimestampUncertain() const { return timestampUncertain; }
  inline void setTimestampUncertain(const uint32_t &_value) { timestampUncertain = _value; }
  /**
   * description: Increments on receipt or transmit of an AVTPDU with the tv bit set.
   */
  inline const uint32_t &getTimestampValid() const { return timestampValid; }
  inline void setTimestampValid(const uint32_t &_value) { timestampValid = _value; }
  /**
   * description: Increments on receipt or transmit of an AVTPDU with the tv bit cleared.
   */
  inline const uint32_t &getTimestampNotValid() const { return timestampNotValid; }
  inline void setTimestampNotValid(const uint32_t &_value) { timestampNotValid = _value; }
  /**
   * description: Increments on receipt of an AVTPDU that contains an unsupported media type.
   */
  inline const uint32_t &getUnsupportedFormat() const { return unsupportedFormat; }
  inline void setUnsupportedFormat(const uint32_t &_value) { unsupportedFormat = _value; }
  /**
   * description: Increments on receipt of an AVTPDU with an avtp_timestamp field that is in the
   *   past.
   */
  inline const uint32_t &getLateTimestamp() const { return lateTimestamp; }
  inline void setLateTimestamp(const uint32_t &_value) { lateTimestamp = _value; }
  /**
   * description: Increments on receipt of an AVTPDU with an avtp_timestamp field that is too far
   *   in the future to process.
   */
  inline const uint32_t &getEarlyTimestamp() const { return earlyTimestamp; }
  inline void setEarlyTimestamp(const uint32_t &_value) { earlyTimestamp = _value; }
  /**
   * description: Increments on each AVTPDU received.
   */
  inline const uint32_t &getFramesRx() const { return framesRx; }
  inline void setFramesRx(const uint32_t &_value) { framesRx = _value; }
  /**
   * description: Increments on each AVTPDU transmitted.
   */
  inline const uint32_t &getFramesTx() const { return framesTx; }
  inline void setFramesTx(const uint32_t &_value) { framesTx = _value; }
  /**
   * description: Increments on each reset due to launch time lag.
   */
  inline const uint32_t &getResetCount() const { return resetCount; }
  inline void setResetCount(const uint32_t &_value) { resetCount = _value; }

  /*!
   * Increments on a stream media clock locking.
   */
  uint32_t mediaLocked;


  /*!
   * Increments on a stream media clock unlocking.
   */
  uint32_t mediaUnlocked;


  /*!
   * Increments when stream playback is interrupted.
   */
  uint32_t streamInterrupted;


  /*!
   * Increments when an AVTPDU is received with a nonsequential sequence_num field.
   */
  uint32_t seqNumMismatch;


  /*!
   * Increments on a toggle of the mr bit in the AVTPDU.
   */
  uint32_t mediaReset;


  /*!
   * Increments on a toggle of the tu bit in the AVTPDU.
   */
  uint32_t timestampUncertain;


  /*!
   * Increments on receipt or transmit of an AVTPDU with the tv bit set.
   */
  uint32_t timestampValid;


  /*!
   * Increments on receipt or transmit of an AVTPDU with the tv bit cleared.
   */
  uint32_t timestampNotValid;


  /*!
   * Increments on receipt of an AVTPDU that contains an unsupported media type.
   */
  uint32_t unsupportedFormat;


  /*!
   * Increments on receipt of an AVTPDU with an avtp_timestamp field that is in the past.
   */
  uint32_t lateTimestamp;


  /*!
   * Increments on receipt of an AVTPDU with an avtp_timestamp field that is too far in the future to process.
   */
  uint32_t earlyTimestamp;


  /*!
   * Increments on each AVTPDU received.
   */
  uint32_t framesRx;


  /*!
   * Increments on each AVTPDU transmitted.
   */
  uint32_t framesTx;

  /*!
   * Counter for "reset due to launch time lag"
   */
  uint32_t resetCount;

};

/*!
 * contains all fields describing attributes and status of an AVB audio stream.
 */
struct IasAvbAudioStreamAttributes
{
  /*!
   * @brief Default constructor.
   */
  IasAvbAudioStreamAttributes();

  /*!
   * @brief Copy constructor.
   */
  IasAvbAudioStreamAttributes(IasAvbAudioStreamAttributes const & iOther);

  /*!
   * @brief Field constructor.
   *
   * @param[in] iiDirection Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiMaxNumChannels Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiNumChannels Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiSampleFreq Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiFormat Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiClockId Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiAssignMode Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiStreamId Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiDmac Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiSourceMac Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiTxActive Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiRxStatus Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiLocalStreamId Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiPreconfigured Initial value for field IasAvbAudioStreamAttributes.
   * @param[in] iiDiagnostics Initial value for field IasAvbAudioStreamAttributes.
   */
  IasAvbAudioStreamAttributes(IasAvbStreamDirection const & iDirection, uint16_t const & iMaxNumChannels, uint16_t const & iNumChannels, uint32_t const & iSampleFreq, IasAvbAudioFormat const & iFormat, uint32_t const & iClockId, IasAvbIdAssignMode const & iAssignMode, uint64_t const & iStreamId, uint64_t const & iDmac, uint64_t const & iSourceMac, bool const & iTxActive, IasAvbStreamState const & iRxStatus, uint16_t const & iLocalStreamId, bool const & iPreconfigured, IasAvbStreamDiagnostics const & iDiagnostics);

  /*!
   * @brief Assignment operator.
   */
  IasAvbAudioStreamAttributes & operator = (IasAvbAudioStreamAttributes const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (IasAvbAudioStreamAttributes const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (IasAvbAudioStreamAttributes const & iOther) const;

  /**
   * description: direction of the stream (to/from the network)
   */
  inline const IasAvbStreamDirection &getDirection() const { return direction; }
  inline void setDirection(const IasAvbStreamDirection &_value) { direction = _value; }
  /**
   * description: maximum number of audio channels the stream has to support
   */
  inline const uint16_t &getMaxNumChannels() const { return maxNumChannels; }
  inline void setMaxNumChannels(const uint16_t &_value) { maxNumChannels = _value; }
  /**
   * description: current number of audio channels in use
   */
  inline const uint16_t &getNumChannels() const { return numChannels; }
  inline void setNumChannels(const uint16_t &_value) { numChannels = _value; }
  /**
   * description: audio sample frequency in Hz
   */
  inline const uint32_t &getSampleFreq() const { return sampleFreq; }
  inline void setSampleFreq(const uint32_t &_value) { sampleFreq = _value; }
  /**
   * description: audio format as encoded by IasAvbAudioFormat
   */
  inline const IasAvbAudioFormat &getFormat() const { return format; }
  inline void setFormat(const IasAvbAudioFormat &_value) { format = _value; }
  /**
   * description: Id of the clock domain to be used by the stream
   */
  inline const uint32_t &getClockId() const { return clockId; }
  inline void setClockId(const uint32_t &_value) { clockId = _value; }
  /**
   * description: controls the definition of streamId and destination MAC
   */
  inline const IasAvbIdAssignMode &getAssignMode() const { return assignMode; }
  inline void setAssignMode(const IasAvbIdAssignMode &_value) { assignMode = _value; }
  /**
   * description: streamId
   */
  inline const uint64_t &getStreamId() const { return streamId; }
  inline void setStreamId(const uint64_t &_value) { streamId = _value; }
  /**
   * description: destination MAC address
   */
  inline const uint64_t &getDmac() const { return dmac; }
  inline void setDmac(const uint64_t &_value) { dmac = _value; }
  /**
   * description: source MAC address of the talker
   */
  inline const uint64_t &getSourceMac() const { return sourceMac; }
  inline void setSourceMac(const uint64_t &_value) { sourceMac = _value; }
  /**
   * description: 'true' if stream is currently active (valid for transmit streams only)
   */
  inline const bool &getTxActive() const { return txActive; }
  inline void setTxActive(const bool _value) { txActive = _value; }
  /**
   * description: status of the stream (valid for receive streams only)
   */
  inline const IasAvbStreamState &getRxStatus() const { return rxStatus; }
  inline void setRxStatus(const IasAvbStreamState &_value) { rxStatus = _value; }
  /**
   * description: ID of local stream the AVB stream is connected to or 0 if not connected
   */
  inline const uint16_t &getLocalStreamId() const { return localStreamId; }
  inline void setLocalStreamId(const uint16_t &_value) { localStreamId = _value; }
  /**
   * description: 'true' if stream has been created statically by configuration library during
   *   startup, 'false' if stream has been created dynamically during runtime
   */
  inline const bool &getPreconfigured() const { return preconfigured; }
  inline void setPreconfigured(const bool _value) { preconfigured = _value; }
  /**
   * description: stream diagnostics data
   */
  inline const IasAvbStreamDiagnostics &getDiagnostics() const { return diagnostics; }
  inline void setDiagnostics(const IasAvbStreamDiagnostics &_value) { diagnostics = _value; }

  /*!
   * direction of the stream (to/from the network)
   */
  IasAvbStreamDirection direction;


  /*!
   * maximum number of audio channels the stream has to support
   */
  uint16_t maxNumChannels;


  /*!
   * current number of audio channels in use
   */
  uint16_t numChannels;


  /*!
   * audio sample frequency in Hz
   */
  uint32_t sampleFreq;


  /*!
   * audio format as encoded by IasAvbAudioFormat
   */
  IasAvbAudioFormat format;


  /*!
   * Id of the clock domain to be used by the stream
   */
  uint32_t clockId;


  /*!
   * controls the definition of streamId and destination MAC
   */
  IasAvbIdAssignMode assignMode;


  /*!
   * streamId
   */
  uint64_t streamId;


  /*!
   * destination MAC address
   */
  uint64_t dmac;


  /*!
   * source MAC address of the talker
   */
  uint64_t sourceMac;


  /*!
   * 'true' if stream is currently active (valid for transmit streams only)
   */
  bool txActive;


  /*!
   * status of the stream (valid for receive streams only)
   */
  IasAvbStreamState rxStatus;


  /*!
   * ID of local stream the AVB stream is connected to or 0 if not connected
   */
  uint16_t localStreamId;


  /*!
   * 'true' if stream has been created statically by configuration library during startup, 'false' if stream has been created dynamically during runtime
   */
  bool preconfigured;


  /*!
   * stream diagnostics data
   */
  IasAvbStreamDiagnostics diagnostics;


};

struct IasAvbVideoStreamAttributes
{
  /*!
   * @brief Default constructor.
   */
  IasAvbVideoStreamAttributes();

  /*!
   * @brief Copy constructor.
   */
  IasAvbVideoStreamAttributes(IasAvbVideoStreamAttributes const & iOther);

  /*!
   * @brief Field constructor.
   *
   * @param[in] iiDirection Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiMaxPacketRate Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiMaxPacketSize Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiFormat Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiClockId Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiAssignMode Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiStreamId Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiDmac Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiSourceMac Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiTxActive Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiRxStatus Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiLocalStreamId Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiPreconfigured Initial value for field IasAvbVideoStreamAttributes.
   * @param[in] iiDiagnostics Initial value for field IasAvbVideoStreamAttributes.
   */
  IasAvbVideoStreamAttributes(IasAvbStreamDirection const & iDirection, uint16_t const & iMaxPacketRate, uint16_t const & iMaxPacketSize, IasAvbVideoFormat const & iFormat, uint32_t const & iClockId, IasAvbIdAssignMode const & iAssignMode, uint64_t const & iStreamId, uint64_t const & iDmac, uint64_t const & iSourceMac, bool const & iTxActive, IasAvbStreamState const & iRxStatus, uint16_t const & iLocalStreamId, bool const & iPreconfigured, IasAvbStreamDiagnostics const & iDiagnostics);

  /*!
   * @brief Assignment operator.
   */
  IasAvbVideoStreamAttributes & operator = (IasAvbVideoStreamAttributes const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (IasAvbVideoStreamAttributes const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (IasAvbVideoStreamAttributes const & iOther) const;

  /**
   * description: direction of the stream (to/from the network)
   */
  inline const IasAvbStreamDirection &getDirection() const { return direction; }
  inline void setDirection(const IasAvbStreamDirection &_value) { direction = _value; }
  /**
   * description: maximum number of packets per second
   */
  inline const uint16_t &getMaxPacketRate() const { return maxPacketRate; }
  inline void setMaxPacketRate(const uint16_t &_value) { maxPacketRate = _value; }
  /**
   * description: maximum packet size in bytes
   */
  inline const uint16_t &getMaxPacketSize() const { return maxPacketSize; }
  inline void setMaxPacketSize(const uint16_t &_value) { maxPacketSize = _value; }
  /**
   * description: video format as encoded by IasAvbVideoFormat
   */
  inline const IasAvbVideoFormat &getFormat() const { return format; }
  inline void setFormat(const IasAvbVideoFormat &_value) { format = _value; }
  /**
   * description: Id of the clock domain to be used by the stream
   */
  inline const uint32_t &getClockId() const { return clockId; }
  inline void setClockId(const uint32_t &_value) { clockId = _value; }
  /**
   * description: controls the definition of streamId and destination MAC
   */
  inline const IasAvbIdAssignMode &getAssignMode() const { return assignMode; }
  inline void setAssignMode(const IasAvbIdAssignMode &_value) { assignMode = _value; }
  /**
   * description: streamId
   */
  inline const uint64_t &getStreamId() const { return streamId; }
  inline void setStreamId(const uint64_t &_value) { streamId = _value; }
  /**
   * description: destination MAC address
   */
  inline const uint64_t &getDmac() const { return dmac; }
  inline void setDmac(const uint64_t &_value) { dmac = _value; }
  /**
   * description: source MAC address of the talker
   */
  inline const uint64_t &getSourceMac() const { return sourceMac; }
  inline void setSourceMac(const uint64_t &_value) { sourceMac = _value; }
  /**
   * description: 'true' if stream is currently active (valid for transmit streams only)
   */
  inline const bool &getTxActive() const { return txActive; }
  inline void setTxActive(const bool _value) { txActive = _value; }
  /**
   * description: status of the stream (valid for receive streams only)
   */
  inline const IasAvbStreamState &getRxStatus() const { return rxStatus; }
  inline void setRxStatus(const IasAvbStreamState &_value) { rxStatus = _value; }
  /**
   * description: ID of local stream the AVB stream is connected to or 0 if not connected
   */
  inline const uint16_t &getLocalStreamId() const { return localStreamId; }
  inline void setLocalStreamId(const uint16_t &_value) { localStreamId = _value; }
  /**
   * description: 'true' if stream has been created statically by configuration library during
   *   startup, 'false' if stream has been created dynamically during runtime
   */
  inline const bool &getPreconfigured() const { return preconfigured; }
  inline void setPreconfigured(const bool _value) { preconfigured = _value; }
  /**
   * description: stream diagnostics data
   */
  inline const IasAvbStreamDiagnostics &getDiagnostics() const { return diagnostics; }
  inline void setDiagnostics(const IasAvbStreamDiagnostics &_value) { diagnostics = _value; }

  /*!
   * direction of the stream (to/from the network)
   */
  IasAvbStreamDirection direction;


  /*!
   * maximum number of packets per second
   */
  uint16_t maxPacketRate;


  /*!
   * maximum packet size in bytes
   */
  uint16_t maxPacketSize;


  /*!
   * video format as encoded by IasAvbVideoFormat
   */
  IasAvbVideoFormat format;


  /*!
   * Id of the clock domain to be used by the stream
   */
  uint32_t clockId;


  /*!
   * controls the definition of streamId and destination MAC
   */
  IasAvbIdAssignMode assignMode;


  /*!
   * streamId
   */
  uint64_t streamId;


  /*!
   * destination MAC address
   */
  uint64_t dmac;


  /*!
   * source MAC address of the talker
   */
  uint64_t sourceMac;


  /*!
   * 'true' if stream is currently active (valid for transmit streams only)
   */
  bool txActive;


  /*!
   * status of the stream (valid for receive streams only)
   */
  IasAvbStreamState rxStatus;


  /*!
   * ID of local stream the AVB stream is connected to or 0 if not connected
   */
  uint16_t localStreamId;


  /*!
   * 'true' if stream has been created statically by configuration library during startup, 'false' if stream has been created dynamically during runtime
   */
  bool preconfigured;


  /*!
   * stream diagnostics data
   */
  IasAvbStreamDiagnostics diagnostics;

};

struct IasAvbClockReferenceStreamAttributes
{
  /*!
   * @brief Default constructor.
   */
  IasAvbClockReferenceStreamAttributes();

  /*!
   * @brief Copy constructor.
   */
  IasAvbClockReferenceStreamAttributes(IasAvbClockReferenceStreamAttributes const & iOther);

  /*!
   * @brief Field constructor.
   *
   * @param[in] iiDirection Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiType Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiCrfStampsPerPdu Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiCrfStampInterval Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiBaseFreq Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiPull Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiClockId Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiAssignMode Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiStreamId Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiDmac Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiSourceMac Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiTxActive Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiRxStatus Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiPreconfigured Initial value for field IasAvbClockReferenceStreamAttributes.
   * @param[in] iiDiagnostics Initial value for field IasAvbClockReferenceStreamAttributes.
   */
  IasAvbClockReferenceStreamAttributes(IasAvbStreamDirection const & iDirection, IasAvbClockReferenceStreamType const & iType, uint16_t const & iCrfStampsPerPdu, uint16_t const & iCrfStampInterval, uint32_t const & iBaseFreq, IasAvbClockMultiplier const & iPull, uint32_t const & iClockId, IasAvbIdAssignMode const & iAssignMode, uint64_t const & iStreamId, uint64_t const & iDmac, uint64_t const & iSourceMac, bool const & iTxActive, IasAvbStreamState const & iRxStatus, bool const & iPreconfigured, IasAvbStreamDiagnostics const & iDiagnostics);

  /*!
   * @brief Assignment operator.
   */
  IasAvbClockReferenceStreamAttributes & operator = (IasAvbClockReferenceStreamAttributes const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (IasAvbClockReferenceStreamAttributes const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (IasAvbClockReferenceStreamAttributes const & iOther) const;

  /**
   * description: direction of the stream (to/from the network)
   */
  inline const IasAvbStreamDirection &getDirection() const { return direction; }
  inline void setDirection(const IasAvbStreamDirection &_value) { direction = _value; }
  /**
   * description: type of of the stream as defined in IEEE1722-rev1 11.2.6
   */
  inline const IasAvbClockReferenceStreamType &getType() const { return type; }
  inline void setType(const IasAvbClockReferenceStreamType &_value) { type = _value; }
  /**
   * description: number of CRF time stamps to be put into each PDU
   */
  inline const uint16_t &getCrfStampsPerPdu() const { return crfStampsPerPdu; }
  inline void setCrfStampsPerPdu(const uint16_t &_value) { crfStampsPerPdu = _value; }
  /**
   * description: number of clock events per time stamp
   */
  inline const uint16_t &getCrfStampInterval() const { return crfStampInterval; }
  inline void setCrfStampInterval(const uint16_t &_value) { crfStampInterval = _value; }
  /**
   * description: base frequency (events per second)
   */
  inline const uint32_t &getBaseFreq() const { return baseFreq; }
  inline void setBaseFreq(const uint32_t &_value) { baseFreq = _value; }
  /**
   * description: frequency modifier as defined in IEEE1722-rev1 11.2.8
   */
  inline const IasAvbClockMultiplier &getPull() const { return pull; }
  inline void setPull(const IasAvbClockMultiplier &_value) { pull = _value; }
  /**
   * description: Id of the clock domain to be used by the stream
   */
  inline const uint32_t &getClockId() const { return clockId; }
  inline void setClockId(const uint32_t &_value) { clockId = _value; }
  /**
   * description: controls the definition of streamId and destination MAC
   */
  inline const IasAvbIdAssignMode &getAssignMode() const { return assignMode; }
  inline void setAssignMode(const IasAvbIdAssignMode &_value) { assignMode = _value; }
  /**
   * description: streamId
   */
  inline const uint64_t &getStreamId() const { return streamId; }
  inline void setStreamId(const uint64_t &_value) { streamId = _value; }
  /**
   * description: destination MAC address
   */
  inline const uint64_t &getDmac() const { return dmac; }
  inline void setDmac(const uint64_t &_value) { dmac = _value; }
  /**
   * description: source MAC address of the talker
   */
  inline const uint64_t &getSourceMac() const { return sourceMac; }
  inline void setSourceMac(const uint64_t &_value) { sourceMac = _value; }
  /**
   * description: 'true' if stream is currently active (valid for transmit streams only)
   */
  inline const bool &getTxActive() const { return txActive; }
  inline void setTxActive(const bool _value) { txActive = _value; }
  /**
   * description: status of the stream (valid for receive streams only)
   */
  inline const IasAvbStreamState &getRxStatus() const { return rxStatus; }
  inline void setRxStatus(const IasAvbStreamState &_value) { rxStatus = _value; }
  /**
   * description: 'true' if stream has been created statically by configuration library during
   *   startup, 'false' if stream has been created dynamically during runtime
   */
  inline const bool &getPreconfigured() const { return preconfigured; }
  inline void setPreconfigured(const bool _value) { preconfigured = _value; }
  /**
   * description: stream diagnostics data
   */
  inline const IasAvbStreamDiagnostics &getDiagnostics() const { return diagnostics; }
  inline void setDiagnostics(const IasAvbStreamDiagnostics &_value) { diagnostics = _value; }

  /*!
   * direction of the stream (to/from the network)
   */
  IasAvbStreamDirection direction;
  /*!
   * type of of the stream as defined in IEEE1722-rev1 11.2.6
   */
  IasAvbClockReferenceStreamType type;
  /*!
   * number of CRF time stamps to be put into each PDU
   */
  uint16_t crfStampsPerPdu;
  /*!
   * number of clock events per time stamp
   */
  uint16_t crfStampInterval;
  /*!
   * base frequency (events per second)
   */
  uint32_t baseFreq;
  /*!
   * frequency modifier as defined in IEEE1722-rev1 11.2.8
   */
  IasAvbClockMultiplier pull;
  /*!
   * Id of the clock domain to be used by the stream
   */
  uint32_t clockId;
  /*!
   * controls the definition of streamId and destination MAC
   */
  IasAvbIdAssignMode assignMode;
  /*!
   * streamId
   */
  uint64_t streamId;
  /*!
   * destination MAC address
   */
  uint64_t dmac;
  /*!
   * source MAC address of the talker
   */
  uint64_t sourceMac;
  /*!
   * 'true' if stream is currently active (valid for transmit streams only)
   */
  bool txActive;
  /*!
   * status of the stream (valid for receive streams only)
   */
  IasAvbStreamState rxStatus;
  /*!
   * 'true' if stream has been created statically by configuration library during startup, 'false' if stream has been created dynamically during runtime
   */
  bool preconfigured;
  /*!
   * stream diagnostics data
   */
  IasAvbStreamDiagnostics diagnostics;
};

typedef std::vector<IasAvbAudioStreamAttributes> AudioStreamInfoList;
typedef std::vector<IasAvbVideoStreamAttributes> VideoStreamInfoList;
typedef std::vector<IasAvbClockReferenceStreamAttributes> ClockReferenceStreamInfoList;

/* const values are implemented through static inline getter functions in CommonAPI.
 * This breaks our table-driven configuration design, so we redeclare them as real constants
 * here.
 */
const uint32_t cIasAvbPtpClockDomainId = 0;
const uint32_t cIasAvbHwCaptureClockDomainId = 16;
const uint32_t cIasAvbRawClockDomainId = 33; // to avoid FIDL API change, we declare this only for the C++ API now.

} //namespace IasMediaTransportAvb

#endif /* IASAVBSTREAMHANDLERTYPES_HPP_ */
