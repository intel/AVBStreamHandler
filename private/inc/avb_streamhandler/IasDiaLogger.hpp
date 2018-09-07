/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasDiaLogger.hpp
 * @brief   The definition of the IasDiaLogger class.
 * @details This is the base class for IasDiaLogger.
 * @date    2016
 */

#ifndef IASDIALOGGER_HPP_
#define IASDIALOGGER_HPP_

#include "IasAvbTypes.hpp"
#include "IasAvbDiagnosticPacket.hpp"

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>

namespace IasMediaTransportAvb
{

class IasDiaLogger
{
public:
  /**
   *  @brief Constructor.
   */
  IasDiaLogger();

  /**
   *  @brief Destructor, non virtual as final
   */
  ~IasDiaLogger();

  IasAvbProcessingResult init(IasAvbStreamHandlerEnvironment* environmentInstance);

  void cleanup();

  /**
   * @brief Increment the link down counter.
   */
  inline void incLinkDown();

  /**
   * @brief Increment the link up counter.
   */

  inline void incLinkUp();
  /**
   * @brief Increment the packet receive counter.
   */

  inline void incRxCount();

  /**
   * @brief Increment the packet transmit counter.
   */
  inline void incTxCount();

  /**
   * @brief Increment the packet sequence number.
   */
  inline void incSequenceNumber();

  /**
   * @brief Zero the receive counter.
   */
  inline void clearRxCount();

  /**
   * @brief Zero the transmit counter.
   */
  inline void clearTxCount();

  /**
   * @brief Update the packet timestamp field.
   */
  inline void setTimestampField(const uint64_t timestamp);

  /**
   * @brief Set the ethernet ready fields and send a packet.
   *
   * @param[in] socket on which to send the packet
   *
   * @returns void
   */
  void triggerEthReadyPacket(const int32_t socket);

  /**
   * @brief Set the avb sync fields and send a packet.
   *
   * @param[in] socket on which to send the packet
   *
   * @returns void
   */
  void triggerAvbSyncPacket(const int32_t socket);

  /**
   * @brief Set the listener media ready fields and send a packet.
   *
   * @param[in] socket on which to send the packet
   *
   * @returns void
   */
  void triggerListenerMediaReadyPacket(const int32_t socket);

  /**
   * @brief Set the talker media ready fields and send a packet.
   *
   * @param[in] socket on which to send the packet
   *
   * @returns void
   */
  void triggerTalkerMediaReadyPacket(const int32_t socket);

private:

  /**
   * @brief Copy constructor, private unimplemented to prevent misuse.
   */
  IasDiaLogger(IasDiaLogger const &other);

  /**
   * @brief Assignment operator, private unimplemented to prevent misuse.
   */
  IasDiaLogger& operator=(IasDiaLogger const &other);

  /**
   * @brief Sets the generic media ready fields and send a packet
   *
   * @param[in] socket on which to send the packet
   * @param[in] is it a talker or listener station
   *
   * @returns void
   */
  void triggerMediaReadyPacket(const int32_t socket, bool isTalker = false);

  /**
   * @brief Sets the diagnostic message
   *
   * @param[in] socket on which to send the packet
   *
   * @returns void
   */
  void sendDiagnosticMessage(const int32_t socket);

  IasAvbDiagnosticPacket* mDiagnosticPacket;

  struct sockaddr_ll mDestInfo;

  // AVNu Diagnostic Counters
  uint16_t            mSequenceNumber;
  uint32_t            mLinkUpCount;
  uint32_t            mLinkDownCount;
  uint32_t            mFramesTxCount;
  uint32_t            mFramesRxCount;
  uint32_t            mRxCrcErrorCount;
  uint32_t            mGptpGmChangedCount;
};


inline void IasDiaLogger::incLinkDown()
{
  ++mLinkDownCount;
}


inline void IasDiaLogger::incLinkUp()
{
  ++mLinkUpCount;
}


inline void IasDiaLogger::incRxCount()
{
  ++mFramesRxCount;
}


inline void IasDiaLogger::incTxCount()
{
  ++mFramesTxCount;
}


inline void IasDiaLogger::incSequenceNumber()
{
  ++mSequenceNumber;
}


inline void IasDiaLogger::clearTxCount()
{
  mFramesTxCount = 0;
}


inline void IasDiaLogger::clearRxCount()
{
  mFramesRxCount = 0;
}


inline void IasDiaLogger::setTimestampField(const uint64_t timestamp)
{
  if (NULL != mDiagnosticPacket)
  {
    mDiagnosticPacket->writeNetworkOrderField(timestamp, IasAvbDiagnosticPacket::MESSAGE_TIMESTAMP);
  }
}

} // namespace IasMediaTransportAvb

#endif /* IASDIALOGGER_HPP_ */

