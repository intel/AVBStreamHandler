/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbSwClockDomain.hpp
 * @brief   The definition of the IasAvbSwClockDomain class.
 * @details The software clock domain is used to deal with AVB related
 *          time information if hardware support is not available.
 * @date    2013
 */

#ifndef IASAVBSWCLOCKDOMAIN_HPP_
#define IASAVBSWCLOCKDOMAIN_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAvbClockDomain.hpp"
#include "lib_ptp_daemon/IasLibPtpDaemon.hpp"
#include "avb_streamhandler/IasAvbClockDomain.hpp"


namespace IasMediaTransportAvb {

class /*IAS_DSO_PUBLIC*/ IasAvbSwClockDomain : public IasAvbClockDomain
{
  public:

    /**
     *  @brief Constructor.
     */
    IasAvbSwClockDomain();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbSwClockDomain();

    /**
     * @brief Time acquisition (live mode)
     * Use this method to provide a new time measurement to the clock domain on an "as-it-happens" basis.
     * The method uses the TSC counter internally (for more information, see @ref getTsc()). Call
     * @ref reset() before the first call to advance().
     *
     * @param[in] elapsed     time in nanoseconds that elapsed since the last call to advance()
     */
    inline void advance(uint64_t events, const uint32_t elapsed);

    /**
     * @brief Time acquisition (deferred mode)
     * Use this method to provide a new time measurement to the clock domain.
     * The elapsedTSC parameter should be determined by subtracting the current TSC value
     * from the TSC value acquired before the previous call to advance().
     * Call @ref reset() before the first call to advance().
     *
     * @param[in] elapsed     time in nanoseconds that elapsed since the last call to advance()
     * @param[in] elapsedTSC  difference between the current TSC value and the value of the last advance() call
     */
    virtual void advance(uint64_t events, const uint32_t elapsed, const uint32_t elapsedTSC);

    /**
     * @brief Prepare clock domain for operation
     * Call this function before starting to measure the clock with one of the other methods.
     * The average number of calls per second needs to be known for proper filter coefficient
     * calculation.
     *
     * param[in] avgCallPerSec  expected average number of calls to advance() or updateRelative()per second.
     */
    virtual void reset(uint32_t avgCallsPerSec);

    /**
     * @brief Update rate ratio by specifying the relative error
     * Use this method as an alternative to the advance() methods. Do not use both.
     * Relative error values > 1.0 indicate that the clock runs too slow, values < 1.0 the opposite.
     * Example: relErr=1.05 means the clock needs to run 5% faster.
     * A call to the reset() method is required before using updateRelative in order to
     * specify the average calling rate.
     *
     * param[in] relErr       relative error of rate ratio as described above.
     */
    virtual inline void updateRelative(double relErr);

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbSwClockDomain(IasAvbSwClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbSwClockDomain& operator=(IasAvbSwClockDomain const &other);

    static void calculateConversionCoeffs();

    //
    // Member variables
    //

    std::string            mInstanceName;
    bool              mResetPending;
    uint64_t            mLastTsc;
    IasLibPtpDaemon*  mPtpProxy;
    double           mFactorLong;
    double           mFactorUnlock;
    uint32_t            mThreshold1;
    uint32_t            mThreshold2;
};


inline void IasAvbSwClockDomain::advance(uint64_t events, const uint32_t elapsed)
{
#if IASAVB_USE_TSC
  const uint64_t now = IasLibPtpDaemon::getTsc();
#else
  AVB_ASSERT( NULL != mPtpProxy );
  const uint64_t now = mPtpProxy->getLocalTime();
#endif
  advance(events, elapsed, uint32_t(now - mLastTsc));
  mLastTsc = now;
}


inline void IasAvbSwClockDomain::updateRelative( double relErr )
{
  updateRateRatio( getRateRatio() * relErr );
}


} // namespace IasMediaTransportAvb

#endif /* IASAVBSWCLOCKDOMAIN_HPP_ */
