/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbRxStreamClockDomain.hpp
 * @brief   The definition of the IasAvbRxStreamClockDomain class.
 * @details This is the specialized clock domain for receive streams.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBRXSTREAMCLOCKDOMAIN_H
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBRXSTREAMCLOCKDOMAIN_H

#include "IasAvbTypes.hpp"
#include "IasAvbSwClockDomain.hpp"
#include "IasAvbTSpec.hpp"

namespace IasMediaTransportAvb {


class IasAvbRxStreamClockDomain : public IasAvbClockDomain
{
  public:
    /**
     *  @brief Constructor
     */
    IasAvbRxStreamClockDomain();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbRxStreamClockDomain();

    virtual void reset(IasAvbSrClass cl, uint32_t timestamp, uint32_t eventRate);
    virtual void reset(IasAvbSrClass cl, uint64_t timestamp, uint32_t eventRate);

    virtual void update(uint64_t events, uint32_t timestamp, uint32_t deltaMediaClock, uint32_t deltaWallClock);
    virtual void update(uint64_t events, uint64_t timestamp, uint32_t deltaMediaClock, uint64_t deltaWallClock);

    virtual void invalidate();

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbRxStreamClockDomain(IasAvbRxStreamClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbRxStreamClockDomain& operator=(IasAvbRxStreamClockDomain const &other);

    // Helpers
    void initLastTimeStamp(uint32_t timestamp);

    // Member variables

    std::string              mInstanceName;
    IasLibPtpDaemon         *mPtpProxy;
    IasAvbSwClockDomain      mPacketRate;
    double                   mFactorLong;
    double                   mFactorUnlock;
    uint32_t                 mThreshold1;
    uint32_t                 mThreshold2;
    uint64_t                 mLastTimeStamp;
    uint32_t                 mEpoch;
};


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_IASAVBRXSTREAMCLOCKDOMAIN_H */
