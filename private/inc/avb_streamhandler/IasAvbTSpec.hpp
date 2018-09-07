/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbTSpec.hpp
 * @brief   The definition of the IasAvbTSpec class.
 * @details Represents a IEEE802.1Q traffic specification.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBSTSPEC_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBSTSPEC_HPP

#include "IasAvbTypes.hpp"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"

namespace IasMediaTransportAvb {


class IasAvbTSpec
{
  public:
    /*
     * Every Layer 2 Ethernet frame has 18 bytes before the 1722 frame starts (DMAC,SMAC,VLAN,EthType).
     * Note: Actually it's rather used as an offset than an overhead in the implementation of the stream classes!
     */
    static const uint16_t cIasAvbPerFrameOverhead = 6u + 6u + 4u + 2u; // (== 18)

    /*
     * Minimum length of an Layer 2 Ethernet frame is 64 bytes according to IEEE 802.3 spec.
     */
    static const uint16_t cIasAvbEthernetMinFrameLen = 64u;

    /*
     * Minimum payload size within an Ethernet frame is 42 bytes (since VLAN Tag is always present)
     * Minimum frame length - DMAC - SMAC- VLAN - type - CRC
     */
    static const uint16_t cIasAvbEthernetMinPayloadSize = cIasAvbEthernetMinFrameLen - 6u -6u -4u - 2u - 4u;// (== 42);

    /*
     * Every Layer 1 Ethernet packet has an overall overhead of 42 bytes around the 1722 frame (PRE,SFD,DMAC,SMAC,VLAN,
     * EthType,CRC,IPG)
     */
    static const uint16_t cIasAvbEthernetPerPacketOverhead = 7u + 1u + cIasAvbPerFrameOverhead + 4u + 12u; // (== 42)

    /*
     * 1 byte SRP reservation overhead (not part of the actual packet being sent but needed for bandwidth calculation)
     */
    static const uint16_t cIasAvbSrpOverhead = 1u;

    /*
     * Number of 802.1Q SR classes that can be supported simultaneously
     */
    static const uint32_t cIasAvbNumSupportedClasses = 2u;

    /**
     *  @brief Constructor for standard streams
     */
    inline IasAvbTSpec(const uint16_t maxFrameSize, const IasAvbSrClass srClass, const uint16_t maxIntervalFrames = 1u);

    /**
     *  @brief empty Destructor
     */
    ~IasAvbTSpec() {}

    // Note: let the compiler-generated assignment op & copy ctor apply

    inline uint16_t    getMaxIntervalFrames() const;
    inline void setMaxIntervalFrames(const uint16_t maxIntervalFrames);

    inline uint16_t    getMaxFrameSize() const;
    inline IasAvbSrClass  getClass() const;
    inline uint32_t    getPacketsPerSecond() const;
    inline uint8_t     getVlanPriority() const;
    inline uint16_t    getVlanId() const;
    inline uint32_t    getPresentationTimeOffset() const;
    inline uint32_t    getRequiredBandwidth() const;
    inline uint32_t    getMaxTransitTime() const;

    inline static uint32_t getPacketsPerSecondByClass(IasAvbSrClass cl);
    inline static uint8_t getVlanPrioritybyClass(IasAvbSrClass cl);
    inline static uint16_t getVlanIdbyClass(IasAvbSrClass cl);
    inline static const char* getClassSuffix(IasAvbSrClass cl);

  private:
    friend class IasAvbStreamHandler;

    /**
     * @brief initialize tables, using registry
     */
    static void initTables();

    //
    // Members
    //
    const uint16_t    mMaxFrameSize;
    const IasAvbSrClass  mClass;
    uint16_t          mMaxIntervalFrames;

    static uint8_t mPrioTable[cIasAvbNumSupportedClasses];
    static uint8_t mIdTable[cIasAvbNumSupportedClasses];
    static uint32_t mClassMeasurementTimeTable[cIasAvbNumSupportedClasses];
    static uint32_t mPresentationTimeOffsetTable[cIasAvbNumSupportedClasses];
};


inline IasAvbTSpec::IasAvbTSpec(const uint16_t maxFrameSize, const IasAvbSrClass cl, const uint16_t maxIntervalFrames)
  : mMaxFrameSize( maxFrameSize )
  , mClass( IasAvbSrClass(cl) )
  , mMaxIntervalFrames( maxIntervalFrames )
{
  // nothing to do
}


inline uint16_t IasAvbTSpec::getMaxIntervalFrames() const
{
  return mMaxIntervalFrames;
}


inline void IasAvbTSpec::setMaxIntervalFrames(const uint16_t maxIntervalFrames)
{
  mMaxIntervalFrames = maxIntervalFrames;
}


inline uint16_t IasAvbTSpec::getMaxFrameSize() const
{
  return mMaxFrameSize;
}


inline IasAvbSrClass IasAvbTSpec::getClass() const
{
  return mClass;
}

inline uint32_t IasAvbTSpec::getMaxTransitTime() const
{
  return getPresentationTimeOffset() + mClassMeasurementTimeTable[mClass];
}


inline uint32_t IasAvbTSpec::getPacketsPerSecond() const
{
  return mMaxIntervalFrames * getPacketsPerSecondByClass(mClass);
}

inline uint32_t IasAvbTSpec::getPacketsPerSecondByClass(IasAvbSrClass cl)
{
  uint32_t ret = 0u;

  if (static_cast<uint32_t>(cl) < cIasAvbNumSupportedClasses)
  {
    uint32_t interval = mClassMeasurementTimeTable[cl];
    if (interval > 0u)
    {
      ret = uint32_t(uint64_t(1e9) / interval);
    }
  }

  return ret;
}

inline uint8_t IasAvbTSpec::getVlanPrioritybyClass(IasAvbSrClass cl)
{
  uint8_t ret = 0u;
  if (static_cast<uint32_t>(cl) < cIasAvbNumSupportedClasses)
  {
    ret = mPrioTable[cl];
  }
  return ret;
}

inline uint16_t IasAvbTSpec::getVlanIdbyClass(IasAvbSrClass cl)
{
  uint16_t ret = 0u;
  if (static_cast<uint32_t>(cl) < cIasAvbNumSupportedClasses)
  {
    ret = mIdTable[cl];
  }
  return ret;
}


inline uint8_t IasAvbTSpec::getVlanPriority() const
{
  AVB_ASSERT(static_cast<uint32_t>(mClass) < cIasAvbNumSupportedClasses);
  return mPrioTable[mClass];
}


inline uint16_t IasAvbTSpec::getVlanId() const
{
  AVB_ASSERT(static_cast<uint32_t>(mClass) < cIasAvbNumSupportedClasses);
  return mIdTable[mClass];
}


inline uint32_t IasAvbTSpec::getPresentationTimeOffset() const
{
  AVB_ASSERT(static_cast<uint32_t>(mClass) < cIasAvbNumSupportedClasses);
  return mPresentationTimeOffsetTable[mClass];
}

inline uint32_t IasAvbTSpec::getRequiredBandwidth() const
{
  /** return value in kBit/s, also consider total overhead of
   * 8 bytes preamble + SFD, 64 bytes Ethernet frame, 12 bytes IPG = 84 bytes
   * 1 byte SRP reservation overhead (not part of the actual packet being sent)
   * Also consider minimum frame length of 64 bytes (min payload == 42 if VLAN Tag is present)
   */
  const uint16_t payload = mMaxFrameSize < cIasAvbEthernetMinPayloadSize ? cIasAvbEthernetMinPayloadSize : mMaxFrameSize;
  return ((payload + cIasAvbEthernetPerPacketOverhead + cIasAvbSrpOverhead) * getPacketsPerSecond() * 8u) / 1000u;
}

inline const char* IasAvbTSpec::getClassSuffix(IasAvbSrClass cl)
{
  const char * ret = NULL;
  switch (cl)
  {
  case IasAvbSrClass::eIasAvbSrClassHigh:
    ret = "high";
    break;
  case IasAvbSrClass::eIasAvbSrClassLow:
    ret = "low";
    break;
  default:
    ret = "<UNKNOWN>";
    break;
  }

  return ret;
}


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBSTSPEC_HPP */
