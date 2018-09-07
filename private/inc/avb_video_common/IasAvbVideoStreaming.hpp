/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoStreaming.hpp
 * @brief   Definition of the packet structs that are used in video data transmission.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOSTREAMING_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOSTREAMING_HPP


#include "avb_streamhandler/IasAvbTypes.hpp"
using IasMediaTransportAvb::Buffer;

/**
 * namespace IasMediaTransportAvb
 */
namespace IasMediaTransportAvb {

struct PacketH264
{
  /*!
   * @brief Default constructor.
   */
  PacketH264();

  /*!
   * @brief Copy constructor.
   */
  PacketH264(PacketH264 const & iOther);

  /*!
   * @brief Field constructor.
   *
   * @param[in] iiBuffer Initial value for field PacketH264.
   */
  PacketH264(Buffer const & iBuffer);

  /*!
   * @brief Assignment operator.
   */
  PacketH264 & operator = (PacketH264 const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (PacketH264 const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (PacketH264 const & iOther) const;

  /*!
   * contains one RTP packet related to RFC6184
   */
  Buffer buffer;
};


struct PacketMpegTS
{
  /*!
   * @brief Default constructor.
   */
  PacketMpegTS();

  /*!
   * @brief Copy constructor.
   */
  PacketMpegTS(PacketMpegTS const & iOther);

  /*!
   * @brief Field constructor.
   *
   * @param[in] iiSph Initial value for field PacketMpegTS.
   * @param[in] iiBuffer Initial value for field PacketMpegTS.
   */
  PacketMpegTS(bool const & iSph, Buffer const & iBuffer);

  /*!
   * @brief Assignment operator.
   */
  PacketMpegTS & operator = (PacketMpegTS const & iOther);

  /*!
   * @brief Equality comparison operator.
   */
  bool operator == (PacketMpegTS const & iOther) const;

  /*!
   * @brief Inequality comparison operator.
   */
  bool operator != (PacketMpegTS const & iOther) const;

  /*!
   * shows if a source packet header (sph) is used
   */
  bool sph;

  /*!
   * contains a number of TS packets
   */
  Buffer buffer;

};


struct TransferPacketH264
{
    size_t size;
    uint8_t data;
};

struct TransferPacketMpegTS
{
    bool sph;
    size_t size;
    uint8_t data;
};


} // namespace IasAvbStreaming


#endif // IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBVIDEOSTREAMING_HPP
