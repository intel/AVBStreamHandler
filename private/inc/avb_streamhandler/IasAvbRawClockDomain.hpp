/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbRawClockDomain.hpp
 * @brief   The definition of the IasAvbRawClockDomain class.
 * @details This is the specialized clock domain 'PTP'.
 * @date    2013
 */

#ifndef IASAVBRAWCLOCKDOMAIN_HPP_
#define IASAVBRAWCLOCKDOMAIN_HPP_


#include <time.h>
#include "IasAvbTypes.hpp"
#include "IasAvbClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"


namespace IasMediaTransportAvb {


class IasAvbRawClockDomain : public IasAvbClockDomain
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAvbRawClockDomain();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbRawClockDomain();


  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbRawClockDomain(IasAvbRawClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbRawClockDomain& operator=(IasAvbRawClockDomain const &other);

    // override
    virtual void onGetEventCount();

    // Member variables

    std::string mInstanceName;
    uint64_t mStartTime;
    uint64_t mLastUpdate;
    uint64_t mLastPtp;
    uint64_t mLastRaw;
    std::mutex mRawClockDomainLock;
};




} // namespace IasMediaTransportAvb

#endif /* IASAVBRAWCLOCKDOMAIN_HPP_ */
