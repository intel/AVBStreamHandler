/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasDiaLogger.cpp
 * @brief   Implementation of a custom avb data logging tool.
 * @date    2016
 */

#include "avb_streamhandler/IasDiaLogger.hpp"
#include "avb_streamhandler/IasAvbDiagnosticPacket.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#include "avb_helper/ias_safe.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <cstring>

#ifndef ETH_P_IEEE1722
#define ETH_P_IEEE1722 0x22F0
#endif

namespace IasMediaTransportAvb {

IasDiaLogger::IasDiaLogger()
  : mDiagnosticPacket(NULL)
  , mSequenceNumber(0u)
  , mLinkUpCount(0u)
  , mLinkDownCount(0u)
  , mFramesTxCount(0u)
  , mFramesRxCount(0u)
  , mRxCrcErrorCount(0u)
  , mGptpGmChangedCount(0u)
{
}


IasDiaLogger::~IasDiaLogger()
{
  cleanup();
}


IasAvbProcessingResult IasDiaLogger::init(IasAvbStreamHandlerEnvironment* environmentInstance)
{
  IasAvbProcessingResult ret = eIasAvbProcOK;
  mDiagnosticPacket = new (nothrow) IasAvbDiagnosticPacket();
  if (NULL == mDiagnosticPacket)
  {
    ret = eIasAvbProcNotEnoughMemory;
  }
  else
  {
    ret = mDiagnosticPacket->init();
  }

  if (ret == eIasAvbProcOK)
  {
    struct ifreq ifr;
    const std::string* interfaceName = IasAvbStreamHandlerEnvironment::getNetworkInterfaceName();
    if (NULL == interfaceName)
    {
      ret = eIasAvbProcInitializationFailed;
    }
    else
    {
      strncpy(ifr.ifr_name, interfaceName->data(), sizeof(ifr.ifr_name) - 1u);
      ifr.ifr_name[sizeof(ifr.ifr_name) -1u] = '\0';

      // Get index of interface for socket address info.
      const int32_t *socket = environmentInstance->getStatusSocket();
      if ((NULL == socket) || (ioctl(*socket, SIOCGIFINDEX, &ifr) < 0))
      {
        ret = eIasAvbProcInitializationFailed;
      }
      else
      {
        mDestInfo.sll_family = PF_PACKET;
        mDestInfo.sll_pkttype = PACKET_BROADCAST;
        // ETH_P_ALL
        mDestInfo.sll_protocol = htons(0x0003);
        mDestInfo.sll_ifindex = ifr.ifr_ifindex;
        mDestInfo.sll_halen = cIasAvbMacAddressLength;
      }
    }
  }

  return ret;
}


void IasDiaLogger::cleanup()
{
  if (NULL != mDiagnosticPacket)
  {
    delete mDiagnosticPacket;
    mDiagnosticPacket = NULL;
  }
}


void IasDiaLogger::triggerEthReadyPacket(const int32_t socket)
{
  ++mLinkUpCount;

  // Descriptor type
  mDiagnosticPacket->writeNetworkOrderField(IasAvbDiagnosticPacket::cInterfaceDescriptorType, IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_TYPE);

  // Descriptor index - where do I get this from?
  mDiagnosticPacket->writeField(&IasAvbDiagnosticPacket::cDescriptorIndex,
    sizeof(IasAvbDiagnosticPacket::cDescriptorIndex), IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_INDEX);

  // Station state
  mDiagnosticPacket->writeField(&IasAvbDiagnosticPacket::cEthernetReadyStationState,
    sizeof(uint8_t), IasAvbDiagnosticPacket::FieldOffset::STATION_STATE_FIELD);

  sendDiagnosticMessage(socket);
  return;
}


void IasDiaLogger::triggerAvbSyncPacket(const int32_t socket)
{
  // Descriptor type
  mDiagnosticPacket->writeNetworkOrderField(IasAvbDiagnosticPacket::cInterfaceDescriptorType, IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_TYPE);

  // Descriptor index - where do I get this from?
  mDiagnosticPacket->writeField(&IasAvbDiagnosticPacket::cDescriptorIndex,
    sizeof(IasAvbDiagnosticPacket::cDescriptorIndex), IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_INDEX);

  // Station state
  mDiagnosticPacket->writeField(&IasAvbDiagnosticPacket::cAvbSyncStationState,
    sizeof(uint8_t), IasAvbDiagnosticPacket::FieldOffset::STATION_STATE_FIELD);

  sendDiagnosticMessage(socket);
  return;
}


void IasDiaLogger::triggerListenerMediaReadyPacket(const int32_t socket)
{
  triggerMediaReadyPacket(socket);
  return;
}


void IasDiaLogger::triggerTalkerMediaReadyPacket(const int32_t socket)
{
  triggerMediaReadyPacket(socket, true);
  return;
}


void IasDiaLogger::triggerMediaReadyPacket(const int32_t socket, bool isTalker)
{
  // Descriptor type

  uint16_t descriptorType = (isTalker ? IasAvbDiagnosticPacket::cStreamOutputDescriptorType
    : IasAvbDiagnosticPacket::cStreamInputDescriptorType);

  mDiagnosticPacket->writeNetworkOrderField(descriptorType, IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_TYPE);

  // Descriptor index - where do I get this from?
  mDiagnosticPacket->writeField(&IasAvbDiagnosticPacket::cDescriptorIndex,
    sizeof(IasAvbDiagnosticPacket::cDescriptorIndex), IasAvbDiagnosticPacket::FieldOffset::DESCRIPTOR_INDEX);

  // Station state
  mDiagnosticPacket->writeField(&IasAvbDiagnosticPacket::cAvbMediaReadyStationState,
    sizeof(uint8_t), IasAvbDiagnosticPacket::FieldOffset::STATION_STATE_FIELD);

  sendDiagnosticMessage(socket);
  return;
}


void IasDiaLogger::sendDiagnosticMessage(const int32_t socket)
{
  ++mSequenceNumber;
  if ((!IasAvbStreamHandlerEnvironment::isTestProfileEnabled()) || (NULL == mDiagnosticPacket))
  {
    return;
  }

  // Fields updated per packet
  // Packet Sequence Number
  mDiagnosticPacket->writeNetworkOrderField(mSequenceNumber, IasAvbDiagnosticPacket::FieldOffset::SEQUENCE_ID);

  // Counters
  // Link Up Counter
  mDiagnosticPacket->writeNetworkOrderField(mLinkUpCount, IasAvbDiagnosticPacket::FieldOffset::LINK_UP_COUNT);

  // Link Down Counter
  mDiagnosticPacket->writeNetworkOrderField(mLinkDownCount, IasAvbDiagnosticPacket::FieldOffset::LINK_DOWN_COUNT);

  // Tx Frame Counter
  mDiagnosticPacket->writeNetworkOrderField(mFramesTxCount, IasAvbDiagnosticPacket::FieldOffset::FRAMES_TX_COUNT);

  // Rx Frame Counter
  mDiagnosticPacket->writeNetworkOrderField(mFramesRxCount, IasAvbDiagnosticPacket::FieldOffset::FRAMES_RX_COUNT);

  mDiagnosticPacket->writeNetworkOrderField(mRxCrcErrorCount, IasAvbDiagnosticPacket::FieldOffset::RX_CRC_ERROR_COUNT);

  mDiagnosticPacket->writeNetworkOrderField(mGptpGmChangedCount, IasAvbDiagnosticPacket::FieldOffset::GPTP_GM_CHANGED);

  // Don't know what form this is going to take, length 24
  uint8_t stateData[3] = {0x00, 0x00, 0x00};
  mDiagnosticPacket->writeField(&stateData, sizeof(stateData),
    IasAvbDiagnosticPacket::FieldOffset::STATION_STATE_DATA_FIELD);

  // Continue without error on fail.
  const uint8_t* buffer = mDiagnosticPacket->getBuffer();
  sendto(socket, buffer, IasAvbDiagnosticPacket::cPacketLength, 0,
    (struct sockaddr *)&mDestInfo, sizeof(struct sockaddr_ll));

  return;
}


} // namespace Ias
