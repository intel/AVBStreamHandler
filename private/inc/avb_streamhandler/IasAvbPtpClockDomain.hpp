/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbPtpClockDomain.hpp
 * @brief   The definition of the IasAvbPtpClockDomain class.
 * @details This is the specialized clock domain 'PTP'.
 * @date    2013
 */

#ifndef IASAVBPTPCLOCKDOMAIN_HPP_
#define IASAVBPTPCLOCKDOMAIN_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAvbClockDomain.hpp"


namespace IasMediaTransportAvb {


class /*IAS_DSO_PUBLIC*/ IasAvbPtpClockDomain : public IasAvbClockDomain
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAvbPtpClockDomain();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbPtpClockDomain();


  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbPtpClockDomain(IasAvbPtpClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbPtpClockDomain& operator=(IasAvbPtpClockDomain const &other);

    // override
    virtual void onGetEventCount();

    // Member variables

    std::string mInstanceName;
    uint64_t mStartTime;
    uint64_t mLastUpdate;
};


} // namespace IasMediaTransportAvb

#endif /* IASAVBPTPCLOCKDOMAIN_HPP_ */
