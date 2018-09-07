/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockDomain.hpp
 * @brief   The definition of the IasAvbClockDomain class.
 * @details This is the base class for all various clock domains.
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCLOCKDOMAIN_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCLOCKDOMAIN_HPP

#include "IasAvbTypes.hpp"
#include <mutex>
#include <dlt.h>

namespace IasMediaTransportAvb {

class IasAvbClockDomain;

class IasAvbClockDomainClientInterface
{
  public:

    /**
     * @brief indicates an update of the rate ratio
     *
     * @param [in] domain pointer to the clock domain object
     */
    virtual void notifyUpdateRatio(const IasAvbClockDomain * domain) = 0;

    /**
     * @brief indicates a change in lock state
     *
     * @param [in] domain pointer to the clock domain object
     */
    virtual void notifyUpdateLockState(const IasAvbClockDomain * domain) = 0;

  protected:
    //@{
    /// can only be created and destroyed through implementation class
    IasAvbClockDomainClientInterface() {}
    ~IasAvbClockDomainClientInterface() {}
    //@}
};

class IasAvbClockDomain
{
  public:

    /**
     * @brief Represents the current lock state.
     */
    enum IasAvbLockState
    {
      eIasAvbLockStateInit = 0,
      eIasAvbLockStateUnlocked,
      eIasAvbLockStateLocking,
      eIasAvbLockStateLocked,
    };

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbClockDomain();

    /**
     * @brief returns the current lock state.
     * The rate ratio should be considered reliable
     * when lock state equal eIasAvbLockStateLocked.
     */
    inline IasAvbLockState getLockState() const;

    /**
     * @brief returns the filtered rate ratio.
     * The rate ratio is the relationship between any local
     * clock (e.g. an audio sample clock) and the 802.1AS (gPTP)
     * time and is defined as follows:
     *
     * rateRatio = elapsedTime802.1ASClock / elapsedTimeLocalClock
     *
     * i.e. a local clock running slower than the ptp clock has a value > 1.
     *
     * Ideally, the ratio should deviate from 1.0 only by some ppm.
     */
    inline double getRateRatio() const;

    /**
     * @brief returns absolute event count
     *
     * The event counter can be used to track the absolute number of
     * events (e.g. audio samples) over time that have been used to
     * calculate the rate ratio. Comparing the event counters of two
     * clock domains can be used for long-time drift compensation.
     * The timeStamp parameter will be filled with the time stamp
     * corresponding to the last update of the event counter.
     */
    inline uint64_t getEventCount(uint64_t & timeStamp);

    /**
     * @brief returns event rate
     *
     * The event rate represents the number of events per second
     * (e.g. audio sample frequency). This information is used
     * to adapt the control loop calculations in the ALSA worker
     * threads. This is needed since master clock might be different
     * from the actual sample frequency of the streams handled by the
     * worker thread.
     */
    inline uint32_t getEventRate();

    /**
     * @brief get clock domain type
     *
     * used to cast down to derived class safely
     */
    inline IasAvbClockDomainType getType() const;

    /**
     * @brief set drift compensation value
     *
     * specify a drift compensation value in ppm that is applied to the current
     * rate ratio value.
     *
     * @param [in] val compensation value in ppm
     * @returns eIasAvbProcOK on success
     * @returns eIasAvbProcInvalidParam if val is out of range
     */
    IasAvbProcessingResult setDriftCompensation(int32_t val);

    /**
     * @brief register client for event callbacks
     *
     * Only one client can be registered
     *
     * @param [in] pointer to object implementing the client
     * @returns eIasAvbProcOK on success
     * @returns eIasAvbProcAlreadyInUse if another client is already registered
     * @returns eIasAvbProcInvalidParam if client is NULL
     */
    IasAvbProcessingResult registerClient(IasAvbClockDomainClientInterface *client);

    /**
     * @brief unregister client for event callbacks
     *
     * @param [in] pointer to object implementing the client
     * @returns eIasAvbProcOK on success
     * @returns eIasAvbProcInvalidParam if client is not equal to the client pointer used at registration
     */
    IasAvbProcessingResult unregisterClient(IasAvbClockDomainClientInterface *client);

    /**
     * @brief Sets a flag indicating that a reset is needed
     *
     * This method can be used to set a flag which can be queried by objects that need to know if a
     * clock domain reset is needed. E.g. the clock controller detects a epoch change in PTP time
     * it can set this flag. The AVB audio stream can detect it and reset the clock domain. It is the one
     * which is aware of the parameter that needs to be passed to the reset method (presentation time stamp).
     */
    inline void setResetRequest();

    /**
     * @brief Returns the reset request flag.
     *
     * If this method returns 'true' the clock domain's reset function has to be called.
     *
     * @returns 'true' if a request is required, otherwise 'false'.
     */
    inline bool getResetRequest();

    /**
     * @brief Getter method for the instance's clock ID.
     *
     * The initialised clock id value is set to '-1' as a debug hint.
     *
     * @returns Instance clock Id
     */
    inline uint32_t getClockDomainId() const { return mClockId; }

    /**
     * @brief Setter method for the instance's clock ID.
     *
     * @param [in] clock id value to be set
     */
    inline void setClockDomainId(uint32_t id) { mClockId = id; }

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbClockDomain(IasAvbClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbClockDomain& operator=(IasAvbClockDomain const &other);

  protected:

    /**
     *  @brief Constructor, to be called by derived class only.
     */
    IasAvbClockDomain(DltContext &dltContext, IasAvbClockDomainType type);

    /*
     * @brief Update rate ratio with latest acquired value
     * This method should be called by the derived class
     * every time a new current rate ratio has been measured.
     * The average number of calls per second to this method
     * should be considered in the time constant calculation.
     *
     * @param[in] newRatio the currently measured rate ratio
     */
    void updateRateRatio(double newRatio);

    /**
     * @brief Supply initial value for faster lock-in
     * The initial value can be specified on startup of the clock
     * domain before the first call to updateRateRatio.
     *
     * @param[in] initVal initial rate ratio estimate value
     */
    void setInitialValue(double initVal);

    /**
     * @brief Set time constant of internal filters
     * This class uses two first-order butterworth low pass filters
     * to produce two kinds of moving average values for the rate ratio.
     * The value following the currently measured value more quickly is
     * the one which can be accessed by clients through getRateRatio().
     * The slower-moving one is used internally for lock state validation
     * and its time constant is derived automatically from the faster one.
     * The time constant can be calculated by tc=T*fs where T is the
     * theoretical first-order filter time constant in seconds and fs
     * is the average number of calls per second to updateRateRatio().
     *
     * @param[in] timeConstant the normalized time constant as described above
     * param[in] avgCallPerSec  expected average number of calls to updateRateRatio() per second.
     */
    void setFilter(double timeConstant, uint32_t avgCallsPerSec);

    /**
     * @brief Lock state change notification
     *
     * Derived classes can override this method to implement
     * specific reactions upon lock staten changes. Use getLockState()
     * to query the current lock state from within the method. One
     * Example is to dynamically adjust the smoothing time constant depending
     * on the lock state by calling setFilter(). See also @ref setDerivationFactors.
     */
    virtual void lockStateChanged();

    /**
     * @brief Set event counter to new absolute value.
     */
    inline void setEventCount(uint64_t newValue, uint64_t timestamp);

    /**
     * @brief Set event rate that represents the events per second.
     */
    inline void setEventRate(uint32_t eventRate);

    /**
     * @brief Increment event counter by number of events.
     */
    inline void incrementEventCount(uint64_t increment, uint64_t timestamp);

    //
    // fine-tuning (experts only)
    //

    /**
     *  @brief Sets factor to derive slow time constant from fast one
     *  Use this function to change the way the smoothing time constant
     *  is dynamically changed during unlocks or which multiple of the time
     *  constant is used for long term value surveillance (i.e. unlock detection)
     *  By default, the factors are set to 1.0 (i.e. no change during unlock, long term
     *  surveillance off). The long term factor should be set in any case (e.g. to 10.0), the
     *  unlock factor is optional; Alternatively, call setFilter from the lockStateChanged()
     *  implementation in the derived class.
     */
    void setDerivationFactors(double factorLongTerm, double factorUnlock = 1.0);

    /// @brief Set unlock threshold1 (200ppm equals 0.2%)
    void setLockThreshold1(uint32_t ppm);

    /// @brief Set unlock threshold1 (200000ppm equals 20%)
    void setLockThreshold2(uint32_t ppm);

    inline double getTimeConstant() const;

    /// override when derived class needs to do something (rarely needed)
    virtual void onGetEventCount() {}

  private:
    //
    // Helpers
    //
    static double calculateCoefficient(double timeConstant);
    inline void smooth(double & stateBuf, double newVal, double coeff);

    //
    // Members
    //
    const IasAvbClockDomainType mType;
    double mTimeConstant;
    uint32_t  mAvgCallsPerSec;
    double mRateRatio;
    double mCompensation;
    uint64_t  mEventCount;
    uint32_t  mEventRate;
    uint64_t  mEventTimeStamp;
    double mRateRatioSlow;
    double mRateRatioFast;
    double mCoeffSlowLocked;
    double mCoeffSlowUnlocked;
    double mCoeffFastLocked;
    double mCoeffFastUnlocked;
    double mThresholdSlowLow;
    double mThresholdSlowHigh;
    double mThresholdFastLow;
    double mThresholdFastHigh;
    double mInitialValue;
    double mDerivationFactorUnlock;
    double mDerivationFactorLongTerm;
    IasAvbLockState mLockState;
    mutable std::mutex mLock;
    uint32_t  mDebugCount;
    uint32_t  mDebugUnlockCount;
    double mDebugLockedPercentage;
    double mDebugMinRatio;
    double mDebugMaxRatio;
    uint32_t  mDebugOver;
    uint32_t  mDebugUnder;
    uint32_t  mDebugIn;
    IasAvbClockDomainClientInterface *mClient;
    uint32_t  mDebugLogInterval;
    volatile bool mResetRequest;
    uint32_t  mClockId;
  protected:
    DltContext *mLog;

};

inline IasAvbClockDomainType IasAvbClockDomain::getType() const
{
  return mType;
}

inline double IasAvbClockDomain::getRateRatio() const
{
  return mRateRatio;
}

inline IasAvbClockDomain::IasAvbLockState IasAvbClockDomain::getLockState() const
{
  return mLockState;
}

inline void IasAvbClockDomain::smooth(double & stateBuf, double newVal, double coeff)
{
  stateBuf = (coeff * stateBuf) + ((1.0 - coeff) * newVal);
}

inline double IasAvbClockDomain::getTimeConstant() const
{
  return mTimeConstant;
}

inline void IasAvbClockDomain::setEventCount(uint64_t newValue, uint64_t timestamp)
{
  mLock.lock();
  mEventCount = newValue;
  mEventTimeStamp = timestamp;
  mLock.unlock();
}

inline void IasAvbClockDomain::setEventRate(uint32_t eventRate)
{
  AVB_ASSERT(0u != eventRate);
  mEventRate = eventRate;
}

inline void IasAvbClockDomain::incrementEventCount(uint64_t increment, uint64_t timestamp)
{
  mLock.lock();
  mEventCount += increment;
  mEventTimeStamp = timestamp;
  mLock.unlock();
}

inline uint64_t IasAvbClockDomain::getEventCount(uint64_t & timeStamp)
{
  onGetEventCount();
  uint64_t ret = 0u;
  mLock.lock();
  ret = mEventCount;
  timeStamp = mEventTimeStamp;
  mLock.unlock();
  return ret;
}

inline uint32_t IasAvbClockDomain::getEventRate()
{
  // could possibly be '0'!
  return mEventRate;
}

inline void IasAvbClockDomain::setResetRequest()
{
  mResetRequest = true;
}

inline bool IasAvbClockDomain::getResetRequest()
{
  bool ret = mResetRequest;
  mResetRequest = false;
  return ret;
}

} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCLOCKDOMAIN_HPP */
