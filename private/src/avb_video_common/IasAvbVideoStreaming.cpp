/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoStreaming.cpp
 * @brief   Implementation of the packet structures that are used in data transmission.
 * @details See header file for details.
 *
 * @date    2018
 */


#include "avb_video_common/IasAvbVideoStreaming.hpp"

/**
 * namespace IasMediaTransportAvb
 */
namespace IasMediaTransportAvb {

PacketH264::PacketH264()
  : buffer()
{
}

PacketH264::PacketH264(PacketH264 const & iOther)
  : buffer(iOther.buffer)
{
}

PacketH264::PacketH264(
    Buffer const & iBuffer)
  : buffer(iBuffer)
{
}

PacketH264 & PacketH264::operator = (PacketH264 const & iOther)
{
  if (this != &iOther)
  {
    buffer = iOther.buffer;
  }

  return *this;
}

bool PacketH264::operator == (PacketH264 const & iOther) const
{
  return (this == &iOther);
      // TODO: == operator is not complete!
//      || (   (buffer == iOther.buffer));
}

bool PacketH264::operator != (PacketH264 const & iOther) const
{
  return !(*this == iOther);
}


PacketMpegTS::PacketMpegTS()
  : sph(0)
  , buffer()
{
}

PacketMpegTS::PacketMpegTS(PacketMpegTS const & iOther)
  : sph(iOther.sph)
  , buffer(iOther.buffer)
{
}

PacketMpegTS::PacketMpegTS(
    bool const & iSph,
    Buffer const & iBuffer)
  : sph(iSph)
  , buffer(iBuffer)
{
}

PacketMpegTS & PacketMpegTS::operator = (PacketMpegTS const & iOther)
{
  if (this != &iOther)
  {
    sph = iOther.sph;
    buffer = iOther.buffer;
  }

  return *this;
}

bool PacketMpegTS::operator == (PacketMpegTS const & iOther) const
{
  return (this == &iOther);
      // TODO: == operator is not complete!
//      || (   (sph == iOther.sph)
//          && (buffer == iOther.buffer));
}

bool PacketMpegTS::operator != (PacketMpegTS const & iOther) const
{
  return !(*this == iOther);
}

} // namespace IasMediaTransportAvb

