/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file   IasVideoCommonTypes.hpp
 * @brief  Common Types for all video entities
 *
 * @date   2018
 *
 */
#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOCOMMONTYPES_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOCOMMONTYPES_HPP


/**
 * @brief Namespace for all video related definitions and declarations
 */
namespace IasMediaTransportAvb {

#ifndef ARRAYLEN
#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))
#endif


/**
  * @brief Ring buffer access type (read or write access).
  */
enum IasRingBufferAccess
{
  eIasRingBufferAccessUndef = 0,  //!< Undefined
  eIasRingBufferAccessRead,       //!< Read access
  eIasRingBufferAccessWrite       //!< Write access
};


/**
 * @brief Common Result type of the video domain.
 * Result state of functions can be one of these values.
 *
 */
enum IasVideoCommonResult
{
  eIasResultOk = 0,
  eIasResultFailed,
  eIasResultInitFailed,
  eIasResultInvalidParam,
  eIasResultNotInitialized,
  eIasResultMemoryError,
  eIasResultTimeOut,
  eIasResultCondWaitMutexOwn,
  eIasResultCondWaitParam,
  eIasResultUnknown,
  eIasResultObjectNotFound,
  eIasResultClockDomainRate,
  eIasResultInvalidSampleSize,
  eIasResultObjectAlreadyExists,
  eIasResultInvalidShmPath,
  eIasResultBufferEmpty,
  eIasResultBufferFull,
  eIasResultAlreadyInitialized,
  eIasResultNotAllowed,
  eIasResultCRCError,
  eIasResultInvalidSegmentSize,
  eIasResultSinkAlreadyConnected,
  eIasResultConnectionAlreadyExists
};

} // Namespace IasMediaTransportAvb


#endif // IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOCOMMONTYPES_HPP
