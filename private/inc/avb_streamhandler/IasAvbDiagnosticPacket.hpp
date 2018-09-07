/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file    IasAvbDiagnosticPacket.hpp
 * @brief   The definition of the IasAvbDiagnosticPacket class
 * @details This is an AVB diagnostic packet inheriting from the AVB Packet
 * @date    2016
 */

#ifndef MEDIA_TRANSPORT_AVB_STREAMHANDLER_PRIVATE_INC_AVB_STREAMHANDLER_IASAVBDIAGNOSTICPACKET_HPP_
#define MEDIA_TRANSPORT_AVB_STREAMHANDLER_PRIVATE_INC_AVB_STREAMHANDLER_IASAVBDIAGNOSTICPACKET_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAvbPacket.hpp"

namespace IasMediaTransportAvb
{

class IasAvbStreamHandlerEnvironment;

class IasAvbDiagnosticPacket
{
  public:

    enum FieldOffset : uint32_t
    {
      DESTINATION_MAC = 0u,
      SOURCE_MAC = 6u,
      TPID = 12u,
      VLAN = 14u,
      ETHER_TYPE = 16u,
      SUBTYPE = 18u,
      SV_VERSION_CONTROL_DATA = 19u,
      STATUS_AND_CONTROL_DATA_LENGTH = 20u,
      TARGET_ENTITY_ID = 22u,
      CONTROLLER_ENTITY_ID = 30u,
      SEQUENCE_ID = 38u,
      COMMAND_TYPE = 40u,
      DESCRIPTOR_TYPE = 42u,
      DESCRIPTOR_INDEX = 44u,
      COUNTERS_VALID = 46u,
      LINK_UP_COUNT = 50u,
      LINK_DOWN_COUNT = 54u,
      FRAMES_TX_COUNT = 58u,
      FRAMES_RX_COUNT = 62u,
      RX_CRC_ERROR_COUNT = 66u,
      GPTP_GM_CHANGED = 70u,
      MESSAGE_TIMESTAMP = 146u,
      STATION_STATE_FIELD = 154u,
      STATION_STATE_DATA_FIELD = 155u,
      EMPTY_COUNTER = 158u
    };


    static const uint16_t cInterfaceDescriptorType = 0x0009;
    static const uint16_t cStreamOutputDescriptorType = 0x0006;
    static const uint16_t cStreamInputDescriptorType = 0x0005;

    // Not sure about this now.
    static const uint16_t cDescriptorIndex = 0x0000;

    static const uint8_t cEthernetReadyStationState = 0x01;
    static const uint8_t cAvbSyncStationState = 0x02;
    static const uint8_t cAvbMediaReadyStationState = 0x03;

    // Buffer size
    static const uint32_t cPacketLength = 178u;

    /**
     *  @brief Constructor
     */
    IasAvbDiagnosticPacket();

    /**
     *  @brief Desctructor
     */
    ~IasAvbDiagnosticPacket();

    /**
     * @brief Init - Populate all of the common fields of the diagnostic packet. These remain the same throughout lifetime of packet
     *
     * @returns eIasAvbProcOK if init successful, else relevant error.
     */
    IasAvbProcessingResult init();

    /**
     * @brief write field in network order to the packet
     *
     * @param[in] uint16_t/uint32_t/uint64_t field data
     * @param[in] data length
     * @param[in] field offset
     */
    void writeNetworkOrderField(const uint16_t data, FieldOffset fieldOffset);
    void writeNetworkOrderField(const uint32_t data, FieldOffset fieldOffset);
    void writeNetworkOrderField(const uint64_t data, FieldOffset fieldOffset);

    /**
     * @brief write field to the packet
     *
     * @param[in] void* field data
     * @param[in] data length
     * @param[in] field offset
     */
    void writeField(const void *pData, const uint32_t length, FieldOffset fieldOffset);

    /**
     * @brief write field to the packet
     *
     * @returns the underlying packet buffer
     */
    const uint8_t* getBuffer() const;

  private:

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbDiagnosticPacket(IasAvbDiagnosticPacket const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbDiagnosticPacket& operator=(IasAvbDiagnosticPacket const &other);

    uint8_t* mBuffer;
};


inline const uint8_t* IasAvbDiagnosticPacket::getBuffer() const
{
  return mBuffer;
}


} // namespace IasMediaTransportAvb

#endif /* MEDIA_TRANSPORT_AVB_STREAMHANDLER_PRIVATE_INC_AVB_STREAMHANDLER_IASAVBDIAGNOSTICPACKET_HPP_ */
