/*
 @COPYRIGHT_TAG@
 */
/**
 * @file
 */

#ifndef IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_INTERFACE_HPP
#define IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_INTERFACE_HPP

#include "avb_watchdog/IasWatchdogResult.hpp"
#include "avb_helper/IasResult.hpp"

#include <dlt/dlt_cpp_extension.hpp>
#include <mutex>
#include <string>

/**
 * @brief IasWatchdog
 */
namespace IasWatchdog {

class IasSystemdWatchdogManager;

/*!
 *  @brief Class to handle watchdog behavior per-thread.
 *
 *  Provides interface for threads to register to the watchdog manager.
 *
 */
class IasWatchdogInterface
{
  public:
    /*!
     *  @brief Constructor
     *  @param [in] DLT context to use for logging errors.
     */
    IasWatchdogInterface(IasSystemdWatchdogManager& watchdogManager, DltContext& context);

    /*!
     *  @brief Destructor.
     */
    virtual ~IasWatchdogInterface();

    /*!
     *  @brief Registers  watchdog.
     *
     *  @param[in] timeout The time in ms following a reset after which the watchdog should fire.
     *  @param[in] watchdogName A string identifying the watchdog.
     *  @returns IasWatchdogResult object indicating success or nature of error.
     */
    virtual IasWatchdogResult registerWatchdog(uint32_t timeout, std::string watchdogName);

    //TODO: Check this brief regardgin IasIWatchdogManager...
    /*!
     *  @brief Registers the watchdog with pre-configured timeout and name.
     *
     *  This call can only be used when WatchdogInterface was created using
     *  IasIWatchdogManager::createWatchdog(Ias::UInt32, std::string) variant.
     *  @returns IasWatchdogResult object indicating success or nature of error.
     */
    virtual IasWatchdogResult registerWatchdog();

    /*!
     *  @brief Unregisters the watchdog.
     *  @returns IasWatchdogResult object indicating success or nature of error.
     */
    virtual IasWatchdogResult unregisterWatchdog();

    /*!
     *  @brief Resets the watchdog timer.
     *
     *  First call to reset will arm the watchdog. After that reset must be called within the timeout given by registerWatchdog.
     *  @returns IasWatchdogResult object indicating success or nature of error.
     */
    virtual IasWatchdogResult reset();

    virtual uint64_t getCurrentRawTime();

    /*!
     * @brief Returns the next reset time for this watchdog.
     */
    uint64_t getDueResetTime() const;

    /*!
     *  @brief Set the timeout period in ms.
     */
    void setTimeout(const uint32_t timeout);

    /*!
     *  @brief Set the name that identifies this watchdog.
     */
    void setName(const std::string& name);

    /*!
     *  @brief Get the timeout period in ms.
     */
    uint32_t  getTimeout() const { return mTimeout; }
    /*!
     *  @brief Get the process ID of the component using the watchdog (fixed on call to registerWatchdog()).
     */
    int32_t getProcessId() const { return mProcessId; }
    /*!
     *  @brief Get the thread ID of the component using the watchdog (fixed on call to registerWatchdog()).
     */
    uint64_t getThreadId() const { return mThreadId; }
    /*!
     *  @brief Get the name that identifies this watchdog.
     */
    std::string getName() const { return mName; }
    /*!
     *  @brief Returns true if the watchdog was registered.
     */
    bool isRegistered() const { return ( mProcessId != 0u ); }


  private:
    /*!
     *  @brief Private copy constructor.
     */
    IasWatchdogInterface(IasWatchdogInterface&) = delete;

    /*!
     *  @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasWatchdogInterface& operator=(IasWatchdogInterface const &other) = delete;

    /*!
     *  @brief Supplied DLT context to use for logging errors.
     */
    DltContext&               mDltContext;

    /*!
     * @brief The watchdog manager object this watchdog object belongs to.
     */
    IasSystemdWatchdogManager &mWatchdogManager;

    /*!
     * @brief  The next reset time for this watchdog.
     */
    uint64_t mDueResetTime;

    /*!
     * @brief mutex to protect access to the mDueResetTime.
     */
    mutable std::mutex mDueResetTimeMutex;

    /*!
     * @brief Flag indicating whether the watchdog was created with preconfigured timeout and name.
     */
    bool mPreconfigured { false };

    /*!
     * @brief Flag indicating whether the timeout of the watchdog was already reported.
     */
    bool mTimeoutReported { false };

    /*!
     *  @brief The timeout period in ms.
     */
    uint32_t    mTimeout;

    /*!
     *  @brief The process ID of the component using the watchdog.
     */
    int32_t     mProcessId;

    /*!
     *  @brief The thread ID of the component using the watchdog.
     */
    uint64_t    mThreadId;

    /*!
     *  @brief A name to identify this watchdog.
     */
    std::string    mName;

    uint64_t mResult_timestamp;
};

} // namespace IasWatchdog


template<>
int32_t logToDlt(DltContextData &log, IasWatchdog::IasWatchdogInterface const &value);

#endif // IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_INTERFACE_HPP
