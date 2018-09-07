/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbTypes.hpp
 * @brief   The definition of generic AVB types.
 * @date    2013
 */

#ifndef IASAVBTYPES_HPP_
#define IASAVBTYPES_HPP_

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "avb_helper/ias_visibility.h"
#include "avb_helper/ias_debug.h"

#include <sys/types.h> // need POSIX types for igb
#include <inttypes.h>   // for format string macros
#include <new> // for nothrow variant of new operator

namespace IasMediaTransportAvb {

using std::nothrow;

/**
 * @brief Declaration of the Buffer.
 *
 * A buffer contains a pointer to some data and the size of the data.
 * The user of this type has to take care on the memory management on ones own.
 */
struct IAS_DSO_PUBLIC Buffer
{
    /**
     * Size of the data.
     */
    uint32_t size;
    /**
     * Pointer to the data.
     */
    void * data;

    /**
     *  @brief Default constructor for the buffer.
     *
     *  The constructor does not allocate any memory.
     */
    Buffer()
    {
      size = 0u;
      data = NULL;
    }

    /**
     *  @brief Destructor for the buffer.
     *  The destructor does not free any memory.
     */
    ~Buffer()
    {
      size = 0u;
      data = NULL;
    }
};

/**
 * @brief Result values used by the AVB components.
 */
enum IasAvbProcessingResult
{
  eIasAvbProcOK                    = 0,        ///< no error
  eIasAvbProcErr                   = 1,        ///< general error
  eIasAvbProcInvalidParam          = 2,        ///< bad argument
  eIasAvbProcOff                   = 3,        ///< operation not permitted while component is not active
  eIasAvbProcInitializationFailed  = 4,        ///< error during initialization (to be returned by init() only)
  eIasAvbProcNotInitialized        = 5,        ///< operation not permitted before component has been initialized
  eIasAvbProcNoSpaceLeft           = 6,        ///< no space left on disk, file sys, etc.
  eIasAvbProcNotEnoughMemory       = 7,        ///< out of memory (new/malloc/realloc fail, etc.)
  eIasAvbProcAlreadyInUse          = 8,        ///< resource busy
  eIasAvbProcCallbackError         = 9,        ///< ?
  eIasAvbProcUnsupportedFormat     = 10,       ///< support for this data format is not implemented (not POR)
  eIasAvbProcNotImplemented        = 11,       ///< support for this function is not yet implemented (but still needs to be done!)
  eIasAvbProcThreadStartFailed     = 12,       ///< thread could not be started. Maybe a memory issue?
  eIasAvbProcThreadStopFailed      = 13,       ///< thread could not be stopped.
  eIasAvbProcNullPointerAccess     = 14,       ///< there was an access to an uninitialized pointer.
  eIasAvbProcTimeout               = 15,       ///< timeout has occurred.
};

/**
 * @brief Type information for the various AVB stream types. Yet to grow...
 * This type information is needed for user input validation
 * when looking up a stream object by its ID.
 */
enum IasAvbStreamType
{
  eIasAvbAudioStream               = 0,        ///< type id for audio AVB streams
  eIasAvbVideoStream               = 1,        ///< type id for video AVB streams
  eIasAvbClockReferenceStream      = 2,        ///< type id for AVB clock reference streams
};

/**
 * @brief Type information for the various local stream types.
 * This type information is needed for user input validation
 * when looking up a local stream object by its ID, as well as
 * optimizing the ID lookup procedure.
 */
enum IasLocalStreamType
{
  eIasReservedStream               = 0,        ///< Deprecated and no longer supported stream type
  eIasAlsaStream                   = 1,        ///< type id for local stream to/from ALSA interface
  eIasTestToneStream               = 2,        ///< type id for local stream from test tone generator
  eIasLocalVideoInStream           = 3,        ///< type id for video receive stream
  eIasLocalVideoOutStream          = 4,        ///< type id for video transmit stream
};

/**
 * @brief Type information for the clock domain types.
 * This type information is needed for user input validation
 * when looking up a clock domain object by its ID.
 */
enum IasAvbClockDomainType
{
  eIasAvbClockDomainPtp            = 0,        ///< type id for PTP (802.1AS) time domain
  eIasAvbClockDomainHw             = 1,        ///< type id for hardware-based time domain(s)
  eIasAvbClockDomainSw             = 2,        ///< type id for software-based time domain(s)
  eIasAvbClockDomainRx             = 3,        ///< type id for time domains derived from the time stamps of received AVB streams
  eIasAvbClockDomainRaw            = 4,        ///< type id for time domain synchronous to CLOCK_MONOTONIC_RAW
  eIasAVbClockDomainAlsa           = 5,        ///< type id for ALSA HW device time domain // ABu: ToDo: will be used for XTS ALSA2PTP
};

/**
 * @brief Type information for compatibility mode.
 * This type is used to handle the support of multiple
 * 1722 draft versions.
 */
enum IasAvbCompatibility
{
  eIasAvbCompLatest                = 0,        ///< type if for the latest draft
  eIasAvbCompSaf                   = 1,        ///< type for early draft used by old lab test devices
  eIasAvbCompD6                    = 2,        ///< type for 1722a draft 6
};


static const uint32_t cIasAvbMacAddressLength = 6u;
typedef uint8_t IasAvbMacAddress[cIasAvbMacAddressLength];

// absolute maximum number of channels for any audio stream: (ETH_DATA_LEN - AVTP Header (24)) / size of SAF16 type (2)
static const uint16_t cIasAvbMaxNumChannels = 738u;


} //namespace IasMediaTransportAvb

#endif /* IASAVBTYPES_HPP_ */
