/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAlsaClockDomain.hpp
 * @brief   The definition of the IasAvbRawClockDomain class.
 * @details This is the specialized clock domain 'ALSA HW Device'. //TODO: check that!
 * @date    2018
 */

#ifndef IASALSACLOCKDOMAIN_HPP_
#define IASALSACLOCKDOMAIN_HPP_


#include <time.h>
#include "IasAvbTypes.hpp"
#include "IasAvbClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"

//ABu: ToDo: This class is not used so far - check if needed and how timing must be calculated - at the moment this is just a copy of RawClockDomain
namespace IasMediaTransportAvb {


class IasAlsaClockDomain : public IasAvbClockDomain
{
  public:
    /**
     *  @brief Constructor.
     */
    IasAlsaClockDomain();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAlsaClockDomain();


  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAlsaClockDomain(IasAlsaClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAlsaClockDomain& operator=(IasAlsaClockDomain const &other);

    // override
    virtual void onGetEventCount();

    // helpers
    static inline uint64_t getRawTime();

    // Member variables

    std::string mInstanceName;
    uint64_t mStartTime;
    uint64_t mLastUpdate;
};

inline uint64_t IasAlsaClockDomain::getRawTime()
{
  struct timespec tp;
  (void) clock_gettime(CLOCK_MONOTONIC_RAW, &tp);   // ABu: ToDo: check that!!!
  return IasLibPtpDaemon::convertTimespecToNs(tp);
}



} // namespace IasMediaTransportAvb

#endif /* IASALSACLOCKDOMAIN_HPP_ */
