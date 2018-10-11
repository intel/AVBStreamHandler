/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_SYSTEMD_WATCHDOG_MANAGER_HPP
#define IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_SYSTEMD_WATCHDOG_MANAGER_HPP

#include "avb_watchdog/IasWatchdogTimerRegistration.hpp"

#include <mutex>
#include <list>

/**
 * @brief IasWatchdog
 */
namespace IasWatchdog {


class IasWatchdogInterface;

/*!
 *  @brief Class to manage instances of systemd watchdogs.
 *
 *  The watchdog manager is responsible for creating and managing watchdog instances. systemd has no knowledge of
 *  registering/unregistering watchdogs. This is handled solely by the watchdog manager.
 *
 *  The IasSystemdWatchdogManager reads the watchdog timeout 'WATCHDOG_USEC' environment variable provided by systemd
 *  and registers a high priority timer at the given IPC communication object.
 *  To be on the safe side regarding watchdog resets the watchdog is reset every half of the timeout time.
 *
 *  Every time the timer callback is called the systemd watchdog is reset. This causes the thread processing
 *  the IPC communication object to run
 *  .
 *  If that thread is blocked for more than the timeout provided by systemd, the systemd watchdog will
 *  not be reset any more causing the systemd watchdog to timeout taking the corresponding timeout action.
 *
 */
class IasSystemdWatchdogManager
{
  public:
    /*!
     *  @brief Constructor
     *  @param [in] DLT context to use for logging errors.
     */
    IasSystemdWatchdogManager(DltContext& context);

    /*!
     *  @brief Destructor.
     */
    virtual ~IasSystemdWatchdogManager();

    /*!
     *  @brief Initialises the watchdog manager.
     *  @param [in] timer registration object A pointer to the timer registration object.
     *
     *  @return IasResult indicating success or nature of error.
     */
    IasWatchdogResult init(std::shared_ptr<IasWatchdogTimerRegistration> watchdogTimerRegistration);

    /*!
     *  @brief Frees any resources created by the watchdog manager.
     *
     *  From now on the systemd watchdog will not be reset any more.
     *
     *  @return IasResult indicating success or nature of error.
     */
    IasWatchdogResult cleanup();

    /*!
     * @brief This function can be called at intervals for cases where there is a long running task running on the
     * communicator preventing the ITimerInterface from timing out. This ensures the systemd watchdog is reset.
     */
    void checkResetWatchdog();

    /*!
     * @brief Force the watchdog reset manually.
     */
    void forceResetWatchdog();

    /*!
     *  @copydoc IasIWatchdogManagerInterface::createWatchdog
     */
    virtual IasWatchdogInterface* createWatchdog();

    /*!
     *  @copydoc IasIWatchdogManagerInterface::createWatchdog
     */
    virtual IasWatchdogInterface* createWatchdog(uint32_t timeout, std::string watchdogName);

    /*!
     *  @copydoc IasIWatchdogManagerInterface::destroyWatchdog
     */
    virtual IasWatchdogResult destroyWatchdog(IasWatchdogInterface* watchdog);

    /*!
    *  @brief Removes the watchdog from the list of watchdogs.
    */
    IasWatchdogResult removeWatchdog(IasWatchdogInterface* watchdogInterface);

    virtual uint64_t getCurrentRawTime() const;

    /*!
     *  @brief Call sd_notify() if all watchdog interfaces are valid.
     */
    void notifyTimedOut();

  private:

    /*!
     *  @brief Private copy constructor.
     */
    IasSystemdWatchdogManager(IasSystemdWatchdogManager&) = delete;

    /*!
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasSystemdWatchdogManager& operator=(IasSystemdWatchdogManager const &other) = delete;

    /*!
     * @brief Checks all registered watchdog interfaces to ensure they are valid.
     *
     * Validity of a single interface is true if:
     * - watchdog interface is not yet armed (first reset has not been conducted).
     * - watchdog interface due time is not yet reached.
     */
    bool allWatchdogInterfacesValid() const;

    /*!
     *  @brief enum defining watchdog removal modes.
     */
    enum class IasWatchdogRemoveMode
    {
      eRemoveOnly, //! call came in via removeWatchdog()
      eRemoveAndDestroy //! call came in via destroyWatchdog()
    };

    /*!
     *  @brief Internal function to remove watchdog and, if called by destroyWatchdog, delete it.
     */
    IasWatchdogResult removeWatchdogInternal(IasWatchdogInterface* watchdogInterface,
                                                  IasWatchdogRemoveMode const watchdogRemoveMode);

    /*!
     *  @brief Supplied DLT context to use for logging errors.
     */
    DltContext& mDltContext;

    /*!
     *  @brief The timer registration (to setup the timer).
     */
    std::shared_ptr<IasWatchdogTimerRegistration> mWatchdogTimerRegistration;

    /*!
     * @brief The timer reset time.
     */
    uint64_t mResetTimespan;

    /*!
     * @brief The timestamp  of the next timeout we expect from ITimerInterface.
     */
    uint64_t mNextTimeoutTimestamp;

    /*!
     * @brief The timer object (registers and manages timeouts).
     */
    IasWatchdogTimer mWatchdogTimer;

    /*!
     * @brief mutex to lock the mWatchdogInterfaces object.
     */
    mutable std::recursive_mutex mWatchdogInterfacesMutex;

    /*!
     * @brief List of all registered Watchdogs.
     */
    std::list<IasWatchdogInterface*> mWatchdogInterfaces;

    uint64_t mResult_timestamp;
};

} // namespace IasWatchdog

#endif // IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_SYSTEMD_WATCHDOG_MANAGER_HPP
