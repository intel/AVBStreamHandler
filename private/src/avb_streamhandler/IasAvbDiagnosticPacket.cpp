/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbDiagnosticPacket.cpp
 * @brief   The implementation of the IasAvbDiagnosticPacket class.
 * @date    2016
 */

#include "avb_streamhandler/IasAvbDiagnosticPacket.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/ias_safe.h"

#include <arpa/inet.h>

// ntohll not defined in in.h, so we have to create something ourself
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define cond_swap64(x) __bswap_64(x)
#else
#define cond_swap64(x) (x)
#endif

namespace IasMediaTransportAvb
{

// Definitions
const uint16_t IasAvbDiagnosticPacket::cInterfaceDescriptorType;
const uint16_t IasAvbDiagnosticPacket::cStreamOutputDescriptorType;
const uint16_t IasAvbDiagnosticPacket::cStreamInputDescriptorType;
const uint16_t IasAvbDiagnosticPacket::cDescriptorIndex;
const uint8_t IasAvbDiagnosticPacket::cEthernetReadyStationState;
const uint8_t IasAvbDiagnosticPacket::cAvbSyncStationState;
const uint8_t IasAvbDiagnosticPacket::cAvbMediaReadyStationState;

IasAvbDiagnosticPacket::IasAvbDiagnosticPacket()
  : mBuffer(NULL)
{
}


IasAvbDiagnosticPacket::~IasAvbDiagnosticPacket()
{
  delete[] mBuffer;
  mBuffer = NULL;
}


IasAvbProcessingResult IasAvbDiagnosticPacket::init()
{
  mBuffer = new (nothrow) uint8_t[cPacketLength];
  if (NULL != mBuffer)
  {
    // 802.3 Header

    // Destination Mac for broadcast is 01:1B:C5:0A:C0:00
    uint64_t dMac = 0x011BC50AC000;
    (void) IasAvbStreamHandlerEnvironment::getConfigValue(IasRegKeys::cDiagnosticPacketDmac, dMac);

    IasAvbMacAddress mac;
    for (uint32_t i = 0u; i < cIasAvbMacAddressLength; i++)
    {
      mac[i] = uint8_t(dMac >> ((cIasAvbMacAddressLength - i - 1) * 8));
    }

    writeField(mac, cIasAvbMacAddressLength,
      IasAvbDiagnosticPacket::FieldOffset::DESTINATION_MAC);

    // Note, if reversal is required, 64bit swap will not work on this.
    const IasAvbMacAddress* sMac = IasAvbStreamHandlerEnvironment::getSourceMac();
    writeField(&sMac, cIasAvbMacAddressLength,
      IasAvbDiagnosticPacket::FieldOffset::SOURCE_MAC);

    uint16_t tpid = 0x8100u;
    writeNetworkOrderField(tpid, IasAvbDiagnosticPacket::FieldOffset::TPID);

    // Null
    uint16_t vlan = 0x0000;
    writeField(&vlan, sizeof(vlan),
      IasAvbDiagnosticPacket::FieldOffset::VLAN);

    uint16_t ethertype = 0x22F0;
    writeNetworkOrderField(ethertype, IasAvbDiagnosticPacket::FieldOffset::ETHER_TYPE);

    // Payload
    uint8_t subtype = 0xFB;
    writeField(&subtype, sizeof(subtype),
      IasAvbDiagnosticPacket::FieldOffset::SUBTYPE);

    uint8_t version = 0x01u; // SV_VER_MR_RS_GV_TV
    writeField(&version, sizeof(version),
      IasAvbDiagnosticPacket::FieldOffset::SV_VERSION_CONTROL_DATA);

    // Both status and control data length (148)
    uint16_t controlDataLength = 0x0095;
    writeNetworkOrderField(controlDataLength, IasAvbDiagnosticPacket::FieldOffset::STATUS_AND_CONTROL_DATA_LENGTH);

    uint64_t targetId = 0x0000000000000000;
    if (NULL != sMac)
    {
      for (int i = 0; i < 3; ++i)
      {
        targetId += *sMac[i];
        targetId = targetId << 8;
      }

      targetId = targetId << 8;
      targetId += 0xFFFE;

      for (int i = 3; i < 6; ++i)
      {
        targetId = targetId << 8;
        targetId += *sMac[i];
      }
    }

    writeField(&targetId, sizeof(targetId),
      IasAvbDiagnosticPacket::FieldOffset::TARGET_ENTITY_ID);

    uint64_t controllerId = 0x0000000000000000;
    writeField(&controllerId, sizeof(controllerId),
      IasAvbDiagnosticPacket::FieldOffset::CONTROLLER_ENTITY_ID);

    // Last bit in first octet set to 1 according to spec.
    uint16_t commandType = 0x8029;
    writeNetworkOrderField(commandType, IasAvbDiagnosticPacket::FieldOffset::COMMAND_TYPE);

    // Bitmask representing the valid counters
    // Could be a bit smarter, set the botton four bits for counters 0 - 3
    uint32_t countersValid = 0x0700000F;
    writeNetworkOrderField(countersValid, IasAvbDiagnosticPacket::FieldOffset::COUNTERS_VALID);

    // Final counters are unused and should be null
    uint32_t emptyCounter[5] =
    {
      0x00000000u,
      0x00000000u,
      0x00000000u,
      0x00000000u,
      0x00000000u
    };

    writeField(&emptyCounter, sizeof(emptyCounter),
      IasAvbDiagnosticPacket::FieldOffset::STATION_STATE_DATA_FIELD);
  }
  else
  {
    delete[] mBuffer;
    return eIasAvbProcNotEnoughMemory;
  }

  return eIasAvbProcOK;
}


void IasAvbDiagnosticPacket::writeNetworkOrderField(const uint16_t data, FieldOffset fieldOffset)
{
  uint16_t networkOrder = htons(data);
  writeField(&networkOrder, sizeof(uint16_t), fieldOffset);
}


void IasAvbDiagnosticPacket::writeNetworkOrderField(const uint32_t data, FieldOffset fieldOffset)
{
  uint32_t networkOrder = htonl(data);
  writeField(&networkOrder, sizeof(uint32_t), fieldOffset);
}


void IasAvbDiagnosticPacket::writeNetworkOrderField(const uint64_t data, FieldOffset fieldOffset)
{
  uint64_t networkOrder = cond_swap64(data);
  writeField(&networkOrder, sizeof(uint64_t), fieldOffset);
}


void IasAvbDiagnosticPacket::writeField(const void* pData, const uint32_t length, FieldOffset fieldOffset)
{
  avb_safe_result copyResult = avb_safe_memcpy(mBuffer + fieldOffset, length, pData, length);
  AVB_ASSERT(e_avb_safe_result_ok == copyResult);
  (void)copyResult;
}

} // IasMediaTransportAvb

