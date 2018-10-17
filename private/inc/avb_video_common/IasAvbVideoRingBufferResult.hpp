/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoRingBufferResult.hpp
 * @brief
 * @details
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERRESULT_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERRESULT_HPP

namespace IasMediaTransportAvb {

enum IasVideoRingBufferResult
{
  eIasRingBuffOk = 0,             //!< Success
  eIasRingBuffInvalidParam,       //!< Invalid parameter
  eIasRingBuffNotInitialized,     //!< Not initialized
  eIasRingBuffNullPtrAccess,      //!< Null pointer has been accessed
  eIasRingBuffAlsaSuspendError,   //!< SUSPEND recovery failed
  eIasRingBuffNotAllowed,         //!< Not allowed here
  eIasRingBuffInvalidSampleSize,  //!< Invalid sample size
  eIasRingBuffTimeOut,            //!< timeout exceeded
  eIasRingBuffProbeError,         //!< Probe error
  eIasRingBuffCondWaitFailed,     //!< Cond wait failed
  eIasRingBuffTooManyReaders,     //!< Maximum readers limit exceeded
};

}

#endif /* IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEORINGBUFFERRESULT_HPP */
