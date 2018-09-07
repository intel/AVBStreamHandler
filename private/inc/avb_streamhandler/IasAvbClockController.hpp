/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbClockController.hpp
 * @brief   The definition of the IasAvbClockController class.
 * @details The clock controller class compares clock domains and generates control signal to clock drivers
 * @date    2013
 */

#ifndef IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCLOCKCONTROLLER_HPP
#define IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCLOCKCONTROLLER_HPP

#include "IasAvbTypes.hpp"
#include "IasAvbClockDomain.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_helper/IasSignal.hpp"

#include <ctype.h>


namespace IasMediaTransportAvb {

class IasAvbClockDriverInterface;

class IasAvbClockController : private IasAvbClockDomainClientInterface, private IasMediaTransportAvb::IasIRunnable
{
  public:

    /**
     *  @brief Constructor
     */
    IasAvbClockController();

    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbClockController();

    /**
     * @brief initialize controller
     *
     * @
     */
    IasAvbProcessingResult init(IasAvbClockDomain *master, IasAvbClockDomain *slave, uint32_t driverParam);

    /**
     * @brief release resources and revert to state prior to init()
     */
    void cleanup();

  private:

    static const __useconds_t cWaitMin = 1000u;

    enum LockState
    {
      eInit,
      eUnlocked,
      eLockingRate,
      eLockingPhase,
      eLocked,
      eOff // for debug only
    };

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbClockController(IasAvbClockController const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbClockController& operator=(IasAvbClockController const &other);

    //{@
    /// IasAvbClockDomainClientInterface implementation
    virtual void notifyUpdateRatio(const IasAvbClockDomain * domain);
    virtual void notifyUpdateLockState(const IasAvbClockDomain * domain);
    //@}

    //{@
    /// IasIRunnable implementation
    virtual IasResult beforeRun();
    virtual IasResult run();
    virtual IasResult shutDown();
    virtual IasResult afterRun();
    //@}

    //
    // Helpers
    //
    void setLimits();

    //
    // Members
    //
    IasAvbClockDomain *mMaster;
    IasAvbClockDomain *mSlave;
    IasAvbClockDriverInterface *mClockDriver;
    uint32_t mDriverParam;
    bool mEngage;
    volatile LockState mLockState;
    double mUpperLimit;
    double mLowerLimit;
    IasThread mThread;
    IasSignal mSignal;
    volatile bool mEndThread;
    __useconds_t mWait;
    DltContext *mLog;
};


} // namespace IasMediaTransportAvb

#endif /* IAS_MEDIATRANSPORT_AVBSTREAMHANDLER_AVBCLOCKCONTROLLER_HPP */
